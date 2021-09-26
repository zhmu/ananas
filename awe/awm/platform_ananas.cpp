#include "platform_ananas.h"
#include <ananas/syscalls.h>
#include <ananas/x86/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include "pixelbuffer.h"

struct Platform_Ananas::Impl {
    int fd{};
    Size size{};
    using PixelValue = uint32_t;
    PixelValue* fb{};

    Impl()
    {
        fd = open("/dev/vtty0", O_RDWR);
        if (fd < 0) perror("open");

        struct ananas_fb_info fbi{};
        fbi.fb_size = sizeof(fbi);
        if (ioctl(fd, TIOFB_GET_INFO, &fbi) < 0) throw std::runtime_error("cannot obtain fb info");

        size.height = fbi.fb_height;
        size.width = fbi.fb_width;
        if (fbi.fb_bpp != sizeof(PixelValue) * 8) throw std::runtime_error("unsupported fb bpp");

        fb = static_cast<PixelValue*>(mmap(NULL, size.width * size.height * sizeof(PixelValue), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        if (fb == static_cast<PixelValue*>(MAP_FAILED)) throw std::runtime_error("cannot map fb");
    }

    ~Impl()
    {
        munmap(fb, size.width * size.height * sizeof(PixelValue));
        close(fd);
    }

    void Render(PixelBuffer& pb)
    {
        std::memcpy(fb, pb.buffer, size.height * size.width * sizeof(PixelValue));
    }

    std::optional<Event> Poll()
    {
        // TODO
        return {};
    }
};

Platform_Ananas::Platform_Ananas() : impl(std::make_unique<Impl>()) {}

Platform_Ananas::~Platform_Ananas() = default;

void Platform_Ananas::Render(PixelBuffer& fb) { return impl->Render(fb); }

std::optional<Event> Platform_Ananas::Poll() { return impl->Poll(); }

Size Platform_Ananas::GetSize() { return impl->size; }
