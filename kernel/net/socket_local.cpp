#include <ananas/errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "kernel/lib.h"
#include "kernel/net/socket_local.h"
#include "kernel/condvar.h"
#include "kernel/fd.h"
#include "kernel/init.h"
#include "kernel/process.h"

namespace net
{
    constexpr inline auto SocketBufferSize = 1024;

    enum class State
    {
        Idle,
        Bound,
        Listening,
        Connected,
        Closed
    };

    struct LocalSocket;

    namespace
    {
        Mutex mutexLocalSockets{"localSockets"};
        util::List<LocalSocket> allLocalSockets;
    }

    struct LocalSocket : util::List<LocalSocket>::NodePtr
    {
        Mutex ls_mutex{"local"};
        State ls_state{State::Idle};
        refcount_t ls_refcount{1};
        ConditionVariable ls_cv_event{"local"};
        LocalSocket* ls_endpoint{};
        util::vector<LocalSocket*> ls_pending;
        char ls_path[UNIX_PATH_MAX] = { };
        util::array<uint8_t, SocketBufferSize> ls_buffer{};
        size_t ls_buffer_writepos{};
        size_t ls_buffer_readpos{};

        LocalSocket()
        {
            MutexGuard g(mutexLocalSockets);
            allLocalSockets.push_back(*this);
        }

        ~LocalSocket()
        {
            KASSERT(ls_refcount == 0, "destroying with %d refs", ls_refcount);
            if (ls_endpoint != nullptr)
                ls_endpoint->Deref();

            MutexGuard g(mutexLocalSockets);
            allLocalSockets.remove(*this);
        }

        void Ref()
        {
            ++ls_refcount;
        }

        void Deref()
        {
            if (--ls_refcount > 0) return;
            delete this;
        }

        size_t Write(const void* buf, size_t len)
        {
            auto in = static_cast<const uint8_t*>(buf);
            size_t n = 0;
            for (; len > 0; --len) {
                ls_buffer[ls_buffer_writepos] = *in;
                ls_buffer_writepos = (ls_buffer_writepos + 1) % SocketBufferSize;
                ++in, ++n;
            }
            return n;
        }

        size_t Read(void* buf, size_t len)
        {
            auto out = static_cast<uint8_t*>(buf);
            size_t n = 0;
            for (; len > 0; --len) {
                if (ls_buffer_readpos == ls_buffer_writepos) break;
                *out = ls_buffer[ls_buffer_readpos];
                ls_buffer_readpos = (ls_buffer_readpos + 1) % SocketBufferSize;
                ++out, ++n;
            }
            return n;
        }
    };

    namespace {
        Result GetLocalSocket(FD& fd, LocalSocket*& out)
        {
            if (fd.fd_type != FD_TYPE_SOCKET)
                return Result::Failure(EBADF);

            out = fd.fd_data.d_local_socket;
            return Result::Success();
        }

        LocalSocket* FindSocketByAddress(const sockaddr_un* sun)
        {
            MutexGuard g(mutexLocalSockets);
            for(auto& ls: allLocalSockets) {
                if (strcmp(ls.ls_path, sun->sun_path) == 0) return &ls;
            }
            return nullptr;
        }

        Result local_read(fdindex_t index, FD& fd, void* buf, size_t len)
        {
            // TODO block if there's no data
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            //if (ls->ls_state != State::Connected)
            //return Result::Success(0);

            MutexGuard g(ls->ls_mutex);
            auto n = ls->Read(buf, len);
            //kprintf("local_read: pid %d fd %d ls %p -> %d bytes\n", process::GetCurrent().p_pid, index, ls, n);
            ls->ls_cv_event.Signal();
            return Result::Success(n);
        }

        Result local_write(fdindex_t index, FD& fd, const void* buf, size_t len)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            if (ls->ls_state != State::Connected)
                return Result::Failure(EINVAL);

            auto ep = ls->ls_endpoint;
            MutexGuard g(ep->ls_mutex);

            auto n = ep->Write(buf, len);
            //kprintf("local_write: pid %d fd %d ep %p -> %d bytes\n", process::GetCurrent().p_pid, index, ep, n);
            ls->ls_cv_event.Signal();
            return Result::Success(n);
        }

        Result local_open(fdindex_t index, FD& fd, const char* path, int flags, int mode)
        {
            return Result::Failure(EINVAL);
        }

