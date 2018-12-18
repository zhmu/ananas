/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2018 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
#ifndef TESTS_SYSTEM_FRAMEWORK_H
#define TESTS_SYSTEM_FRAMEWORK_H

namespace TestFramework
{
    enum TestResult { TR_Failure, TR_Success };

    TestResult TestBody();
    void TestBegin(const char* file);
    void TestEnd(const char* file);
    void TestAbort(const char* file, int line);
    void TestFailure(const char* file, int line);
    void TestExpectDeath(const char* file, int line);
    void TestBreak();

#define TEST_BODY_BEGIN                                 \
    TestFramework::TestResult TestFramework::TestBody() \
    {                                                   \
        TestResult testResult = TR_Success;             \
        TestBegin(__FILE__);                            \
        do

#define TEST_BODY_END  \
    while (0)          \
        ;              \
    TestEnd(__FILE__); \
    return testResult; \
    }

} // namespace TestFramework

#define TEST_EXPECT_FAILED               \
    do {                                 \
        TestFailure(__FILE__, __LINE__); \
        testResult = TR_Failure;         \
    } while (0)

#define TEST_ASSERT_FAILED               \
    do {                                 \
        TestFailure(__FILE__, __LINE__); \
        TestAbort(__FILE__, __LINE__);   \
        return TR_Failure;               \
    } while (0)

// TEST_{ESCAPE,CLOBBER} are inspired by Chandler Carruth's talk "Tuning C++:
// Benchmarks, and CPUs, and Compilers! Oh My!" (video time 40:40)

// Escape ensures a variable will not be optimized away
#define TEST_ESCAPE(v) __asm __volatile("" : : "g"(p) : "memory")

// Removes any assumption on memory
#define TEST_CLOBBER() __asm __volatile("" : : : "memory")

// EXPECT_... continues if the condition does not hold
#define EXPECT_EQ(expected, actual) \
    if ((actual) != (expected))     \
    TEST_EXPECT_FAILED

#define EXPECT_NE(expected, actual) \
    if ((actual) == (expected))     \
    TEST_EXPECT_FAILED

// ASSERT_... stops execution if the condition does not hold
#define ASSERT_EQ(expected, actual) \
    if ((actual) != (expected))     \
    TEST_ASSERT_FAILED

#define ASSERT_NE(expected, actual) \
    if ((actual) == (expected))     \
    TEST_ASSERT_FAILED

#define ASSERT_FAILURE TEST_ASSERT_FAILED

// ASSERT_DEATH() claims the statement must cause process termination
#define ASSERT_DEATH(statement)              \
    do {                                     \
        TestExpectDeath(__FILE__, __LINE__); \
        statement;                           \
        TEST_ASSERT_FAILED;                  \
    } while (0)

#endif // TESTS_SYSTEM_FRAMEWORK_H
