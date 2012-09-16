#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ananas/cbuffer.h>
#include "test-framework.h"

CBUFFER_DEFINE(test_buf);

#define TEST_BUF_SIZE 64

void
cbuffer_test()
{
	char* buf_storage = malloc(TEST_BUF_SIZE);
	assert(buf_storage != NULL);

	struct test_buf cb;
	CBUFFER_INIT(&cb, buf_storage, TEST_BUF_SIZE);

	/* Initially, buffer must be empty */
	EXPECT(CBUFFER_EMPTY(&cb));
	EXPECT(CBUFFER_DATA_LEFT(&cb) == 0);

	/* Store a single byte in the buffer */
	{
		char ch = 0x42;
		size_t n = CBUFFER_WRITE(&cb, &ch, 1);
		EXPECT(n == 1);
		EXPECT(!CBUFFER_EMPTY(&cb));
		EXPECT(CBUFFER_DATA_LEFT(&cb) == 1);
	}

	/* Attempt to read two bytes back (should yield only one) */
	{
		char ch = 0;
		size_t n = CBUFFER_READ(&cb, &ch, 2);
		EXPECT(n == 1);
		EXPECT(ch == 0x42);
		EXPECT(CBUFFER_EMPTY(&cb));
		EXPECT(CBUFFER_DATA_LEFT(&cb) == 0);
	}

	/* Write and read the entire buffer size; this should yield one less */
	{
		char testbuf[TEST_BUF_SIZE];
		for (unsigned int i = 0; i < TEST_BUF_SIZE; i++)
			testbuf[i] = (char)i;
		size_t nw = CBUFFER_WRITE(&cb, testbuf, TEST_BUF_SIZE);
		EXPECT(nw == TEST_BUF_SIZE - 1);

		char tempbuf[TEST_BUF_SIZE];
		size_t nr = CBUFFER_READ(&cb, tempbuf, TEST_BUF_SIZE);
		EXPECT(nr == TEST_BUF_SIZE - 1);
		EXPECT(memcmp(testbuf, tempbuf, TEST_BUF_SIZE - 1) == 0);
	}

	/* Read/write alternating */
	{
		for (unsigned int i = 0; i < TEST_BUF_SIZE / 2; i++) {
			/* Write two bytes */
			char temp[2];
			temp[0] = (char)(i * 2);
			temp[1] = (char)(i * 2 + 1);
			size_t nw = CBUFFER_WRITE(&cb, temp, 2);
			EXPECT(nw == 2);

			/* Read a single byte */
			char b;
			size_t nr = CBUFFER_READ(&cb, &b, 1);
			EXPECT(nr == 1);

			/* Byte should be the index counter; (we write 2 but read just one) */
			EXPECT(b == i);
		}
	}

	/* Read remaining bytes */
	{
		char testbuf[TEST_BUF_SIZE / 2];
		for (unsigned int i = 0; i < TEST_BUF_SIZE / 2; i++)
			testbuf[i] = (char)(TEST_BUF_SIZE / 2 + i);

		char tempbuf[TEST_BUF_SIZE];
		size_t nr = CBUFFER_READ(&cb, tempbuf, TEST_BUF_SIZE);
		EXPECT(nr == TEST_BUF_SIZE / 2);

		EXPECT(memcmp(testbuf, tempbuf, nr) == 0);
	}

	/* Finally, buffer should be empty now */
	EXPECT(CBUFFER_EMPTY(&cb));
	EXPECT(CBUFFER_DATA_LEFT(&cb) == 0);

	free(buf_storage);
}

/* vim:set ts=2 sw=2: */
