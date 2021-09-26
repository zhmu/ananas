/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <ed@FreeBSD.ORG> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day,
 * and you think this stuff is worth it, you can buy me a beer in return
 * Ed Schouten
 * ----------------------------------------------------------------------------
 *
 * You are encouraged (but not required) to send patches, suggestions
 * and improvements to the author.
 */

/*
 * This file was imported as
 * $FreeBSD: head/sys/teken/teken_subr_compat.h 197522 2009-09-26 15:26:32Z ed $
 */

static void
teken_subr_cons25_set_cursor_type(teken_t *t, unsigned int type)
{

	teken_funcs_param(t, TP_SHOWCURSOR, type != 1);
}

static const teken_color_t cons25_colors[8] = { TC_BLACK, TC_BLUE,
    TC_GREEN, TC_CYAN, TC_RED, TC_MAGENTA, TC_BROWN, TC_WHITE };

static void
teken_subr_cons25_set_adapter_background(teken_t *t, unsigned int c)
{

	t->t_defattr.ta_bgcolor = cons25_colors[c % 8];
	t->t_curattr.ta_bgcolor = cons25_colors[c % 8];
}

static void
teken_subr_cons25_set_adapter_foreground(teken_t *t, unsigned int c)
{

	t->t_defattr.ta_fgcolor = cons25_colors[c % 8];
	t->t_curattr.ta_fgcolor = cons25_colors[c % 8];
	if (c >= 8) {
		t->t_defattr.ta_format |= TF_BOLD;
		t->t_curattr.ta_format |= TF_BOLD;
	} else {
		t->t_defattr.ta_format &= ~TF_BOLD;
		t->t_curattr.ta_format &= ~TF_BOLD;
	}
}

static const teken_color_t cons25_revcolors[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

void
teken_get_defattr_cons25(teken_t *t, int *fg, int *bg)
{

	*fg = cons25_revcolors[teken_256to8(t->t_defattr.ta_fgcolor)];
	if (t->t_defattr.ta_format & TF_BOLD)
		*fg += 8;
	*bg = cons25_revcolors[teken_256to8(t->t_defattr.ta_bgcolor)];
}

static void
teken_subr_cons25_switch_virtual_terminal(teken_t *t, unsigned int vt)
{

	teken_funcs_param(t, TP_SWITCHVT, vt);
}

static void
teken_subr_cons25_set_bell_pitch_duration(teken_t *t, unsigned int pitch,
    unsigned int duration)
{

	teken_funcs_param(t, TP_SETBELLPD, (pitch << 16) |
	    (duration & 0xffff));
}

static void
teken_subr_cons25_set_terminal_mode(teken_t *t, unsigned int mode)
{

	switch (mode) {
	case 0:	/* Switch terminal to xterm. */
		t->t_stateflags &= ~TS_CONS25;
		break;
	case 1: /* Switch terminal to cons25. */
		t->t_stateflags |= TS_CONS25;
		break;
	}
}

#if 0
static void
teken_subr_vt52_decid(teken_t *t)
{
	const char response[] = "\x1B/Z";

	teken_funcs_respond(t, response, sizeof response - 1);
}
#endif
