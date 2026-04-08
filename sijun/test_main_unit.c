#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "utils.h"

/**
 * @brief 스트림 전체를 읽어 문자열 버퍼에 담는다.
 * @param stream 읽을 스트림
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 */
static void read_stream(FILE *stream, char *buffer, size_t buffer_size) {
    size_t length = 0;
    int ch;

    rewind(stream);

    while ((ch = fgetc(stream)) != EOF && length + 1 < buffer_size) {
        buffer[length++] = (char) ch;
    }

    buffer[length] = '\0';
}

/**
 * @brief 메모리 기반 입력으로 run_repl 결과를 검증하기 위한 헬퍼다.
 * @param input_text 입력 문자열
 * @param output_buffer 표준 출력 결과 버퍼
 * @param output_buffer_size 표준 출력 버퍼 크기
 * @param error_buffer 표준 에러 결과 버퍼
 * @param error_buffer_size 표준 에러 버퍼 크기
 * @return run_repl 종료 코드
 */
static int run_repl_with_text(
    const char *input_text,
    char *output_buffer,
    size_t output_buffer_size,
    char *error_buffer,
    size_t error_buffer_size
) {
    FILE *input_stream = tmpfile();
    FILE *output_stream = tmpfile();
    FILE *error_stream = tmpfile();
    int result;

    assert(input_stream != NULL);
    assert(output_stream != NULL);
    assert(error_stream != NULL);

    fputs(input_text, input_stream);
    rewind(input_stream);

    result = run_repl(input_stream, output_stream, error_stream);

    read_stream(output_stream, output_buffer, output_buffer_size);
    read_stream(error_stream, error_buffer, error_buffer_size);

    fclose(input_stream);
    fclose(output_stream);
    fclose(error_stream);
    return result;
}

/* 개행이 있으면 제거하면 길이가 줄어야 한다. */
static void trims_trailing_newline(void) {
    char line[] = "hello\n";

    /* given */

    /* when */
    size_t length = trim_newline(line);

    /* then */
    assert(length == 5);
    assert(strcmp(line, "hello") == 0);
}

/* 개행이 없으면 문자열은 그대로 유지해야 한다. */
static void keeps_line_without_newline(void) {
    char line[] = "hello";

    /* given */

    /* when */
    size_t length = trim_newline(line);

    /* then */
    assert(length == 5);
    assert(strcmp(line, "hello") == 0);
}

/* 빈 줄은 건너뛰어야 한다. */
static void skips_empty_line(void) {
    char line[] = "";
    char error_buffer[64];

    /* given */

    /* when */
    LineAction action = evaluate_line(line, error_buffer, sizeof(error_buffer));

    /* then */
    assert(action.type == LINE_ACTION_SKIP);
    assert(action.message == NULL);
}

/* 공백만 있는 줄은 그대로 출력해야 한다. */
static void prints_whitespace_only_line(void) {
    char line[] = "   ";
    char error_buffer[64];

    /* given */

    /* when */
    LineAction action = evaluate_line(line, error_buffer, sizeof(error_buffer));

    /* then */
    assert(action.type == LINE_ACTION_PRINT);
    assert(strcmp(action.message, "   ") == 0);
}

/* 일반 텍스트는 그대로 출력해야 한다. */
static void prints_plain_text(void) {
    char line[] = "hello";
    char error_buffer[64];

    /* given */

    /* when */
    LineAction action = evaluate_line(line, error_buffer, sizeof(error_buffer));

    /* then */
    assert(action.type == LINE_ACTION_PRINT);
    assert(strcmp(action.message, "hello") == 0);
}

/* .exit는 종료 동작이어야 한다. */
static void exits_on_exit_command(void) {
    char line[] = ".exit";
    char error_buffer[64];

    /* given */

    /* when */
    LineAction action = evaluate_line(line, error_buffer, sizeof(error_buffer));

    /* then */
    assert(action.type == LINE_ACTION_EXIT);
    assert(action.message == NULL);
}

/* 알 수 없는 점 명령은 에러 메시지를 만들어야 한다. */
static void reports_unknown_command(void) {
    char line[] = ".unknown";
    char error_buffer[64];

    /* given */

    /* when */
    LineAction action = evaluate_line(line, error_buffer, sizeof(error_buffer));

    /* then */
    assert(action.type == LINE_ACTION_ERROR);
    assert(strcmp(action.message, "Unknown command: .unknown") == 0);
}

/* 긴 문자열도 잘리지 않고 그대로 반환해야 한다. */
static void preserves_long_line(void) {
    enum { LONG_SIZE = 8192 };
    static char line[LONG_SIZE];
    char error_buffer[64];

    /* given */
    for (int i = 0; i < LONG_SIZE - 1; ++i) {
        line[i] = (char) ('a' + (i % 26));
    }
    line[LONG_SIZE - 1] = '\0';

    /* when */
    LineAction action = evaluate_line(line, error_buffer, sizeof(error_buffer));

    /* then */
    assert(action.type == LINE_ACTION_PRINT);
    assert(strcmp(action.message, line) == 0);
}

/* stdout과 stderr는 분리되어 기록되어야 한다. */
static void separates_output_and_error_streams(void) {
    char output_buffer[64];
    char error_buffer[64];

    /* given */

    /* when */
    int result = run_repl_with_text(".unknown\nhello\n.exit\n", output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assert(result == OK);
    assert(strcmp(output_buffer, "hello\n") == 0);
    assert(strcmp(error_buffer, "Unknown command: .unknown\n") == 0);
}

/* 알 수 없는 명령 뒤에도 다음 입력을 계속 처리해야 한다. */
static void continues_after_unknown_command(void) {
    char output_buffer[64];
    char error_buffer[64];

    /* given */

    /* when */
    int result = run_repl_with_text(".unknown\nhello\nworld\n.exit\n", output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assert(result == OK);
    assert(strcmp(output_buffer, "hello\nworld\n") == 0);
    assert(strcmp(error_buffer, "Unknown command: .unknown\n") == 0);
}

/* .exit 없이 EOF가 와도 정상 종료해야 한다. */
static void exits_cleanly_on_eof(void) {
    char output_buffer[64];
    char error_buffer[16];

    /* given */

    /* when */
    int result = run_repl_with_text("hello\n", output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assert(result == OK);
    assert(strcmp(output_buffer, "hello\n") == 0);
    assert(strcmp(error_buffer, "") == 0);
}

int main(void) {
    trims_trailing_newline();
    keeps_line_without_newline();
    skips_empty_line();
    prints_whitespace_only_line();
    prints_plain_text();
    exits_on_exit_command();
    reports_unknown_command();
    preserves_long_line();
    separates_output_and_error_streams();
    continues_after_unknown_command();
    exits_cleanly_on_eof();
    return 0;
}
