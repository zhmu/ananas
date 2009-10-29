#LEX = flex -I
#YACC = yacc

all: parser_arch parser_kern

parser_kern: grammar_kern.c tokenizer_kern.c
	$(CC) -o parser_kern grammar_kern.c tokenizer_kern.c -ly -ll -lm

parser_arch: grammar_arch.c tokenizer_arch.c
	$(CC) -o parser_arch grammar_arch.c tokenizer_arch.c -ly -ll -lm

grammar_arch.c: grammar_arch.y
	$(YACC) -p arch -d -o grammar_arch.c grammar_arch.y

grammar_kern.c: grammar_kern.y
	$(YACC) -p kern -d -o grammar_kern.c grammar_kern.y

grammar_arch.h: grammar_arch.c

grammar_kern.h: grammar_kern.c

tokenizer_arch.c: grammar_arch.h
	$(LEX) -P arch -o tokenizer_arch.c tokenizer_arch.l 

tokenizer_kern.c: grammar_kern.h
	$(LEX) -P kern -o tokenizer_kern.c tokenizer_kern.l 

clean:
	rm -f *.h *.c *.output parser_arch parser_kern
