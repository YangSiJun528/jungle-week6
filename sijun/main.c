#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 문자열 끝의 개행 문자를 제거한다.
 * @param line 수정할 문자열
 * @param length getline이 반환한 문자열 길이
 * @return 개행 제거 후 문자열 길이
 */
size_t trim_newline(char *line, ssize_t length) {
    size_t trimmed_length = (size_t) length;

    if (trimmed_length > 0 && line[trimmed_length - 1] == '\n') {
        line[--trimmed_length] = '\0';
    }

    if (trimmed_length > 0 && line[trimmed_length - 1] == '\r') {
        line[--trimmed_length] = '\0';
    }

    return trimmed_length;
}

/**
 * @brief 입력 한 줄을 해석해 다음 동작을 결정한다.
 * @param line 개행이 제거된 입력 문자열
 * @return 입력 처리 결과
 */
LineAction evaluate_line(char *line) {
    LineAction action;

    if (line[0] == '\0') {
        action.type = LINE_ACTION_SKIP;
        action.message = NULL;
        return action;
    }

    if (line[0] == '.') {
        if (strcmp(line, ".exit") == 0) {
            action.type = LINE_ACTION_EXIT;
            action.message = NULL;
            return action;
        }

        action.type = LINE_ACTION_ERROR;
        action.message = line;
        return action;
    }

    action.type = LINE_ACTION_PRINT;
    action.message = line;
    return action;
}

/**
 * @brief 입력 스트림을 읽어 출력 스트림으로 결과를 기록한다.
 * @param input_stream 입력 스트림
 * @param output_stream 표준 출력용 스트림
 * @param error_stream 표준 에러용 스트림
 * @return 프로그램 종료 코드
 */
int run_repl(FILE *input_stream, FILE *output_stream, FILE *error_stream) {
    char *line = NULL;
    size_t capacity = 0;
    ssize_t line_length;

    while ((line_length = getline(&line, &capacity, input_stream)) != -1) {
        LineAction action;

        trim_newline(line, line_length);
        action = evaluate_line(line);

        if (action.type == LINE_ACTION_SKIP) {
            continue;
        }
        if (action.type == LINE_ACTION_EXIT) {
            break;
        }
        if (action.type == LINE_ACTION_ERROR) {
            fprintf(error_stream, "Unknown command: %s\n", action.message);
            continue;
        }

        fprintf(output_stream, "%s\n", action.message);
    }

    free(line);
    return OK;
}

#ifndef SIJUN_TEST_BUILD
int main(void) {
    return run_repl(stdin, stdout, stderr);
}
#endif
