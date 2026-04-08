#include "parser_internal.h"
#include <stdlib.h>

/**
 * @brief 값 하나를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
bool parse_value(Parser *parser, SqlValue *out) {
    char ch = current_char(parser);

    out->text = NULL;

    if (is_ascii_digit(ch)) {
        return parse_number(parser, out);
    }
    if (ch == '\'') {
        return parse_string(parser, out);
    }
    set_parse_error(parser, "expected value");
    return false;
}

/**
 * @brief 숫자를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
bool parse_number(Parser *parser, SqlValue *out) {
    size_t start = parser->position;

    if (!is_ascii_digit(current_char(parser))) {
        set_parse_error(parser, "expected value");
        return false;
    }

    parser->position++;
    while (is_ascii_digit(current_char(parser))) {
        parser->position++;
    }

    out->type = VALUE_TYPE_NUMBER;
    out->text = copy_range(parser->input + start, parser->position - start);
    if (out->text == NULL) {
        set_parse_error(parser, "out of memory");
        return false;
    }

    return true;
}

/**
 * @brief 문자열 리터럴을 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
bool parse_string(Parser *parser, SqlValue *out) {
    size_t start;

    if (current_char(parser) != '\'') {
        set_parse_error(parser, "expected value");
        return false;
    }

    parser->position++;
    start = parser->position;

    while (current_char(parser) != '\0' && current_char(parser) != '\'') {
        if (!is_string_char(current_char(parser))) {
            set_parse_error(parser, "invalid string character");
            return false;
        }
        parser->position++;
    }

    if (current_char(parser) != '\'') {
        set_parse_error(parser, "unterminated string");
        return false;
    }

    out->type = VALUE_TYPE_STRING;
    out->text = copy_range(parser->input + start, parser->position - start);
    if (out->text == NULL) {
        set_parse_error(parser, "out of memory");
        return false;
    }

    parser->position++;
    return true;
}

/**
 * @brief 식별자를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 문자열
 * @return 성공하면 1, 실패하면 0
 */
bool parse_identifier(Parser *parser, char **out) {
    size_t start = parser->position;

    if (!is_identifier_start(current_char(parser))) {
        set_parse_error(parser, "expected identifier");
        return false;
    }

    parser->position++;
    while (is_identifier_char(current_char(parser))) {
        parser->position++;
    }

    *out = copy_range(parser->input + start, parser->position - start);
    if (*out == NULL) {
        set_parse_error(parser, "out of memory");
        return false;
    }

    return true;
}

/**
 * @brief 값 배열에 항목을 하나 추가한다.
 * @param result 파싱 결과
 * @param value 추가할 값
 * @return 성공하면 1, 실패하면 0
 */
bool append_value(ParseResult *result, SqlValue value) {
    SqlValue *next_values = realloc(result->values, sizeof(SqlValue) * (result->value_count + 1));

    if (next_values == NULL) {
        return false;
    }

    result->values = next_values;
    result->values[result->value_count++] = value;
    return true;
}
