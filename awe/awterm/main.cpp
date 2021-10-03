#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <optional>

#include <sys/time.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include "ipc.h"
#include "font.h"
#include "pixelbuffer.h"

#include "teken.h"

#ifdef __Ananas__
# define FONT_PATH "/usr/share/fonts"
#else
# define FONT_PATH "../../awm/data"
#endif

namespace
{
    namespace consts {
        constexpr inline auto TerminalWidth = 80;
        constexpr inline auto TerminalHeight = 24;

        constexpr inline auto FontSize = 18;

        const Colour s_BackgroundColour{ 0, 0, 0 };
        const Colour s_CursorColour{ 255, 255, 255 };
        const Colour s_Colour{ 0, 255, 0 };
    }
    constexpr inline teken_unit_t TekenNoValue = std::numeric_limits<teken_unit_t>::max();

    bool quit{};
    int s_CharWidth;
    int s_CharHeight;

    struct Terminal
    {
        PixelBuffer& pixelBuffer;
        font::Font& font;
        struct Pixel {
            teken_char_t c;
            teken_attr_t a;
        };
        std::array<Pixel, consts::TerminalHeight * consts::TerminalWidth> pixels;
        teken_pos_t cursor{ TekenNoValue, TekenNoValue };

        auto& PixelAt(int x, int y) { return pixels[y * consts::TerminalWidth + x]; }

        void DrawCursor()
        {
            const auto x = cursor.tp_col * s_CharWidth;
            const auto y = cursor.tp_row * s_CharHeight;
            const Point pt{ x, y };
            pixelBuffer.FilledRectangle({ pt, { s_CharWidth, s_CharHeight } }, consts::s_CursorColour);
        }

        void Bell()
        {
        }

        void Cursor(const teken_pos_t* p)
        {
            if (cursor.tp_col == p->tp_col && cursor.tp_row == p->tp_row) return;

            if (cursor.tp_col < consts::TerminalWidth && cursor.tp_row < consts::TerminalHeight) {
                auto& pixel = PixelAt(cursor.tp_col, cursor.tp_row);
                PutChar(&cursor, pixel.c, &pixel.a);
            }
            cursor = *p;
            if (cursor.tp_col < consts::TerminalWidth && cursor.tp_row < consts::TerminalHeight) {
                DrawCursor();
            }
        }

        void PutChar(const teken_pos_t* p, const teken_char_t c, const teken_attr_t* a)
        {
            PixelAt(p->tp_col, p->tp_row) = { c, *a };

            char ch = c; // XXX
            std::string_view sv{ &ch, 1 };
            const auto x = p->tp_col * s_CharWidth;
            const auto y = p->tp_row * s_CharHeight;
            const Point pt{ x, y };
            pixelBuffer.FilledRectangle({ pt, { s_CharWidth, s_CharHeight } }, consts::s_BackgroundColour);
            font::DrawText(pixelBuffer, font, pt, consts::s_Colour, sv);
        }

        void Fill(const teken_rect_t* r, teken_char_t c, const teken_attr_t* a)
        {
            teken_pos_t p;
            for (p.tp_row = r->tr_begin.tp_row; p.tp_row < r->tr_end.tp_row; p.tp_row++)
                for (p.tp_col = r->tr_begin.tp_col; p.tp_col < r->tr_end.tp_col; p.tp_col++)
                    PutChar(&p, c, a);
        }

        void Copy(const teken_rect_t* r, const teken_pos_t* p)
        {
            auto copy_pixel = [&](const int deltaX, const int deltaY) {
                const teken_pos_t dst{
                    static_cast<teken_unit_t>(p->tp_row + deltaY),
                    static_cast<teken_unit_t>(p->tp_col + deltaX) };
                auto& pixel = PixelAt(dst.tp_col, dst.tp_row);
                pixel = PixelAt(r->tr_begin.tp_col + deltaX, r->tr_begin.tp_row + deltaY);
                PutChar(&dst, pixel.c, &pixel.a);
            };

            /*
             * copying is a little tricky. we must make sure we do it in
             * correct order, to make sure we don't overwrite our own data.
             */
            const auto nrow = r->tr_end.tp_row - r->tr_begin.tp_row;
            const auto ncol = r->tr_end.tp_col - r->tr_begin.tp_col;
            if (p->tp_row < r->tr_begin.tp_row) {
                // Copy from top to bottom
                if (p->tp_col < r->tr_begin.tp_col) {
                    // Copy from left to right
                    for (int y = 0; y < nrow; y++) {
                        for (int x = 0; x < ncol; x++) {
                            copy_pixel(x, y);
                        }
                    }
                } else {
                    // Copy from right to left
                    for (int y = 0; y < nrow; y++) {
                        for (int x = ncol - 1; x >= 0; x--) {
                            copy_pixel(x, y);
                        }
                    }
                }
            } else {
                // Copy from bottom to top
                if (p->tp_col < r->tr_begin.tp_col) {
                    // Copy from left to right
                    for (int y = nrow - 1; y >= 0; y--) {
                        for (int x = 0; x < ncol; x++) {
                            copy_pixel(x, y);
                        }
                    }
                } else {
                    // Copy from right to left
                    for (int y = nrow - 1; y >= 0; y--) {
                        for (int x = ncol - 1; x >= 0; x--) {
                            copy_pixel(x, y);
                        }
                    }
                }
            }
        }

