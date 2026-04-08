#ifndef HAEGEON_PARSE_H
#define HAEGEON_PARSE_H

enum {
    STATEMENT_TABLE_NAME_SIZE = 64,
    PARSED_VALUE_TEXT_SIZE = 256,
    STATEMENT_MAX_VALUES = 16
};

typedef enum {
    STATEMENT_SELECT,
    STATEMENT_INSERT
} StatementType;

typedef enum {
    VALUE_STRING,
    VALUE_INTEGER
} ValueType;

typedef struct {
    ValueType type;
    char text[PARSED_VALUE_TEXT_SIZE];
} ParsedValue;

typedef struct {
    StatementType type;
    char table_name[STATEMENT_TABLE_NAME_SIZE];
    int value_count;
    ParsedValue values[STATEMENT_MAX_VALUES];
} Statement;

typedef enum {
    PARSE_OK = 0,
    PARSE_ERROR_NULL_INPUT,
    PARSE_ERROR_NULL_OUTPUT,
    PARSE_ERROR_EMPTY_INPUT,
    PARSE_ERROR_UNKNOWN_STATEMENT,
    PARSE_ERROR_MISSING_SEMICOLON,
    PARSE_ERROR_INVALID_SELECT_TARGET,
    PARSE_ERROR_EXPECTED_FROM,
    PARSE_ERROR_EXPECTED_INTO,
    PARSE_ERROR_EXPECTED_VALUES,
    PARSE_ERROR_INVALID_TABLE_NAME,
    PARSE_ERROR_EXPECTED_OPEN_PAREN,
    PARSE_ERROR_EXPECTED_CLOSE_PAREN,
    PARSE_ERROR_EMPTY_VALUE_LIST,
    PARSE_ERROR_INVALID_VALUE,
    PARSE_ERROR_UNTERMINATED_STRING,
    PARSE_ERROR_EXPECTED_COMMA_OR_CLOSE_PAREN,
    PARSE_ERROR_TOO_MANY_VALUES,
    PARSE_ERROR_IDENTIFIER_TOO_LONG,
    PARSE_ERROR_VALUE_TOO_LONG,
    PARSE_ERROR_TRAILING_TOKENS
} ParseResult;

/**
 * @brief SQL 문자열 하나를 Statement 구조체로 해석한다.
 * @param sql 해석할 SQL 문자열이다.
 * @param out_statement 해석 결과를 저장할 구조체 포인터다.
 * @return 성공 시 PARSE_OK, 실패 시 PARSE_ERROR_* 를 반환한다.
 */
int parse(const char *sql, Statement *out_statement);

#endif
