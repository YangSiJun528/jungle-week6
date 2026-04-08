#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *input;
    size_t position;
    const char *error_message;
    size_t error_position;
} Parser;

/**
 * @brief 파싱 실패 정보를 기록한다.
 * @param parser 파서 상태
 * @param message 실패 메시지
 */
static void set_parse_error(Parser *parser, const char *message) {
    if (parser->error_message != NULL) {
        return;
    }

    parser->error_message = message;
    parser->error_position = parser->position;
}

/**
 * @brief 현재 문자를 반환한다.
 * @param parser 파서 상태
 * @return 현재 문자
 */
static char current_char(const Parser *parser) {
    return parser->input[parser->position];
}

/**
 * @brief 공백 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 공백이면 1, 아니면 0
 */
static int is_sql_whitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n';
}

/**
 * @brief 식별자 시작 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 문자면 1, 아니면 0
 */
static int is_identifier_start(char ch) {
    return isalpha((unsigned char) ch) != 0;
}

/**
 * @brief 식별자 본문 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 허용 문자면 1, 아니면 0
 */
static int is_identifier_char(char ch) {
    return isalpha((unsigned char) ch) != 0 || isdigit((unsigned char) ch) != 0 || ch == '_';
}

/**
 * @brief 문자열 내부 허용 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 허용 문자면 1, 아니면 0
 */
static int is_string_char(char ch) {
    return is_identifier_char(ch) || ch == ' ';
}

/**
 * @brief 현재 위치부터 공백을 모두 소비한다.
 * @param parser 파서 상태
 */
static void consume_ws(Parser *parser) {
    while (is_sql_whitespace(current_char(parser))) {
        parser->position++;
    }
}

/**
 * @brief 현재 위치부터 필수 공백을 소비한다.
 * @param parser 파서 상태
 * @return 성공하면 1, 실패하면 0
 */
static int consume_req_ws(Parser *parser) {
    if (!is_sql_whitespace(current_char(parser))) {
        set_parse_error(parser, "expected whitespace");
        return 0;
    }

    consume_ws(parser);
    return 1;
}

/**
 * @brief 현재 위치의 리터럴과 정확히 일치하는지 검사하고 소비한다.
 * @param parser 파서 상태
 * @param literal 기대 문자열
 * @return 성공하면 1, 실패하면 0
 */
static int consume_literal(Parser *parser, const char *literal) {
    size_t length = strlen(literal);

    if (strncmp(parser->input + parser->position, literal, length) != 0) {
        set_parse_error(parser, "unexpected token");
        return 0;
    }

    parser->position += length;
    return 1;
}

/**
 * @brief 지정 구간을 복사한 새 문자열을 만든다.
 * @param start 시작 포인터
 * @param length 길이
 * @return 새로 할당한 문자열, 실패 시 NULL
 */