        Result local_free(Process& proc, FD& fd)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            {
                MutexGuard g(ls->ls_mutex);
                if (auto ep = ls->ls_endpoint; ep != nullptr) {
                    MutexGuard g(ep->ls_mutex);
                    ep->ls_state = State::Closed;
                    ep->ls_cv_event.Broadcast();
                }
                ls->ls_state = State::Closed;
                ls->ls_cv_event.Broadcast();
            }
            ls->Deref();
            //kprintf("local_free pid %d\n", process::GetCurrent().p_pid);
            return Result::Success();
        }

        Result local_unlink(fdindex_t index, FD& fd)
        {
            return Result::Failure(EINVAL);
        }

        Result local_clone(Process& proc_in, fdindex_t index, FD& fd_in, struct CLONE_OPTIONS* opts, Process& proc_out, FD*& fd_out, fdindex_t index_out_min, fdindex_t& index_out)
        {
            fd_in.fd_data.d_local_socket->Ref();
            return fd::CloneGeneric(fd_in, proc_out, fd_out, index_out_min, index_out);
        }

        Result local_ioctl(fdindex_t index, FD& fd, unsigned long request, void* args[])
        {
            return Result::Failure(EINVAL);
        }

        Result local_accept(fdindex_t, FD& fd, struct sockaddr*, socklen_t*)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            if (ls->ls_state != State::Listening)
                return Result::Failure(EINVAL);

            LocalSocket* next;
            ls->ls_mutex.Lock();
            while(true) {
                if (!ls->ls_pending.empty()) {
                    // TODO use a proper queue container
                    next = ls->ls_pending.front();
                    ls->ls_pending.erase(ls->ls_pending.begin());
                    ls->ls_mutex.Unlock();
                    break;
                }

                // TODO check for signals
                ls->ls_cv_event.Wait(ls->ls_mutex);
            }

            // Create a new FD connected to next
            auto& proc = process::GetCurrent();
            FD* fd_new;
            fdindex_t index_out;
            if (auto result = fd::Allocate(FD_TYPE_SOCKET, proc, 0, fd_new, index_out); result.IsFailure()) {
                MutexGuard g(ls->ls_mutex);
                ls->ls_pending.push_back(next); // XXX should be at the front
                return result;
            }

            // Note, this transfers the ref from the pending list to localSocket
            auto localSocket = new LocalSocket;
            localSocket->ls_state = State::Connected;
            localSocket->ls_endpoint = next;
            fd_new->fd_data.d_local_socket = localSocket;

            // Update the retrieved pending socket to refer to us
            {
                MutexGuard g(next->ls_mutex);
                next->ls_state = State::Connected;
                next->ls_endpoint = localSocket;
                next->ls_cv_event.Broadcast();
            }

            //kprintf("local_accept: ls %p, next %p (endpoint %p) --> newsocket %p (endpoint %p) index %d\n", ls, next, next->ls_endpoint, localSocket, localSocket->ls_endpoint, index_out);
            return Result::Success(index_out);
        }

        Result local_bind(fdindex_t, FD& fd, const struct sockaddr* addr, socklen_t socklen)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            if (ls->ls_state != State::Idle)
                return Result::Failure(EINVAL);

            if (addr->sa_family != AF_LOCAL || socklen != sizeof(struct sockaddr_un))
                return Result::Failure(EINVAL);

            auto sun = reinterpret_cast<const sockaddr_un*>(addr);
            if (auto socket = FindSocketByAddress(sun); socket != nullptr)
                return Result::Failure(EADDRINUSE);

            memcpy(ls->ls_path, sun->sun_path, UNIX_PATH_MAX);
            ls->ls_state = State::Bound;
            return Result::Success();
        }

        Result local_connect(fdindex_t, FD& fd, const struct sockaddr* addr, socklen_t socklen)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            if (ls->ls_state != State::Idle)
                return Result::Failure(EINVAL);

            if (addr->sa_family != AF_LOCAL || socklen != sizeof(struct sockaddr_un))
                return Result::Failure(EINVAL);

            auto sun = reinterpret_cast<const sockaddr_un*>(addr);
            auto socket = FindSocketByAddress(sun);
            if (socket == nullptr || socket->ls_state != State::Listening)
                return Result::Failure(ECONNREFUSED);

            ls->ls_mutex.Lock();
            {
                ls->Ref();
                MutexGuard g(socket->ls_mutex);
                socket->ls_pending.push_back(ls);
                socket->ls_cv_event.Broadcast();
            }

            ls->ls_cv_event.Wait(ls->ls_mutex);
            KASSERT(ls->ls_state == State::Connected, "unexpected state %d", ls->ls_state);
            KASSERT(ls->ls_endpoint != socket, "wrong endpoint");
            ls->ls_mutex.Unlock();
            //kprintf("local_connect: ls %p -> endpoint %p\n", ls, ls->ls_endpoint);
            return Result::Success();
        }

        Result local_listen(fdindex_t, FD& fd, int)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            if (ls->ls_state != State::Bound)
                return Result::Failure(EINVAL);

            ls->ls_state = State::Listening;
            return Result::Success();
        }

        Result local_send(fdindex_t, FD& fd, const void*, size_t, int)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return result;

            kprintf("TODO send\n");
            return Result::Failure(EINVAL);
        }

        bool local_can_read(fdindex_t, FD& fd)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return false;

            if (!ls->ls_pending.empty()) return true;
            if (ls->ls_state == State::Closed) return true;

            return ls->ls_buffer_writepos != ls->ls_buffer_readpos;
        }

        bool local_can_write(fdindex_t, FD& fd)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return false;

            if (ls->ls_state == State::Closed) return true;
            return false;
        }

        bool local_has_except(fdindex_t, FD& fd)
        {
            LocalSocket* ls;
            if (auto result = GetLocalSocket(fd, ls); result.IsFailure())
                return false;
            return ls->ls_state == State::Closed;
        }

        FDOperations local_fdops = {
            .d_read = local_read,
            .d_write = local_write,
            .d_open = local_open,
            .d_free = local_free,
            .d_unlink = local_unlink,
            .d_clone = local_clone,
            .d_ioctl = local_ioctl,
            .d_accept = local_accept,
            .d_bind = local_bind,
            .d_connect = local_connect,
            .d_listen = local_listen,
            .d_send = local_send,
            .d_can_read = local_can_read,
            .d_can_write = local_can_write,
            .d_has_except = local_has_except
        };

        const init::OnInit registerFDType(init::SubSystem::Handle, init::Order::Second, []() {
            static FDType ft("local", FD_TYPE_SOCKET, local_fdops);
            fd::RegisterType(ft);
        });
    }

    Result AllocateLocalSocket()
    {
        auto& proc = process::GetCurrent();

        /* Obtain a new handle */
        FD* fd;
        fdindex_t index_out;
        if (auto result = fd::Allocate(FD_TYPE_SOCKET, proc, 0, fd, index_out); result.IsFailure())
            return result;

        auto localSocket = new LocalSocket;
        fd->fd_data.d_local_socket = localSocket;

        return Result::Success(index_out);
    }
}
