#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 문자열 끝의 개행 문자를 제거한다.
 * @param line 수정할 문자열
 * @return 개행 제거 후 문자열 길이
 */
size_t trim_newline(char *line) {
    size_t length = strlen(line);

    if (length > 0 && line[length - 1] == '\n') {
        line[length - 1] = '\0';
        return length - 1;
    }

    return length;
}

/**
 * @brief 입력 한 줄을 해석해 다음 동작을 결정한다.
 * @param line 개행이 제거된 입력 문자열
 * @param error_buffer 에러 메시지를 기록할 버퍼
 * @param error_buffer_size 에러 버퍼 크기
 * @return 입력 처리 결과
 */
LineAction evaluate_line(char *line, char *error_buffer, size_t error_buffer_size) {
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

        snprintf(error_buffer, error_buffer_size, "Unknown command: %s", line);
        action.type = LINE_ACTION_ERROR;
        action.message = error_buffer;
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
    char error_buffer[1024];

    while (getline(&line, &capacity, input_stream) != -1) {
        LineAction action;

        trim_newline(line);
        action = evaluate_line(line, error_buffer, sizeof(error_buffer));

        if (action.type == LINE_ACTION_SKIP) {
            continue;
        }
        if (action.type == LINE_ACTION_EXIT) {
            break;
        }
        if (action.type == LINE_ACTION_ERROR) {
            fprintf(error_stream, "%s\n", action.message);
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
