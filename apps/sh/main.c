#include <types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char** argv)
{
	uint8_t ch;

	printf("hi this is sh!\n");

	char* a = malloc(1234);
	char* b = malloc(4321);
	char* c = malloc(100);
	printf("a = %x\n", a);
	printf("b = %x\n", b);
	printf("c = %x\n", c);
	free(b);
	char* d = malloc(10);
	printf("d = %x\n", d);
	a[1000] = '!';
	while (1) {
		if (!read(1, &ch, 1))
			continue;
		printf("%c", ch);
	
	}
	return 0;
}
