#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include "utils.h"

/**
 * @brief 외부 프로그램을 실행하고 출력을 수집한다.
 * @param command 실행할 셸 명령어
 * @param output 결과를 저장할 버퍼
 * @param output_size 결과 버퍼 크기
 * @return 종료 코드
 */
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

/* 일반 텍스트 뒤 .exit를 입력하면 같은 문자열만 출력하고 종료해야 한다. */
static void prints_text_until_exit(const char *program_path) {
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

/* .exit 없이 EOF가 와도 마지막 입력을 출력하고 종료해야 한다. */
static void prints_text_until_eof(const char *program_path) {
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
    assert(argc == 2);

    prints_text_until_exit(argv[1]);
    prints_text_until_eof(argv[1]);
    return 0;
}
