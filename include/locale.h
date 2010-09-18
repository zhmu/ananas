#ifndef __LOCALE_H__
#define __LOCALE_H__

#include <ananas/_types/null.h>

struct lconv {
	char*	currency_symbol;
	char*	decimal_point;
	char	frac_digits;
	char*	grouping;
	char*	int_curr_symbol;
	char	int_frac_digits;
	char	int_n_cs_precedes;
	char	int_n_sep_by_space;
	char	int_n_sign_posn;
	char	int_p_cs_precedes;
	char	int_p_sep_by_space;
	char	int_p_sign_posn;
	char*	mon_decimal_point;
	char*	mon_grouping;
	char*	mon_thousands_sep;
	char*	negative_sign;
	char	n_cs_precedes;
	char	n_sep_by_space;
	char	n_sign_posn;
	char*	positive_sign;
	char	p_cs_precedes;
	char	p_sep_by_space;
	char	p_sign_posn;
	char*	thousands_sep;
};

#define LC_COLLATE	0x01
#define LC_CTYPE	0x02	
#define LC_MONETARY	0x04
#define LC_NUMERIC	0x08
#define LC_TIME		0x10
#define LC_MESSAGES	0x20
#define LC_ALL		(LC_COLLATE | LC_CTYPE | LC_MONETARY | LC_NUMERIC | LC_TIME | LC_MESSAGES)

struct lconv* localeconv();
char* setlocale(int category, const char* locale);

#endif /* __LOCALE_H__ */
