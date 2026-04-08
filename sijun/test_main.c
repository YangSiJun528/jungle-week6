#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "utils.h"

/* 프로그램을 실행하면 종료 코드와 표준 출력을 확인할 수 있어야 한다. */
static int run_command(const char *command, char *output, size_t output_size) {
    FILE *pipe = popen(command, "r");
    size_t length = 0;
    int ch;
    int status;

    assert(pipe != NULL);

    while ((ch = fgetc(pipe)) != EOF && length + 1 < output_size) {
        output[length++] = (char) ch;
    }
    output[length] = '\0';

    status = pclose(pipe);
    assert(status != -1);
    assert(WIFEXITED(status));
    return WEXITSTATUS(status);
}

/* 인자가 하나일 때 실행하면 같은 문자열을 출력해야 한다. */
static void prints_single_argument(const char *program_path) {
    char output[64];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "\"%s\" hello", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, "hello\n") == 0);
}

/* 빈 문자열일 때 실행하면 개행만 출력해야 한다. */
static void prints_empty_string(const char *program_path) {
    char output[16];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "\"%s\" \"\"", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, "\n") == 0);
}

/* 아주 긴 문자열은 실행하면 잘리지 않고 그대로 출력해야 한다. */
static void prints_very_long_string(const char *program_path) {
    enum { LONG_SIZE = 8192, COMMAND_SIZE = 16384 };
    static char long_value[LONG_SIZE];
    static char command[COMMAND_SIZE];
    static char output[LONG_SIZE + 2];

    /* given */
    for (int i = 0; i < LONG_SIZE - 1; ++i) {
        long_value[i] = 'a' + (i % 26);
    }
    long_value[LONG_SIZE - 1] = '\0';
    snprintf(command, sizeof(command), "\"%s\" \"%s\"", program_path, long_value);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strlen(output) == strlen(long_value) + 1);
    assert(strncmp(output, long_value, strlen(long_value)) == 0);
    assert(output[strlen(long_value)] == '\n');
}

/* 인자가 없을 때 실행하면 실패해야 한다. */
static void fails_without_argument(const char *program_path) {
    char output[16];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "\"%s\"", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == ERR);
    assert(strcmp(output, "") == 0);
}

/* 인자가 두 개 이상일 때 실행하면 실패해야 한다. */
static void fails_with_too_many_arguments(const char *program_path) {
    char output[16];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "\"%s\" first second", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == ERR);
    assert(strcmp(output, "") == 0);
}

int main(int argc, char *argv[]) {
    /* given */
    assert(argc == 2);

    /* when */
    const char *program_path = argv[1];

    /* then */
    prints_single_argument(program_path);
    prints_empty_string(program_path);
    prints_very_long_string(program_path);
    fails_without_argument(program_path);
    fails_with_too_many_arguments(program_path);
    return 0;
}
