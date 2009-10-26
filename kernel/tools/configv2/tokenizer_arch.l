%{
#include <string.h>
#include "grammar_arch.h"  /* import tokens from yacc */

extern YYSTYPE archlval;
%}
%%
[\t ]+		/* ignore whitespace */ ;
[\n\0]+		/* ignore empty lines */ ;
^#.*			/* ignore comments */ ;
.*\.(c|S) 	{ archlval.string = strdup(archtext);return FILENAME; }
mandatory	{ return MANDATORY; }
optional		{ return OPTIONAL; }
option		{ return OPTION; }
[a-zA-Z]+	{ archlval.string = strdup(archtext);return VALUE; }
%%
