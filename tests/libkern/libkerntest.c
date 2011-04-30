#include <stdio.h>
#include <stdlib.h>
#include <ananas/lib.h>

const char* console_getbuf();
const char* console_reset();

#define KPRINTF_VERIFY(result) \
	(strcmp(console_getbuf(), result)) ? fprintf(stderr, "failure in line %d: result '%s' != expected '%s'\n", __LINE__, console_getbuf(), result), abort() : console_reset();

#define KPRINTF_CHECK(result, expr, ...) \
	kprintf(expr, ## __VA_ARGS__); \
	KPRINTF_VERIFY(result)

int
main(int argc, char* argv[])
{
	/* Basic framework sanity check: fixed charachters, appending */
	KPRINTF_CHECK("test", "test");
	kprintf("a"); kprintf("b");
	KPRINTF_VERIFY("ab");
	kprintf("foo"); kprintf("bar");
	KPRINTF_VERIFY("foobar");

	/* Strings */
	KPRINTF_CHECK("foo", "%s", "foo");
	KPRINTF_CHECK("foobar", "%s%s", "foo", "bar");
	KPRINTF_CHECK("foo-bar", "%s-%s", "foo", "bar");
	KPRINTF_CHECK("<foo-bar>", "<%s-%s>", "foo", "bar");
	KPRINTF_CHECK("(null)", "%s", NULL); /* Ananas extension */ 

	/* Characters */
	KPRINTF_CHECK("a", "%c", 'a');
	KPRINTF_CHECK("abc", "%c%c%c", 'a', 'b', 'c');

	/* Basic integer formatting */
	KPRINTF_CHECK("1", "%i", 1);
	KPRINTF_CHECK("1234", "%i", 1234);
	KPRINTF_CHECK("foo1bar", "foo%ibar", 1);
	KPRINTF_CHECK("foo1bar", "foo%ibar", 1);
	KPRINTF_CHECK("-1", "%i", -1);
	KPRINTF_CHECK("-12345678", "%d", -12345678);

	/* Hex integers */
	KPRINTF_CHECK("1", "%x", 1);
	KPRINTF_CHECK("1234", "%x", 0x1234);
	KPRINTF_CHECK("badf00d", "%x", 0xbadf00d);
	KPRINTF_CHECK("BADF00D", "%X", 0xBADF00D);

	/* Octal integers */
	KPRINTF_CHECK("1234567", "%o", 01234567);
	KPRINTF_CHECK("0", "%o", 00);

	/* Pointers; we only check 32 bits */
	KPRINTF_CHECK("1", "%p", (void*)1);
	KPRINTF_CHECK("1234", "%p", (void*)0x1234);
	KPRINTF_CHECK("f00d1234", "%p", (void*)0xf00d1234);

	/* Padding */
	KPRINTF_CHECK(" 1", "% 2x", 1);
	KPRINTF_CHECK("01", "%02x", 1);
	KPRINTF_CHECK(" 123", "% 4i", 123);
	KPRINTF_CHECK("1234", "% 4i", 1234);
	KPRINTF_CHECK("12345", "% 4i", 12345);
	KPRINTF_CHECK("   1.0002", "% 4i.%04i", 1, 2);
	KPRINTF_CHECK(" foo", "%4s", "foo");

	return 0;
}

/* vim:set ts=2 sw=2: */
