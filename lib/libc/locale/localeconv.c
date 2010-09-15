#include <locale.h>

/* XXX this is hardcoded for Europe */
static struct lconv current_locale = {
	.decimal_point		= ",",
	.thousands_sep		= ".",
	.grouping		= "3",
	.int_curr_symbol	= "EURe",
	.currency_symbol	= "EUR",
	.mon_decimal_point	= ",",
	.mon_thousands_sep	= ".",
	.mon_grouping		= "3",
	.positive_sign		= "+",
	.negative_sign		= "-",
	.int_frac_digits	= 2,
	.frac_digits		= 2,
	.int_p_cs_precedes	= 1,
	.int_p_sep_by_space	= 1,
	.int_n_cs_precedes	= 1,
	.int_n_sep_by_space	= 1,
	.int_p_sign_posn	= 0,
	.int_n_sign_posn	= 0,
	.n_sep_by_space		= 0,
	.p_sep_by_space		= 0,
	.p_sign_posn		= 0,
	.n_sign_posn		= 0,
	.n_cs_precedes		= 1,
	.p_cs_precedes		= 1
};

struct lconv*
localeconv()
{
	return &current_locale;
}
