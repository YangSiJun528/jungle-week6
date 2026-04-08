#include "parser_internal.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief 현재 위치부터 공백을 모두 소비한다.
 * @param parser 파서 상태
 */
void consume_ws(Parser *parser) {
    while (is_sql_whitespace(current_char(parser))) {
        parser->position++;
    }
}

/**
 * @brief 현재 위치부터 필수 공백을 소비한다.
 * @param parser 파서 상태
 * @return 성공하면 1, 실패하면 0
 */
bool consume_req_ws(Parser *parser) {
    if (!is_sql_whitespace(current_char(parser))) {
        set_parse_error(parser, "expected whitespace");
        return false;
    }

    consume_ws(parser);
    return true;
}

/**
 * @brief 현재 위치의 리터럴과 정확히 일치하는지 검사하고 소비한다.
 * @param parser 파서 상태
 * @param literal 기대 문자열
 * @return 성공하면 1, 실패하면 0
 */
bool consume_literal(Parser *parser, const char *literal) {
    size_t length = strlen(literal);

    if (strncmp(parser->input + parser->position, literal, length) != 0) {
        set_parse_error(parser, "unexpected token");
        return false;
    }

    parser->position += length;
    return true;
}

/**
 * @brief 지정 구간을 복사한 새 문자열을 만든다.
 * @param start 시작 포인터
 * @param length 길이
 * @return 새로 할당한 문자열, 실패 시 NULL
 */
char *copy_range(const char *start, size_t length) {
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, start, length);
    copy[length] = '\0';
    return copy;
}

/**
 * @brief 현재 문자를 반환한다.
 * @param parser 파서 상태
 * @return 현재 문자
 */
char current_char(const Parser *parser) {
    return parser->input[parser->position];
}

/**
 * @brief ASCII 영문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 영문자면 1, 아니면 0
 */
bool is_ascii_letter(char ch) {
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z');
}

/**
 * @brief ASCII 숫자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 숫자면 1, 아니면 0
 */
bool is_ascii_digit(char ch) {
    return '0' <= ch && ch <= '9';
}

/**
 * @brief 공백 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 공백이면 1, 아니면 0
 */
bool is_sql_whitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n';
}

/**
 * @brief 식별자 시작 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 문자면 1, 아니면 0
 */
bool is_identifier_start(char ch) {
    return is_ascii_letter(ch);
}

/**
 * @brief 식별자 본문 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 허용 문자면 1, 아니면 0
 */
bool is_identifier_char(char ch) {
    return is_ascii_letter(ch) || is_ascii_digit(ch) || ch == '_';
}

/**
 * @brief 문자열 내부 허용 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 허용 문자면 1, 아니면 0
 */
bool is_string_char(char ch) {
    return is_identifier_char(ch) || ch == ' ';
}

/**
 * @brief 파싱 실패 정보를 기록한다.
 * @param parser 파서 상태
 * @param message 실패 메시지
 */
void set_parse_error(Parser *parser, const char *message) {
    if (parser->error_message != NULL) {
        return;
    }

    parser->error_message = message;
    parser->error_position = parser->position;
}
