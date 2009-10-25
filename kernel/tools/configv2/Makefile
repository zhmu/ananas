#LEX = flex -I
#YACC = yacc

parser: tokenizer.o
	$(CC) -o parser grammar.c tokenizer.c -ly -ll -lm

grammer.c: 
	$(YACC) -d -o grammar.c grammar.y

grammar.h: grammer.c

tokenizer.c: grammar.h
	$(LEX) -o tokenizer.c tokenizer.l 

clean:
	rm *.h *.c *.o parser
