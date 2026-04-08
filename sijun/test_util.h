#ifndef TEST_UTIL_H
#define TEST_UTIL_H

#include <assert.h>
#include <stdbool.h>
#include <string.h>

/**
 * @brief 조건이 참인지 검증한다.
 * @param condition 검증할 조건
 */
static inline void assertTrue(bool condition) {
    assert(condition);
}

/**
 * @brief 정수 성격 값을 비교한다.
 * @param actual 실제 값
 * @param expected 기대 값
 */
static inline void assertEq(long long actual, long long expected) {
    assert(actual == expected);
}

/**
 * @brief 문자열이 같은지 검증한다.
 * @param actual 실제 문자열
 * @param expected 기대 문자열
 */
static inline void assertStrEq(const char *actual, const char *expected) {
    assert(actual != NULL);
    assert(expected != NULL);
    assert(strcmp(actual, expected) == 0);
}

/**
 * @brief 포인터가 NULL인지 검증한다.
 * @param pointer 검증할 포인터
 */
static inline void assertNull(const void *pointer) {
    assert(pointer == NULL);
}

#endif
