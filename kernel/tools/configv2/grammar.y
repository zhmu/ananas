%{
#include <stdlib.h>
#include <stdio.h>
extern FILE *yyin;
%}
%token <string> FILENAME VALUE 
%token MANDATORY OPTIONAL OPTION EOL

/* override default int type */
%union {
	char *string;
}

%%
line: FILENAME MANDATORY 		{ printf("%s is a mandatory file.\n", $1); }
	|	FILENAME OPTIONAL 		{ printf("%s is a optional file.\n", $1); } 
	|	FILENAME OPTION VALUE	{ printf("%s is a option file: %s.\n", $1,$3); }
	;
%%

int main(int argc, char* argv[])
{

	if (argc > 1)	{
		printf("opening file: %s\n", argv[1]);
		FILE *file = fopen(argv[1], "r");
		if (!file)	{
			fprintf(stderr,"Failed to open file: %s",argv[1]);
			exit(1);
		}
		yyin = file;
	}

	yyparse();	
	return 0;
}
