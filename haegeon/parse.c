#include "parse.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 현재 커서를 다음 non-space 문자까지 이동시킨다.
 * @param cursor 이동할 커서의 주소다.
 * @return 반환값은 없다.
 */
static void skip_spaces(const char **cursor) {
    while (**cursor != '\0' && isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }
}

/**
 * @brief 현재 위치가 주어진 키워드와 정확히 일치하는지 확인한다.
 * @param cursor 확인할 커서의 주소다.
 * @param keyword 비교할 키워드 문자열이다.
 * @return 일치하면 1, 일치하지 않으면 0을 반환한다.
 */
static int match_keyword(const char **cursor, const char *keyword) {
    const char *start = *cursor;
    size_t index = 0;

    while (keyword[index] != '\0') {
        if (start[index] == '\0') {
            return 0;
        }

        if (tolower((unsigned char)start[index]) !=
            tolower((unsigned char)keyword[index])) {
            return 0;
        }

        index++;
    }

    if (isalnum((unsigned char)start[index]) || start[index] == '_') {
        return 0;
    }

    *cursor = start + index;
    return 1;
}

/**
 * @brief 현재 위치에서 유효한 식별자를 읽어 버퍼에 저장한다.
 * @param cursor 읽을 커서의 주소다.
 * @param buffer 식별자를 저장할 버퍼다.
 * @param buffer_size 버퍼 크기다.
 * @return 성공 시 PARSE_OK, 실패 시 관련 PARSE_ERROR_* 를 반환한다.
 */
static int read_identifier(const char **cursor, char *buffer, size_t buffer_size) {
    const char *start = *cursor;
    size_t length = 0;

    if (!(isalpha((unsigned char)*start) || *start == '_')) {
        return PARSE_ERROR_INVALID_TABLE_NAME;
    }

    while (isalnum((unsigned char)*start) || *start == '_') {
        if (length + 1 >= buffer_size) {
            return PARSE_ERROR_IDENTIFIER_TOO_LONG;
        }

        buffer[length++] = *start;
        start++;
    }

    buffer[length] = '\0';
    *cursor = start;

    return PARSE_OK;
}

/**
 * @brief INSERT 값 하나를 읽어 ParsedValue 구조체에 저장한다.
 * @param cursor 읽을 커서의 주소다.
 * @param out_value 해석 결과를 저장할 값 구조체다.
 * @return 성공 시 PARSE_OK, 실패 시 관련 PARSE_ERROR_* 를 반환한다.
 */
static int parse_insert_value(const char **cursor, ParsedValue *out_value) {
    const char *start = *cursor;
    size_t length = 0;

    if (*start == '\'') {
        start++;

        while (*start != '\0' && *start != '\'') {
            if (length + 1 >= sizeof(out_value->text)) {
                return PARSE_ERROR_VALUE_TOO_LONG;
            }

            out_value->text[length++] = *start;
            start++;
        }

        if (*start != '\'') {
            return PARSE_ERROR_UNTERMINATED_STRING;
        }

        out_value->text[length] = '\0';
        out_value->type = VALUE_STRING;
        *cursor = start + 1;
        return PARSE_OK;
    }

    if (isdigit((unsigned char)*start)) {
        while (isdigit((unsigned char)*start)) {
            if (length + 1 >= sizeof(out_value->text)) {
                return PARSE_ERROR_VALUE_TOO_LONG;
            }

            out_value->text[length++] = *start;
            start++;
        }

        out_value->text[length] = '\0';
        out_value->type = VALUE_INTEGER;
        *cursor = start;
        return PARSE_OK;
    }

    return PARSE_ERROR_INVALID_VALUE;
}

/**
 * @brief 문장 끝의 세미콜론과 뒤쪽 공백 규칙을 검증한다.
 * @param cursor 확인할 커서의 주소다.
 * @return 성공 시 PARSE_OK, 실패 시 관련 PARSE_ERROR_* 를 반환한다.
 */
static int finish_with_semicolon(const char **cursor) {
    skip_spaces(cursor);

    if (**cursor != ';') {
        return PARSE_ERROR_MISSING_SEMICOLON;
    }

    (*cursor)++;
    skip_spaces(cursor);

    if (**cursor != '\0') {
        return PARSE_ERROR_TRAILING_TOKENS;
    }

    return PARSE_OK;
}

/**
 * @brief SELECT 키워드 다음부터 SELECT 문 세부 구문을 해석한다.
 * @param cursor 해석할 문자열 커서의 주소다.
 * @param out_statement 해석 결과를 기록할 구조체다.
 * @return 성공 시 PARSE_OK, 실패 시 관련 PARSE_ERROR_* 를 반환한다.
 */