        void Param(int cmd, unsigned int value)
        {
        }

        void Respond(const void* buf, size_t len)
        {
        }
    };

    teken_funcs_t tekenFuncs = {
        .tf_bell = [](auto t, auto... a) { static_cast<Terminal*>(t)->Bell(a...); },
        .tf_cursor = [](auto t, auto... a) { static_cast<Terminal*>(t)->Cursor(a...); },
        .tf_putchar = [](auto t, auto... a) { static_cast<Terminal*>(t)->PutChar(a...); },
        .tf_fill = [](auto t, auto... a) { static_cast<Terminal*>(t)->Fill(a...); },
        .tf_copy = [](auto t, auto... a) { static_cast<Terminal*>(t)->Copy(a...); },
        .tf_param = [](auto t, auto... a) { static_cast<Terminal*>(t)->Param(a...); },
        .tf_respond = [](auto t, auto... a) { static_cast<Terminal*>(t)->Respond(a...); },
    };

    int s_AweFD = -1;
    ipc::Handle s_Handle;
    uint32_t* s_Framebuffer;
    std::unique_ptr<font::Font> s_Font;
    std::unique_ptr<PixelBuffer> s_PixelBuffer;
    teken_t s_Teken;
    std::unique_ptr<Terminal> s_Terminal;
    int s_MasterFD;

    bool awm_init()
    {
        auto& space = s_Font->GetGlyph(' ');
        s_CharWidth = space.advance;
        s_CharHeight = consts::FontSize;

        const auto windowWidth = consts::TerminalWidth * s_CharWidth;
        const auto windowHeight = consts::TerminalHeight * s_CharHeight;

        s_AweFD = socket(AF_UNIX, SOCK_STREAM, 0);
        if (s_AweFD < 0) {
            fprintf(stderr, "socket: %d\n", errno);
            return false;
        }

        sockaddr_un sun{};
        sun.sun_family = AF_UNIX;
        strcpy(sun.sun_path, "/tmp/awewm");
        if (connect(s_AweFD, reinterpret_cast<sockaddr*>(&sun), sizeof(sun)) < 0) {
            fprintf(stderr, "connect: %d\n", errno);
            return false;
        }

        ipc::Message msgCreateWindow{ ipc::MessageType::CreateWindow };
        msgCreateWindow.u.createWindow.height = windowHeight;
        msgCreateWindow.u.createWindow.width = windowWidth;
        strcpy(msgCreateWindow.u.createWindow.title, "window");
        if (write(s_AweFD, &msgCreateWindow, sizeof(msgCreateWindow)) != sizeof(msgCreateWindow)) {
            fprintf(stderr, "write: %d\n", errno);
            return false;
        }

        ipc::CreateWindowReply reply;
        if (read(s_AweFD, &reply, sizeof(reply)) != sizeof(reply)) {
            fprintf(stderr, "read: %d\n", errno);
            return false;
        }

        if (reply.result != ipc::ResultCode::Success) {
            fprintf(stderr, "CreateWindow: %d", static_cast<int>(reply.result));
            return false;
        }
        s_Handle = reply.handle;

        s_Framebuffer = static_cast<uint32_t*>(shmat(reply.fbKey, NULL, 0));
        if (s_Framebuffer == reinterpret_cast<void*>(-1)) {
            fprintf(stderr, "shmat: %d\n", errno);
            return false;
        }
        s_PixelBuffer = std::make_unique<PixelBuffer>(s_Framebuffer, Size{ windowWidth, windowHeight});

        return true;
    }

