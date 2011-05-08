#include <assert.h>
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

	/* memset: 4 bytes aligned */
	unsigned int dummy = (unsigned int)-1;
	kmemset(&dummy, 0, sizeof(dummy));
	assert(dummy == 0);

	/* memset: 16 bytes aligned */
	unsigned char buf[17] = { 0 };
	kmemset(buf, 0xcd, 16);
	for (int i = 0; i < 16; i++)
		assert(buf[i] == 0xcd);
	assert(buf[16] == 0);

	/* memset: 3 bytes aligned */
	kmemset(&buf, 0xab, 3);
	assert(buf[0] == 0xab);
	assert(buf[1] == 0xab);
	assert(buf[2] == 0xab);
	assert(buf[3] == 0xcd);

	/* memset: 3 bytes unaligned */
	kmemset(&buf[1], 0xaa, 3);
	assert(buf[0] == 0xab);
	assert(buf[1] == 0xaa);
	assert(buf[2] == 0xaa);
	assert(buf[3] == 0xaa);
	assert(buf[4] == 0xcd);

	/* memcpy: 4 bytes aligned */
	unsigned int dummy2 = (unsigned int)-1;
	kmemcpy(&dummy2, &dummy, sizeof(dummy));
	assert(dummy2 == dummy);

	/* memcpy: 16 bytes aligned */
	unsigned char tmp[17] = {0};
	kmemcpy(tmp, buf, 16);
	for (int i = 0; i < 16; i++)
		assert(tmp[i] == buf[i]);

	/* memcpy: 3 bytes aligned */
	kmemset(buf, 0x12, 3);
	kmemcpy(tmp, buf, 3);
	assert(tmp[0] == 0x12);
	assert(tmp[1] == 0x12);
	assert(tmp[2] == 0x12);
	assert(tmp[3] == 0xaa);

	/* memcpy: 7 bytes unaligned */
	kmemset(buf, 0x24, 16);
	kmemcpy(&tmp[1], &buf[2], 7);
	assert(tmp[0] == 0x12);
	assert(tmp[1] == 0x24);
	assert(tmp[2] == 0x24);
	assert(tmp[3] == 0x24);
	assert(tmp[4] == 0x24);
	assert(tmp[5] == 0x24);
	assert(tmp[6] == 0x24);
	assert(tmp[7] == 0x24);
	assert(tmp[8] == 0xcd);

	return 0;
}

/* vim:set ts=2 sw=2: */
