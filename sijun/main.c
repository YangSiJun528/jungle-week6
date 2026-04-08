#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"

/**
 * @brief 특별한 시스템 명령어 처리 결과를 나타낸다.
 */
typedef enum {
    SYSTEM_COMMAND_NOT_MATCHED,
    SYSTEM_COMMAND_HANDLED,
    SYSTEM_COMMAND_EXIT
} SystemCommandResult;

/**
 * @brief 일반 텍스트 입력을 처리하고 출력할 문자열을 반환한다.
 * @param text 개행과 특별 명령이 제거된 일반 텍스트
 * @return 출력할 문자열
 */
static const char *process_args(const char *text) {
    return text;
}

/**
 * @brief 점으로 시작하는 특별한 시스템 명령어를 처리한다.
 * @param input 사용자가 입력한 한 줄 전체 문자열
 * @return 명령어 처리 결과
 */
static SystemCommandResult process_system_command(const char *input) {
    if (input[0] != '.') {
        return SYSTEM_COMMAND_NOT_MATCHED;
    }

    if (strcmp(input, ".exit") == 0) {
        return SYSTEM_COMMAND_EXIT;
    }

    fprintf(stderr, "Unknown command: %s\n", input);
    return SYSTEM_COMMAND_HANDLED;
}

int main(void) {
    char *line = NULL;
    size_t capacity = 0;

    while (getline(&line, &capacity, stdin) != -1) {
        size_t length = strlen(line);
        const char *result;
        SystemCommandResult command_result;

        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }

        if (line[0] == '\0') {
            continue;
        }

        command_result = process_system_command(line);
        if (command_result == SYSTEM_COMMAND_EXIT) {
            break;
        }
        if (command_result == SYSTEM_COMMAND_HANDLED) {
            continue;
        }

        result = process_args(line);
        printf("%s\n", result);
    }

    free(line);
    return OK;
}
