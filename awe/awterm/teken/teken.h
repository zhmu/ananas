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
 * $FreeBSD: head/sys/teken/teken.h 199175 2009-11-11 09:43:26Z ed $
 */

#ifndef _TEKEN_H_
#define	_TEKEN_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * libteken: terminal emulation library.
 *
 * This library converts an UTF-8 stream of bytes to terminal drawing
 * commands.
 */

typedef uint32_t teken_char_t;
typedef unsigned short teken_unit_t;
typedef unsigned char teken_format_t;
#define	TF_BOLD		0x01
#define	TF_UNDERLINE	0x02
#define	TF_BLINK	0x04
#define	TF_REVERSE	0x08
typedef unsigned char teken_color_t;
#define	TC_BLACK	0
#define	TC_RED		1
#define	TC_GREEN	2
#define	TC_BROWN	3
#define	TC_BLUE		4
#define	TC_MAGENTA	5
#define	TC_CYAN		6
#define	TC_WHITE	7
#define	TC_NCOLORS	8

typedef struct {
	teken_unit_t	tp_row;
	teken_unit_t	tp_col;
} teken_pos_t;
typedef struct {
	teken_pos_t	tr_begin;
	teken_pos_t	tr_end;
} teken_rect_t;
typedef struct {
	teken_format_t	ta_format;
	teken_color_t	ta_fgcolor;
	teken_color_t	ta_bgcolor;
} teken_attr_t;
typedef struct {
	teken_unit_t	ts_begin;
	teken_unit_t	ts_end;
} teken_span_t;

typedef struct __teken teken_t;

typedef void teken_state_t(teken_t *, teken_char_t);

/*
 * Drawing routines supplied by the user.
 */

typedef void tf_bell_t(void *);
typedef void tf_cursor_t(void *, const teken_pos_t *);
typedef void tf_putchar_t(void *, const teken_pos_t *, teken_char_t,
    const teken_attr_t *);
typedef void tf_fill_t(void *, const teken_rect_t *, teken_char_t,
    const teken_attr_t *);
typedef void tf_copy_t(void *, const teken_rect_t *, const teken_pos_t *);
typedef void tf_param_t(void *, int, unsigned int);
#define	TP_SHOWCURSOR	0
#define	TP_KEYPADAPP	1
#define	TP_AUTOREPEAT	2
#define	TP_SWITCHVT	3
#define	TP_132COLS	4
#define	TP_SETBELLPD	5
#define	TP_SETBELLPD_PITCH(pd)		((pd) >> 16)
#define	TP_SETBELLPD_DURATION(pd)	((pd) & 0xffff)
#define	TP_MOUSE	6
typedef void tf_respond_t(void *, const void *, size_t);

typedef struct {
	tf_bell_t	*tf_bell;
	tf_cursor_t	*tf_cursor;
	tf_putchar_t	*tf_putchar;
	tf_fill_t	*tf_fill;
	tf_copy_t	*tf_copy;
	tf_param_t	*tf_param;
	tf_respond_t	*tf_respond;
} teken_funcs_t;

typedef teken_char_t teken_scs_t(teken_t *, teken_char_t);

/*
 * Terminal state.
 */

struct __teken {
	const teken_funcs_t *t_funcs;
	void		*t_softc;

	teken_state_t	*t_nextstate;
	unsigned int	 t_stateflags;

#define T_NUMSIZE	8
	unsigned int	 t_nums[T_NUMSIZE];
	unsigned int	 t_curnum;

	teken_pos_t	 t_cursor;
	teken_attr_t	 t_curattr;
	teken_pos_t	 t_saved_cursor;
	teken_attr_t	 t_saved_curattr;

	teken_attr_t	 t_defattr;
	teken_pos_t	 t_winsize;

	/* For DECSTBM. */
	teken_span_t	 t_scrollreg;
	/* For DECOM. */
	teken_span_t	 t_originreg;

#define	T_NUMCOL	160
	unsigned int	 t_tabstops[T_NUMCOL / (sizeof(unsigned int) * 8)];

	unsigned int	 t_utf8_left;
	teken_char_t	 t_utf8_partial;

	unsigned int	 t_curscs;
	teken_scs_t	*t_saved_curscs;
	teken_scs_t	*t_scs[2];
};

/* Initialize teken structure. */
void	teken_init(teken_t *, const teken_funcs_t *, void *);

/* Deliver character input. */
void	teken_input(teken_t *, const void *, size_t);

/* Get/set teken attributes. */
const teken_pos_t *teken_get_cursor(teken_t *);
const teken_attr_t *teken_get_curattr(teken_t *);
const teken_attr_t *teken_get_defattr(teken_t *);
void	teken_get_defattr_cons25(teken_t *, int *, int *);
const teken_pos_t *teken_get_winsize(teken_t *);
void	teken_set_cursor(teken_t *, const teken_pos_t *);
void	teken_set_curattr(teken_t *, const teken_attr_t *);
void	teken_set_defattr(teken_t *, const teken_attr_t *);
void	teken_set_winsize(teken_t *, const teken_pos_t *);

/* Key input escape sequences. */
#define	TKEY_UP		0x00
#define	TKEY_DOWN	0x01
#define	TKEY_LEFT	0x02
#define	TKEY_RIGHT	0x03

#define	TKEY_HOME	0x04
#define	TKEY_END	0x05
#define	TKEY_INSERT	0x06
#define	TKEY_DELETE	0x07
#define	TKEY_PAGE_UP	0x08
#define	TKEY_PAGE_DOWN	0x09

#define	TKEY_F1		0x0a
#define	TKEY_F2		0x0b
#define	TKEY_F3		0x0c
#define	TKEY_F4		0x0d
#define	TKEY_F5		0x0e
#define	TKEY_F6		0x0f
#define	TKEY_F7		0x10
#define	TKEY_F8		0x11
#define	TKEY_F9		0x12
#define	TKEY_F10	0x13
#define	TKEY_F11	0x14
#define	TKEY_F12	0x15
const char *teken_get_sequence(teken_t *, unsigned int);

/* Legacy features. */
void	teken_set_8bit(teken_t *);
void	teken_set_cons25(teken_t *);

/* Color conversion. */
teken_color_t teken_256to8(teken_color_t);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* !_TEKEN_H_ */
