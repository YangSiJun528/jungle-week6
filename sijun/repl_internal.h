#ifndef REPL_INTERNAL_H
#define REPL_INTERNAL_H

#include <stddef.h>
#include <sys/types.h>

/**
 * @brief 입력 한 줄을 처리한 결과 종류를 나타낸다.
 */
typedef enum {
    LINE_ACTION_SKIP,
    LINE_ACTION_PRINT,
    LINE_ACTION_ERROR,
    LINE_ACTION_EXIT
} LineActionType;

/**
 * @brief 한 줄 처리 결과 데이터를 담는다.
 */
typedef struct {
    LineActionType type;
    const char *message;
} LineAction;

/**
 * @brief 문자열 끝의 개행 문자를 제거한다.
 * @param line 수정할 문자열
 * @param length getline이 반환한 문자열 길이
 * @return 개행 제거 후 문자열 길이
 */
size_t trim_newline(char *line, ssize_t length);

/**
 * @brief 입력 한 줄을 해석해 다음 동작을 결정한다.
 * @param line 개행이 제거된 입력 문자열
 * @return 입력 처리 결과
 */
LineAction evaluate_line(char *line);

#endif
