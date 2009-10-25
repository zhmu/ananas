%{
#include <string.h>
#include "grammar.h"  /* import tokens from yacc */

extern YYSTYPE yylval;
%}
%%
[\t ]+		/* ignore whitespace */ ;
[\n\0]+		/* ignore empty lines */ ;
^#.*			/* ignore comments */ ;
.*\.(c|S) 	{ yylval.string = strdup(yytext);return FILENAME; }
mandatory	{ return MANDATORY; }
optional		{ return OPTIONAL; }
option		{ return OPTION; }
[a-zA-Z]+	{ yylval.string = strdup(yytext);return VALUE; }
%%
