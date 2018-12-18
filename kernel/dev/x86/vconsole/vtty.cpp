#include "kernel/driver.h"
#include "vconsole.h"
#include "vtty.h"
#include "ivideo.h"

namespace
{
    void term_bell(void* s)
    {
        // Nothing
    }

    void term_cursor(void* s, const teken_pos_t* p)
    {
        auto& vtty = *static_cast<VTTY*>(s);
        if (vtty.IsActive())
            vtty.GetVideo().SetCursor(p->tp_col, p->tp_row);
    }

    void term_putchar(void* s, const teken_pos_t* p, teken_char_t c, const teken_attr_t* a)
    {
        auto& vtty = *static_cast<VTTY*>(s);
        auto& pixel = vtty.PixelAt(p->tp_col, p->tp_row);
        pixel = IVideo::Pixel(c, *a);
        if (vtty.IsActive())
            vtty.GetVideo().PutPixel(p->tp_col, p->tp_row, pixel);
    }

    void term_fill(void* s, const teken_rect_t* r, teken_char_t c, const teken_attr_t* a)
    {
        teken_pos_t p;

        // Braindead implementation of fill() - just call putchar()
        for (p.tp_row = r->tr_begin.tp_row; p.tp_row < r->tr_end.tp_row; p.tp_row++)
            for (p.tp_col = r->tr_begin.tp_col; p.tp_col < r->tr_end.tp_col; p.tp_col++)
                term_putchar(s, &p, c, a);
    }

    void term_copy(void* s, const teken_rect_t* r, const teken_pos_t* p)
    {
        auto place_pixel = [](void* s, const teken_pos_t& d) {
            auto& vtty = *static_cast<VTTY*>(s);
            if (vtty.IsActive())
                vtty.GetVideo().PutPixel(d.tp_col, d.tp_row, vtty.PixelAt(d.tp_col, d.tp_row));
        };

        /*
         * copying is a little tricky. we must make sure we do it in
         * correct order, to make sure we don't overwrite our own data.
         */
        int nrow = r->tr_end.tp_row - r->tr_begin.tp_row;
        int ncol = r->tr_end.tp_col - r->tr_begin.tp_col;
        auto& vtty = *static_cast<VTTY*>(s);

        if (p->tp_row < r->tr_begin.tp_row) {
            /* copy from top to bottom. */
            if (p->tp_col < r->tr_begin.tp_col) {
                /* copy from left to right. */
                for (int y = 0; y < nrow; y++) {
                    teken_pos_t d;
                    d.tp_row = p->tp_row + y;
                    for (int x = 0; x < ncol; x++) {
                        d.tp_col = p->tp_col + x;
                        vtty.PixelAt(d.tp_col, d.tp_row) =
                            vtty.PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
                        place_pixel(s, d);
                    }
                }
            } else {
                /* copy from right to left. */
                for (int y = 0; y < nrow; y++) {
                    teken_pos_t d;
                    d.tp_row = p->tp_row + y;
                    for (int x = ncol - 1; x >= 0; x--) {
                        d.tp_col = p->tp_col + x;
                        vtty.PixelAt(d.tp_col, d.tp_row) =
                            vtty.PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
                        place_pixel(s, d);
                    }
                }
            }
        } else {
            /* copy from bottom to top. */
            if (p->tp_col < r->tr_begin.tp_col) {
                /* copy from left to right. */
                for (int y = nrow - 1; y >= 0; y--) {
                    teken_pos_t d;
                    d.tp_row = p->tp_row + y;
                    for (int x = 0; x < ncol; x++) {
                        d.tp_col = p->tp_col + x;
                        vtty.PixelAt(d.tp_col, d.tp_row) =
                            vtty.PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
                        place_pixel(s, d);
                    }
                }
            } else {
                /* copy from right to left. */
                for (int y = nrow - 1; y >= 0; y--) {
                    teken_pos_t d;
                    d.tp_row = p->tp_row + y;
                    for (int x = ncol - 1; x >= 0; x--) {
                        d.tp_col = p->tp_col + x;
                        vtty.PixelAt(d.tp_col, d.tp_row) =
                            vtty.PixelAt(r->tr_begin.tp_col + x, r->tr_begin.tp_row + y);
                        place_pixel(s, d);
                    }
                }
            }
        }
    }

    void term_param(void* s, int cmd, unsigned int value)
    {
        // TODO
    }

    void term_respond(void* s, const void* buf, size_t len)
    {
        // TODO
    }

    teken_funcs_t tf = {.tf_bell = term_bell,
                        .tf_cursor = term_cursor,
                        .tf_putchar = term_putchar,
                        .tf_fill = term_fill,
                        .tf_copy = term_copy,
                        .tf_param = term_param,
                        .tf_respond = term_respond};

} // unnamed namespace

VTTY::VTTY(const CreateDeviceProperties& cdp)
    : TTY(cdp), v_video(static_cast<VConsole*>(cdp.cdp_Parent)->GetVideo())
{
}

Result VTTY::Attach()
{
    teken_pos_t tp;
    tp.tp_row = v_video.GetHeight();
    tp.tp_col = v_video.GetWidth();
    teken_init(&v_teken, &tf, this);
    teken_set_winsize(&v_teken, &tp);

    v_buffer = new IVideo::Pixel[v_video.GetHeight() * v_video.GetWidth()];
    return Result::Success();
}

Result VTTY::Detach()
{
    delete[] v_buffer;
    return Result::Success();
}

Result VTTY::Print(const char* buffer, size_t len)
{
    MutexGuard g(v_mutex);
    teken_input(&v_teken, buffer, len);
    return Result::Success();
}

void VTTY::Activate()
{
    if (v_active)
        return;
    v_active = true;

    // Restore screen contents - XXX This can be done much more efficient
    for (int y = 0; y < v_video.GetHeight(); y++)
        for (int x = 0; x < v_video.GetWidth(); x++)
            v_video.PutPixel(x, y, PixelAt(x, y));
}

void VTTY::Deactivate()
{
    if (!v_active)
        return;
    v_active = false;
}

namespace
{
    struct VTTY_Driver : public Driver {
        VTTY_Driver() : Driver("vtty") {}

        const char* GetBussesToProbeOn() const override
        {
            // We are created directly by vconsole
            return nullptr;
        }

        Device* CreateDevice(const CreateDeviceProperties& cdp) override { return new VTTY(cdp); }
    };

    const RegisterDriver<VTTY_Driver> registerDriver;

} // unnamed namespace

/* vim:set ts=2 sw=2: */
