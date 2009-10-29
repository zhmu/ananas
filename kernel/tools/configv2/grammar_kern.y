%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern FILE *kernin;

int kernerror(char *s)
{
	fprintf(stderr, "%s\n", s);
}

int kernwrap()	
{
	return 1;
}
%}
%token <string> VALUE 
%token ARCH IDENT HINTS DEVICE OPTION CONSOLE

/* override default int type */
%union {
	char *string;
}

%type<string> subject line

%%
lines:	/* empty */
	|	lines line	{ printf("%s\n", $2); free($2); }
	;

line: subject  		{ asprintf(&$$,"%s %s", $1); }
	|	subject VALUE	{ asprintf(&$$,"%s %s", $1,$2); }
	;

subject:
		ARCH 			{$$=strdup("arch");}
	|	IDENT			{$$=strdup("ident");}
	|	HINTS			{$$=strdup("hints");}
	|	DEVICE		{$$=strdup("device");}
	|	OPTION		{$$=strdup("option");}
	|	CONSOLE		{$$=strdup("console");}
	;
	
%%

int main(int argc, char* argv[])
{
	FILE *file;

	if (argc > 1)	{
		printf("opening file: %s\n", argv[1]);
		if ( (file = fopen(argv[1], "r")) == NULL)	{
			fprintf(stderr,"Failed to open file: %s",argv[1]);
			exit(1);
		}
		kernin = file;
	}

	kernparse();
	fclose(file);
	return 0;
}

