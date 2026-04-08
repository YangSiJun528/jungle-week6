#include "parser_internal.h"
#include <stdlib.h>
#include <string.h>

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
        if (current_char(&parser) != ';') {
            set_parse_error(&parser, "expected ;");
        } else {
            parser.position++;
            consume_ws(&parser);
        }
    }

    if (parser.error_message == NULL) {
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
