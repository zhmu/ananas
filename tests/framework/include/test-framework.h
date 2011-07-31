#ifndef __TEST_FRAMEWORK_H__
#define __TEST_FRAMEWORK_H__

enum FRAMEWORK_EXPECTING {
	SUCCESS,
	FAILURE
};

void framework_init();
void framework_done();
void framework_expect(int cond, enum FRAMEWORK_EXPECTING e, const char* file, int line, const char* expr);

/* EXPECT(x) is used for a test which is expected to be successful */
#define EXPECT(x) \
	framework_expect((x), SUCCESS, __FILE__, __LINE__, #x)

/* EXPECT_FAILURE(x) is used for a test which is expected not to pass just yet */
#define EXPECT_FAILURE(x) \
	framework_expect((x), FAILURE, __FILE__, __LINE__, #x)

#endif /* __TEST_FRAMEWORK_H__ */