static int parse_select_statement_details(const char **cursor, Statement *out_statement) {
    int result;

    skip_spaces(cursor);

    if (**cursor != '*') {
        return PARSE_ERROR_INVALID_SELECT_TARGET;
    }

    (*cursor)++;
    skip_spaces(cursor);

    if (!match_keyword(cursor, "from")) {
        return PARSE_ERROR_EXPECTED_FROM;
    }

    skip_spaces(cursor);
    result = read_identifier(cursor, out_statement->table_name,
                             sizeof(out_statement->table_name));
    if (result != PARSE_OK) {
        return result;
    }

    result = finish_with_semicolon(cursor);
    if (result != PARSE_OK) {
        return result;
    }

    out_statement->type = STATEMENT_SELECT;
    out_statement->value_count = 0;
    return PARSE_OK;
}

/**
 * @brief INSERT 키워드 다음부터 INSERT 문 세부 구문을 해석한다.
 * @param cursor 해석할 문자열 커서의 주소다.
 * @param out_statement 해석 결과를 기록할 구조체다.
 * @return 성공 시 PARSE_OK, 실패 시 관련 PARSE_ERROR_* 를 반환한다.
 */
static int parse_insert_statement_details(const char **cursor, Statement *out_statement) {
    int result;

    skip_spaces(cursor);

    if (!match_keyword(cursor, "into")) {
        return PARSE_ERROR_EXPECTED_INTO;
    }

    skip_spaces(cursor);
    result = read_identifier(cursor, out_statement->table_name,
                             sizeof(out_statement->table_name));
    if (result != PARSE_OK) {
        return result;
    }

    skip_spaces(cursor);

    if (!match_keyword(cursor, "values")) {
        return PARSE_ERROR_EXPECTED_VALUES;
    }

    skip_spaces(cursor);

    if (**cursor != '(') {
        return PARSE_ERROR_EXPECTED_OPEN_PAREN;
    }

    (*cursor)++;
    skip_spaces(cursor);

    if (**cursor == ')') {
        return PARSE_ERROR_EMPTY_VALUE_LIST;
    }

    out_statement->value_count = 0;

    while (1) {
        if (out_statement->value_count >= STATEMENT_MAX_VALUES) {
            return PARSE_ERROR_TOO_MANY_VALUES;
        }

        result = parse_insert_value(cursor,
                                    &out_statement->values[out_statement->value_count]);
        if (result != PARSE_OK) {
            return result;
        }

        out_statement->value_count++;
        skip_spaces(cursor);

        if (**cursor == ',') {
            (*cursor)++;
            skip_spaces(cursor);

            if (**cursor == '\0') {
                return PARSE_ERROR_EXPECTED_CLOSE_PAREN;
            }

            continue;
        }

        if (**cursor == ')') {
            (*cursor)++;
            break;
        }

        if (**cursor == '\0') {
            return PARSE_ERROR_EXPECTED_CLOSE_PAREN;
        }

        return PARSE_ERROR_EXPECTED_COMMA_OR_CLOSE_PAREN;
    }

    result = finish_with_semicolon(cursor);
    if (result != PARSE_OK) {
        return result;
    }

    out_statement->type = STATEMENT_INSERT;
    return PARSE_OK;
}

/**
 * @brief SQL 문자열 하나를 Statement 구조체로 해석한다.
 * @param sql 해석할 SQL 문자열이다.
 * @param out_statement 해석 결과를 저장할 구조체 포인터다.
 * @return 성공 시 PARSE_OK, 실패 시 PARSE_ERROR_* 를 반환한다.
 */
int parse(const char *sql, Statement *out_statement) {
    const char *cursor;
    Statement temporary_statement;
    int result;

    if (sql == NULL) {
        return PARSE_ERROR_NULL_INPUT;
    }

    if (out_statement == NULL) {
        return PARSE_ERROR_NULL_OUTPUT;
    }

    memset(&temporary_statement, 0, sizeof(temporary_statement));
    cursor = sql;
    skip_spaces(&cursor);

    if (*cursor == '\0') {
        return PARSE_ERROR_EMPTY_INPUT;
    }

    if (match_keyword(&cursor, "select")) {
        result = parse_select_statement_details(&cursor, &temporary_statement);
    } else if (match_keyword(&cursor, "insert")) {
        result = parse_insert_statement_details(&cursor, &temporary_statement);
    } else {
        return PARSE_ERROR_UNKNOWN_STATEMENT;
    }

    if (result != PARSE_OK) {
        return result;
    }

    *out_statement = temporary_statement;
    return PARSE_OK;
}
