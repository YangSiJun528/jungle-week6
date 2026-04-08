#include "parser_internal.h"
#include <stdlib.h>

/**
 * @brief select 쿼리를 파싱한다.
 * @param parser 파서 상태
 * @param result 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
bool parse_select_query(Parser *parser, ParseResult *result) {
    if (!consume_literal(parser, "select")) {
        return false;
    }
    if (!consume_req_ws(parser)) {
        return false;
    }
    if (!consume_literal(parser, "*")) {
        return false;
    }
    if (!consume_req_ws(parser)) {
        return false;
    }
    if (!consume_literal(parser, "from")) {
        return false;
    }
    if (!consume_req_ws(parser)) {
        return false;
    }
    if (!parse_identifier(parser, &result->table_name)) {
        return false;
    }

    result->type = QUERY_TYPE_SELECT;
    return true;
}

/**
 * @brief insert 쿼리를 파싱한다.
 * @param parser 파서 상태
 * @param result 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
bool parse_insert_query(Parser *parser, ParseResult *result) {
    if (!consume_literal(parser, "insert")) {
        return false;
    }
    if (!consume_req_ws(parser)) {
        return false;
    }
    if (!consume_literal(parser, "into")) {
        return false;
    }
    if (!consume_req_ws(parser)) {
        return false;
    }
    if (!parse_identifier(parser, &result->table_name)) {
        return false;
    }
    if (!consume_req_ws(parser)) {
        return false;
    }
    if (!consume_literal(parser, "values")) {
        return false;
    }
    consume_ws(parser);
    if (current_char(parser) != '(') {
        set_parse_error(parser, "expected (");
        return false;
    }
    parser->position++;
    consume_ws(parser);
    if (!parse_value_list(parser, result)) {
        return false;
    }
    consume_ws(parser);
    if (current_char(parser) != ')') {
        set_parse_error(parser, "expected )");
        return false;
    }
    parser->position++;

    result->type = QUERY_TYPE_INSERT;
    return true;
}

/**
 * @brief insert용 값 목록을 파싱한다.
 * @param parser 파서 상태
 * @param result 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
bool parse_value_list(Parser *parser, ParseResult *result) {
    SqlValue value;

    if (!parse_value(parser, &value)) {
        return false;
    }
    if (!append_value(result, value)) {
        free(value.text);
        set_parse_error(parser, "out of memory");
        return false;
    }

    while (1) {
        consume_ws(parser);
        if (current_char(parser) != ',') {
            break;
        }

        parser->position++;
        consume_ws(parser);

        if (!parse_value(parser, &value)) {
            return false;
        }
        if (!append_value(result, value)) {
            free(value.text);
            set_parse_error(parser, "out of memory");
            return false;
        }
    }

    return true;
}
