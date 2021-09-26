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
 * $FreeBSD: head/sys/teken/teken_scs.h 203659 2010-02-08 09:16:59Z ed $
 */

static inline teken_char_t
teken_scs_process(teken_t *t, teken_char_t c)
{

	return (t->t_scs[t->t_curscs](t, c));
}

/* Unicode points for VT100 box drawing. */
static const uint16_t teken_boxdrawing_unicode[31] = {
    0x25c6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1,
    0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c, 0x23ba,
    0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534, 0x252c,
    0x2502, 0x2264, 0x2265, 0x03c0, 0x2260, 0x00a3, 0x00b7
};

/* ASCII points for VT100 box drawing. */
static const uint8_t teken_boxdrawing_8bit[31] = {
    '?', '?', 'H', 'F', 'C', 'L', '?', '?',
    'N', 'V', '+', '+', '+', '+', '+', '-',
    '-', '-', '-', '-', '+', '+', '+', '+',
    '|', '?', '?', '?', '?', '?', '?',
};

static teken_char_t
teken_scs_special_graphics(teken_t *t, teken_char_t c)
{

	/* Box drawing. */
	if (c >= '`' && c <= '~')
		return (t->t_stateflags & TS_8BIT ?
		    teken_boxdrawing_8bit[c - '`'] :
		    teken_boxdrawing_unicode[c - '`']);
	return (c);
}

static teken_char_t
teken_scs_uk_national(teken_t *t, teken_char_t c)
{

	/* Pound sign. */
	if (c == '#')
		return (t->t_stateflags & TS_8BIT ? 0x9c : 0xa3);
	return (c);
}

static teken_char_t
teken_scs_us_ascii(teken_t *t __unused, teken_char_t c)
{

	/* No processing. */
	return (c);
}
