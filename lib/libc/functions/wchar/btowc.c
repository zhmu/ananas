#include <wchar.h>

wint_t btowc(int c)
{
	wchar_t w;
	char ch = (char)c;
	if (mbrtowc(&w, &ch, 1, NULL) > WEOF)
		return WEOF;
	return w;
}
