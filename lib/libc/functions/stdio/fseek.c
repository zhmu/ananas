#include <stdio.h>

int fseek(FILE* stream, long loffset, int whence) { return fseeko(stream, (off_t)loffset, whence); }
