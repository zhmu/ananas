%{
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
extern FILE *archin;

int archerror(char *s)
{
	fprintf(stderr, "%s\n", s);
}

int archwrap()	
{
	return 1;
}
%}
%token <string> FILENAME VALUE 
%token MANDATORY OPTIONAL OPTION 

%type<string> MANDATORY OPTIONAL OPTION 

/* override default int type */
%union {
	char *string;
}

%type<string> required line

%%
lines:	/* empty */
	|	lines line	{ printf("%s\n", $2); free($2); }
	;

line: FILENAME required 		{ asprintf(&$$,"%s %s", $1,$2); }
	|	FILENAME required VALUE	{ asprintf(&$$,"%s %s %s", $1,$2,$3);  } 
	;

required:
		MANDATORY	{$$=strdup("mandatory");}
	|	OPTIONAL		{$$=strdup("optional");}
	|	OPTION		{$$=strdup("option");}
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
		archin = file;
	}

	archparse();

	fclose(file);

	if (argc > 2) {
		printf("opening second file: %s\n", argv[2]);
		if ( (file=fopen(argv[2], "r'")) == NULL)	{
			fprintf(stderr,"Failed to open file: %s",argv[1]);
			exit(1);
		}
		archrestart(file); 
		archparse();	
	}
	
	return 0;
}