    bool awm_set_title(const char* title)
    {
        ipc::Message msgSetWindowTitle{ ipc::MessageType::SetWindowTitle };
        msgSetWindowTitle.u.setWindowTitle.handle = s_Handle;
        strncpy(msgSetWindowTitle.u.setWindowTitle.title, title, sizeof(msgSetWindowTitle.u.setWindowTitle.title) - 1);
        if (write(s_AweFD, &msgSetWindowTitle, sizeof(msgSetWindowTitle)) != sizeof(msgSetWindowTitle)) {
            fprintf(stderr, "write: %d\n", errno);
            return false;
        }

        ipc::GenericReply reply;
        if (read(s_AweFD, &reply, sizeof(reply)) != sizeof(reply)) {
            fprintf(stderr, "read: %d\n", errno);
            return false;
        }
        return true;
    }

    bool awm_update()
    {
        ipc::Message msgUpdateWindow{ ipc::MessageType::UpdateWindow };
        msgUpdateWindow.u.updateWindow.handle = s_Handle;
        if (write(s_AweFD, &msgUpdateWindow, sizeof(msgUpdateWindow)) != sizeof(msgUpdateWindow)) {
            fprintf(stderr, "write: %d\n", errno);
            return false;
        }

        ipc::GenericReply reply;
        if (read(s_AweFD, &reply, sizeof(reply)) != sizeof(reply)) {
            fprintf(stderr, "read: %d\n", errno);
            return false;
        }
        return true;
    }

    bool tty_init()
    {
        s_MasterFD = posix_openpt(O_RDWR | O_NOCTTY);
        if (s_MasterFD < 0) {
            fprintf(stderr, "posix_openpt: %d\n", errno);
            return false;
        }
        if (grantpt(s_MasterFD) < 0) {
            fprintf(stderr, "grantpt: %d\n", errno);
            return false;
        }
        return true;
    }

    bool font_init()
    {
        try {
            s_Font = std::make_unique<font::Font>(FONT_PATH "/RobotoMono-Regular.ttf", consts::FontSize);
            return true;
        } catch(std::exception& e) {
            fprintf(stderr, "exception: %s\n", e.what());
            return false;
        }
    }

    bool term_init()
    {
        s_Terminal = std::make_unique<Terminal>(*s_PixelBuffer, *s_Font);

        teken_init(&s_Teken, &tekenFuncs, s_Terminal.get());
        teken_pos_t tp;
        tp.tp_row = consts::TerminalHeight;
        tp.tp_col = consts::TerminalWidth;
        teken_set_winsize(&s_Teken, &tp);
        return true;
    }

    std::optional<ipc::Event> ReadEvent()
    {
        ipc::Event event;
        int left = sizeof(event);
        auto p = reinterpret_cast<char*>(&event);
        while(left > 0) {
            auto n = read(s_AweFD, p, left);
            if (n <= 0) return {};
            p += n;
            left -= n;
        }
        return event;
    }

    void run()
    {
        while(!quit) {
            fd_set fds;
            int nfds = 0;
            auto add_fd = [&](int fd) {
              FD_SET(fd, &fds);
              nfds = std::max(nfds, fd + 1);
            };

            FD_ZERO(&fds);
            add_fd(s_MasterFD);
            add_fd(s_AweFD);
            int n = select(nfds, &fds, nullptr, nullptr, nullptr);
            if (n <= 0) break;

            if (FD_ISSET(s_MasterFD, &fds)) {
                // Master has data for us, rely to the slave
                char buf[512];
                int len = read(s_MasterFD, buf, sizeof(buf));
                if (len <= 0) break;

                teken_input(&s_Teken, buf, len);
                awm_update();
            }

            if (FD_ISSET(s_AweFD, &fds)) {
                if (auto event = ReadEvent(); event) {
                    switch(event->type) {
                        case ipc::EventType::KeyDown: {
                            const char ch = event->u.keyDown.ch;
                            if (ch != 0) write(s_MasterFD, &ch, 1);
                            break;
                        }
                    }
                }
            }
        }
    }

    int child()
    {
        auto s = setsid();
        setpgid(0, 0);

        if (unlockpt(s_MasterFD) < 0) {
            fprintf(stderr, "unlockpt: %d\n", errno);
            return -1;
        }

        char* devname = ptsname(s_MasterFD);
        int fd = open(devname, O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "open(%s): %d\n", devname, errno);
            return -1;
        }
        close(s_MasterFD);

        char prog[] = "/bin/sh";
        char* const argv[] = { prog, NULL };

        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        return execv(prog, argv);
    }

    void sigint(int)
    {
        quit = true;
    }
}

int main()
{
    if (!tty_init()) return -1;
    int p = fork();
    if (p == 0) return child();

    if (!font_init()) return -1;
    if (!awm_init()) return -1;
    if (!term_init()) return -1;
    awm_set_title("awterm");

    awm_update();

    signal(SIGINT, sigint);
    run();

    kill(p, SIGKILL);
    waitpid(p, NULL, 0);

    return 0;
}
