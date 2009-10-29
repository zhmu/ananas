%{
#include <string.h>
#include "grammar_kern.h"  /* import tokens from yacc */

extern YYSTYPE kernlval;
%}
%%
[\t ]+		/* ignore whitespace */ ;
[\n\0]+		/* ignore empty lines */ ;
^#.*			/* ignore comments */ ;
arch				{ return ARCH; }
ident				{ return IDENT; }
hints				{ return HINTS; }
device			{ return DEVICE; }
option			{ return OPTION; }
console			{ return CONSOLE; }
[a-zA-Z0-9]+	{ kernlval.string = strdup(kerntext);return VALUE; }
%%
