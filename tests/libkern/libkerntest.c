#include <assert.h>
#include <ananas/lib.h>

const char* console_getbuf_reset();

#define KPRINTF_VERIFY(result) \
	assert(strcmp(console_getbuf_reset(), result) == 0)

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

	/* Basic integer formatting */
	KPRINTF_CHECK("1", "%i", 1);
	KPRINTF_CHECK("1234", "%i", 1234);
	KPRINTF_CHECK("foo1bar", "foo%ibar", 1);
	KPRINTF_CHECK("foo1bar", "foo%ibar", 1);

	/* Hex integers */
	KPRINTF_CHECK("1", "%x", 1);
	KPRINTF_CHECK("1234", "%x", 0x1234);
	KPRINTF_CHECK("badf00d", "%x", 0xbadf00d);
	KPRINTF_CHECK("BADF00D", "%X", 0xBADF00D);

	/* Pointers; we only check 32 bits */
	KPRINTF_CHECK("1", "%p", (void*)1);
	KPRINTF_CHECK("1234", "%p", (void*)0x1234);
	KPRINTF_CHECK("f00d1234", "%p", (void*)0xf00d1234);

	/* Formatting */
	KPRINTF_CHECK("01", "%02x", 1);
	KPRINTF_CHECK(" 1", "% 2x", 1);

	return 0;
}

/* vim:set ts=2 sw=2: */
