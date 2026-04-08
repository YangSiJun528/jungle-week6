#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "test_util.h"
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

    assertTrue(input_stream != NULL);
    assertTrue(output_stream != NULL);
    assertTrue(error_stream != NULL);

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
    size_t length = trim_newline(line, 6);

    /* then */
    assertEq((long long) length, 5);
    assertStrEq(line, "hello");
}

/* CRLF도 함께 제거해야 명령어 비교가 정확하다. */
static void trims_trailing_crlf(void) {
    char line[] = ".exit\r\n";

    /* given */

    /* when */
    size_t length = trim_newline(line, 7);

    /* then */
    assertEq((long long) length, 5);
    assertStrEq(line, ".exit");
}

/* 개행이 없으면 문자열은 그대로 유지해야 한다. */
static void keeps_line_without_newline(void) {
    char line[] = "hello";

    /* given */

    /* when */
    size_t length = trim_newline(line, 5);

    /* then */
    assertEq((long long) length, 5);
    assertStrEq(line, "hello");
}

/* 빈 줄은 건너뛰어야 한다. */
static void skips_empty_line(void) {
    char line[] = "";

    /* given */

    /* when */
    LineAction action = evaluate_line(line);

    /* then */
    assertEq(action.type, LINE_ACTION_SKIP);
    assertNull(action.message);
}

/* 공백만 있는 줄은 그대로 출력해야 한다. */
static void prints_whitespace_only_line(void) {
    char line[] = "   ";

    /* given */

    /* when */
    LineAction action = evaluate_line(line);

    /* then */
    assertEq(action.type, LINE_ACTION_PRINT);
    assertStrEq(action.message, "   ");
}

/* 일반 텍스트는 그대로 출력해야 한다. */
static void prints_plain_text(void) {
    char line[] = "hello";

    /* given */

    /* when */
    LineAction action = evaluate_line(line);

    /* then */
    assertEq(action.type, LINE_ACTION_PRINT);
    assertStrEq(action.message, "hello");
}

/* .exit는 종료 동작이어야 한다. */
static void exits_on_exit_command(void) {
    char line[] = ".exit";

    /* given */

    /* when */
    LineAction action = evaluate_line(line);

    /* then */
    assertEq(action.type, LINE_ACTION_EXIT);
    assertNull(action.message);
}

/* 알 수 없는 점 명령은 에러 메시지를 만들어야 한다. */
static void reports_unknown_command(void) {
    char line[] = ".unknown";

    /* given */

    /* when */
    LineAction action = evaluate_line(line);

    /* then */
    assertEq(action.type, LINE_ACTION_ERROR);
    assertStrEq(action.message, ".unknown");
}

/* 긴 문자열도 잘리지 않고 그대로 반환해야 한다. */
static void preserves_long_line(void) {
    enum { LONG_SIZE = 8192 };
    static char line[LONG_SIZE];

    /* given */
    for (int i = 0; i < LONG_SIZE - 1; ++i) {
        line[i] = (char) ('a' + (i % 26));
    }
    line[LONG_SIZE - 1] = '\0';

    /* when */
    LineAction action = evaluate_line(line);

    /* then */
    assertEq(action.type, LINE_ACTION_PRINT);
    assertStrEq(action.message, line);
}

