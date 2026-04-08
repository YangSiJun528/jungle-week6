/*
 * 이 파일은 단위 테스트가 아니라 실행 파일 기준의 통합 테스트다.
 * ctest가 test_main을 실행하면서 sijun 실행 파일 경로를 인자로 넘기고,
 * test_main은 popen()으로 해당 프로그램에 표준 입력을 흘려보낸 뒤 출력과 종료 코드를 검증한다.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "utils.h"

/* 프로그램을 실행하면 종료 코드와 출력을 확인할 수 있어야 한다. */
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

/* 일반 텍스트를 입력한 뒤 .exit 하면 같은 문자열을 출력해야 한다. */
static void prints_single_line_until_exit(const char *program_path) {
    char output[64];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "printf 'hello\\n.exit\\n' | \"%s\"", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, "hello\n") == 0);
}

/* 빈 줄은 무시하고 다음 텍스트만 출력해야 한다. */
static void ignores_empty_lines(const char *program_path) {
    char output[64];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "printf '\\nhello\\n\\n.exit\\n' | \"%s\"", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, "hello\n") == 0);
}

/* 공백이 있는 줄은 그대로 보존해 출력해야 한다. */
static void preserves_whitespace(const char *program_path) {
    char output[16];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "printf ' hi  \\n.exit\\n' | \"%s\"", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, " hi  \n") == 0);
}

/* 아주 긴 줄은 실행하면 잘리지 않고 그대로 출력해야 한다. */
static void prints_very_long_line(const char *program_path) {
    enum { LONG_SIZE = 8192, COMMAND_SIZE = 16384 };
    static char long_value[LONG_SIZE];
    static char command[COMMAND_SIZE];
    static char output[LONG_SIZE + 2];

    /* given */
    for (int i = 0; i < LONG_SIZE - 1; ++i) {
        long_value[i] = 'a' + (i % 26);
    }
    long_value[LONG_SIZE - 1] = '\0';
    snprintf(command, sizeof(command), "printf '%s\\n.exit\\n' | \"%s\"", long_value, program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strlen(output) == strlen(long_value) + 1);
    assert(strncmp(output, long_value, strlen(long_value)) == 0);
    assert(output[strlen(long_value)] == '\n');
}

/* .exit만 입력하면 추가 출력 없이 종료해야 한다. */
static void exits_without_extra_output(const char *program_path) {
    char output[16];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "printf '.exit\\n' | \"%s\"", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, "") == 0);
}

/* 알 수 없는 특별 명령어는 에러를 출력한 뒤 계속 처리해야 한다. */
static void reports_unknown_command_and_continues(const char *program_path) {
    char output[128];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "printf '.unknown\\nhello\\n.exit\\n' | \"%s\" 2>&1", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, "Unknown command: .unknown\nhello\n") == 0);
}

/* .exit 없이 EOF가 와도 정상 종료해야 한다. */
static void exits_on_eof(const char *program_path) {
    char output[64];
    char command[1024];

    /* given */
    snprintf(command, sizeof(command), "printf 'hello\\n' | \"%s\"", program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assert(exit_code == OK);
    assert(strcmp(output, "hello\n") == 0);
}

int main(int argc, char *argv[]) {
    /* given */
    assert(argc == 2);

    /* when */
    const char *program_path = argv[1];

    /* then */
    prints_single_line_until_exit(program_path);
    ignores_empty_lines(program_path);
    preserves_whitespace(program_path);
    prints_very_long_line(program_path);
    exits_without_extra_output(program_path);
    reports_unknown_command_and_continues(program_path);
    exits_on_eof(program_path);
    return 0;
}