static char *copy_range(const char *start, size_t length) {
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

/**
 * @brief 식별자를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 문자열
 * @return 성공하면 1, 실패하면 0
 */
static int parse_identifier(Parser *parser, char **out) {
    size_t start = parser->position;

    if (!is_identifier_start(current_char(parser))) {
        set_parse_error(parser, "expected identifier");
        return 0;
    }

    parser->position++;
    while (is_identifier_char(current_char(parser))) {
        parser->position++;
    }

    *out = copy_range(parser->input + start, parser->position - start);
    if (*out == NULL) {
        set_parse_error(parser, "out of memory");
        return 0;
    }

    return 1;
}

/**
 * @brief 숫자를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
static int parse_number(Parser *parser, SqlValue *out) {
    size_t start = parser->position;

    if (!isdigit((unsigned char) current_char(parser))) {
        set_parse_error(parser, "expected value");
        return 0;
    }

    parser->position++;
    while (isdigit((unsigned char) current_char(parser))) {
        parser->position++;
    }

    out->type = VALUE_TYPE_NUMBER;
    out->text = copy_range(parser->input + start, parser->position - start);
    if (out->text == NULL) {
        set_parse_error(parser, "out of memory");
        return 0;
    }

    return 1;
}

/**
 * @brief 문자열 리터럴을 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
static int parse_string(Parser *parser, SqlValue *out) {
    size_t start;

    if (current_char(parser) != '\'') {
        set_parse_error(parser, "expected value");
        return 0;
    }

    parser->position++;
    start = parser->position;

    while (current_char(parser) != '\0' && current_char(parser) != '\'') {
        if (!is_string_char(current_char(parser))) {
            set_parse_error(parser, "invalid string character");
            return 0;
        }
        parser->position++;
    }

    if (current_char(parser) != '\'') {
        set_parse_error(parser, "unterminated string");
        return 0;
    }

    out->type = VALUE_TYPE_STRING;
    out->text = copy_range(parser->input + start, parser->position - start);
    if (out->text == NULL) {
        set_parse_error(parser, "out of memory");
        return 0;
    }

    parser->position++;
    return 1;
}

/**
 * @brief 값 하나를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
static int parse_value(Parser *parser, SqlValue *out) {
    char ch = current_char(parser);

    out->text = NULL;

    if (isdigit((unsigned char) ch)) {
        return parse_number(parser, out);
    }
    if (ch == '\'') {
        return parse_string(parser, out);
    }
    if (is_identifier_start(ch)) {
        out->type = VALUE_TYPE_IDENTIFIER;
        return parse_identifier(parser, &out->text);
    }

    set_parse_error(parser, "expected value");
    return 0;
}

/**
 * @brief 값 배열에 항목을 하나 추가한다.
 * @param result 파싱 결과
 * @param value 추가할 값
 * @return 성공하면 1, 실패하면 0
 */
static int append_value(ParseResult *result, SqlValue value) {
    SqlValue *next_values = realloc(result->values, sizeof(SqlValue) * (result->value_count + 1));

    if (next_values == NULL) {
        return 0;
    }

    result->values = next_values;
    result->values[result->value_count++] = value;
    return 1;
}

/**
 * @brief insert용 값 목록을 파싱한다.
 * @param parser 파서 상태
 * @param result 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
static int parse_value_list(Parser *parser, ParseResult *result) {
    SqlValue value;

    if (!parse_value(parser, &value)) {
        return 0;
    }
    if (!append_value(result, value)) {
        free(value.text);
        set_parse_error(parser, "out of memory");
        return 0;
    }

    while (1) {
        consume_ws(parser);
        if (current_char(parser) != ',') {
            break;
        }

        parser->position++;
        consume_ws(parser);

        if (!parse_value(parser, &value)) {
            return 0;
        }
        if (!append_value(result, value)) {
            free(value.text);
            set_parse_error(parser, "out of memory");
            return 0;
        }
    }

    return 1;
}

/**
 * @brief select 쿼리를 파싱한다.
 * @param parser 파서 상태
 * @param result 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
static int parse_select_query(Parser *parser, ParseResult *result) {
    if (!consume_literal(parser, "select")) {
        return 0;
    }
    if (!consume_req_ws(parser)) {
        return 0;
    }
    if (!consume_literal(parser, "*")) {
        return 0;
    }
    if (!consume_req_ws(parser)) {
        return 0;
    }
    if (!consume_literal(parser, "from")) {
        return 0;
    }
    if (!consume_req_ws(parser)) {
        return 0;
    }
    if (!parse_identifier(parser, &result->table_name)) {
        return 0;
    }

    result->type = QUERY_TYPE_SELECT;
    return 1;
}

/**
 * @brief insert 쿼리를 파싱한다.
 * @param parser 파서 상태
 * @param result 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
static int parse_insert_query(Parser *parser, ParseResult *result) {
    if (!consume_literal(parser, "insert")) {
        return 0;
    }
    if (!consume_req_ws(parser)) {
        return 0;
    }
    if (!consume_literal(parser, "into")) {
        return 0;
    }
    if (!consume_req_ws(parser)) {
        return 0;
    }
    if (!parse_identifier(parser, &result->table_name)) {
        return 0;
    }
    if (!consume_req_ws(parser)) {
        return 0;
    }
    if (!consume_literal(parser, "values")) {
        return 0;
    }
    consume_ws(parser);
    if (current_char(parser) != '(') {
        set_parse_error(parser, "expected (");
        return 0;
    }
    parser->position++;
    consume_ws(parser);
    if (!parse_value_list(parser, result)) {
        return 0;
    }
    consume_ws(parser);
    if (current_char(parser) != ')') {
        set_parse_error(parser, "expected )");
        return 0;
    }
    parser->position++;

    result->type = QUERY_TYPE_INSERT;
    return 1;
}

/**
 * @brief parse()가 할당한 메모리를 해제한다.
 * @param result 해제할 파싱 결과
 */
void free_parse_result(ParseResult *result) {
    size_t index;

    free(result->table_name);
    result->table_name = NULL;

    for (index = 0; index < result->value_count; ++index) {
        free(result->values[index].text);
    }

    free(result->values);
    result->values = NULL;
    result->value_count = 0;
    result->type = QUERY_TYPE_INVALID;
    result->error_message = NULL;
    result->error_position = 0;
}

/**
 * @brief 경량 SQL 문자열을 EBNF 규칙에 따라 파싱한다.
 * @param input 파싱할 SQL 문자열
 * @return 파싱 결과
 */
ParseResult parse(const char *input) {
    Parser parser;
    ParseResult result;

    result.type = QUERY_TYPE_INVALID;
    result.table_name = NULL;
    result.values = NULL;
    result.value_count = 0;
    result.error_message = NULL;
    result.error_position = 0;

    parser.input = input;
    parser.position = 0;
    parser.error_message = NULL;
    parser.error_position = 0;

    consume_ws(&parser);

    if (strncmp(parser.input + parser.position, "select", 6) == 0) {
        parse_select_query(&parser, &result);
    } else if (strncmp(parser.input + parser.position, "insert", 6) == 0) {
        parse_insert_query(&parser, &result);
    } else {
        set_parse_error(&parser, "expected query");
    }

    if (parser.error_message == NULL) {
        consume_ws(&parser);
        if (current_char(&parser) != '\0') {
            set_parse_error(&parser, "unexpected trailing input");
        }
    }

    if (parser.error_message != NULL) {
        free_parse_result(&result);
        result.error_message = parser.error_message;
        result.error_position = parser.error_position;
        return result;
    }

    return result;
}

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
