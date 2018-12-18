#include <stdio.h>

long int ftell(FILE* stream) { return (long int)ftello(stream); }