/* stdout과 stderr는 분리되어 기록되어야 한다. */
static void separates_output_and_error_streams(void) {
    char output_buffer[64];
    char error_buffer[64];

    /* given */

    /* when */
    int result = run_repl_with_text(".unknown\nhello\n.exit\n", output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assertEq(result, OK);
    assertStrEq(output_buffer, "hello\n");
    assertStrEq(error_buffer, "Unknown command: .unknown\n");
}

/* 알 수 없는 명령 뒤에도 다음 입력을 계속 처리해야 한다. */
static void continues_after_unknown_command(void) {
    char output_buffer[64];
    char error_buffer[64];

    /* given */

    /* when */
    int result = run_repl_with_text(".unknown\nhello\nworld\n.exit\n", output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assertEq(result, OK);
    assertStrEq(output_buffer, "hello\nworld\n");
    assertStrEq(error_buffer, "Unknown command: .unknown\n");
}

/* .exit 없이 EOF가 와도 정상 종료해야 한다. */
static void exits_cleanly_on_eof(void) {
    char output_buffer[64];
    char error_buffer[16];

    /* given */

    /* when */
    int result = run_repl_with_text("hello\n", output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assertEq(result, OK);
    assertStrEq(output_buffer, "hello\n");
    assertStrEq(error_buffer, "");
}

/* CRLF 입력의 .exit도 종료 명령으로 처리해야 한다. */
static void exits_on_crlf_exit_command(void) {
    char output_buffer[16];
    char error_buffer[16];

    /* given */

    /* when */
    int result = run_repl_with_text(".exit\r\n", output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assertEq(result, OK);
    assertStrEq(output_buffer, "");
    assertStrEq(error_buffer, "");
}

/* select 문은 테이블명을 파싱해야 한다. */
static void parses_select_query(void) {
    ParseResult result = parse("select * from users");

    /* given */

    /* when */

    /* then */
    assertNull(result.error_message);
    assertEq(result.type, QUERY_TYPE_SELECT);
    assertStrEq(result.table_name, "users");
    assertEq((long long) result.value_count, 0);

    free_parse_result(&result);
}

/* 앞뒤 공백이 있어도 select 문은 파싱해야 한다. */
static void parses_select_query_with_surrounding_whitespace(void) {
    ParseResult result = parse(" \n\tselect * from members \t");

    /* given */

    /* when */

    /* then */
    assertNull(result.error_message);
    assertEq(result.type, QUERY_TYPE_SELECT);
    assertStrEq(result.table_name, "members");

    free_parse_result(&result);
}

/* insert 문은 숫자 문자열 식별자를 모두 파싱해야 한다. */
static void parses_insert_query_values(void) {
    ParseResult result = parse("insert into users values (1, 'kim min', admin_1)");

    /* given */

    /* when */

    /* then */
    assertNull(result.error_message);
    assertEq(result.type, QUERY_TYPE_INSERT);
    assertStrEq(result.table_name, "users");
    assertEq((long long) result.value_count, 3);
    assertEq(result.values[0].type, VALUE_TYPE_NUMBER);
    assertStrEq(result.values[0].text, "1");
    assertEq(result.values[1].type, VALUE_TYPE_STRING);
    assertStrEq(result.values[1].text, "kim min");
    assertEq(result.values[2].type, VALUE_TYPE_IDENTIFIER);
    assertStrEq(result.values[2].text, "admin_1");

    free_parse_result(&result);
}

/* insert 문은 괄호 주변 공백을 허용해야 한다. */
static void parses_insert_query_with_flexible_whitespace(void) {
    ParseResult result = parse("insert into logs values\t( foo ,42,'bar' )");

    /* given */

    /* when */

    /* then */
    assertNull(result.error_message);
    assertEq(result.type, QUERY_TYPE_INSERT);
    assertStrEq(result.table_name, "logs");
    assertEq((long long) result.value_count, 3);

    free_parse_result(&result);
}

/* 식별자는 문자로 시작해야 한다. */
static void rejects_table_name_starting_with_digit(void) {
    ParseResult result = parse("select * from 123users");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "expected identifier");
    assertEq((long long) result.error_position, 14);
}

/* 식별자는 ASCII 영문자로 시작해야 한다. */
static void rejects_non_ascii_table_name(void) {
    ParseResult result = parse("select * from \xE9users");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "expected identifier");
}

/* 문자열에는 정의된 문자만 허용해야 한다. */
static void rejects_invalid_string_character(void) {
    ParseResult result = parse("insert into users values ('a@a.com')");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "invalid string character");
}

/* 문자열도 EBNF의 ASCII 문자 집합만 허용해야 한다. */
static void rejects_non_ascii_string_character(void) {
    ParseResult result = parse("insert into users values ('\xE9')");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "invalid string character");
}

/* value list는 비어 있을 수 없다. */
static void rejects_empty_value_list(void) {
    ParseResult result = parse("insert into users values ()");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "expected value");
}

/* trailing input이 있으면 실패해야 한다. */
static void rejects_trailing_input(void) {
    ParseResult result = parse("select * from users extra");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "unexpected trailing input");
}

int main(void) {
    trims_trailing_newline();
    trims_trailing_crlf();
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
    exits_on_crlf_exit_command();
    parses_select_query();
    parses_select_query_with_surrounding_whitespace();
    parses_insert_query_values();
    parses_insert_query_with_flexible_whitespace();
    rejects_table_name_starting_with_digit();
    rejects_non_ascii_table_name();
    rejects_invalid_string_character();
    rejects_non_ascii_string_character();
    rejects_empty_value_list();
    rejects_trailing_input();
    return 0;
}
