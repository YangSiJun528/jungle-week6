#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "utils.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *input;
    size_t position;
    const char *error_message;
    size_t error_position;
} Parser;

/*************************************************************************
 * Forward Declarations
 *************************************************************************/

/* 쿼리 상위 흐름 */
static bool parse_select_query(Parser *parser, ParseResult *result);
static bool parse_insert_query(Parser *parser, ParseResult *result);
static bool parse_value_list(Parser *parser, ParseResult *result);

/* 값 파싱 단계 */
static bool parse_value(Parser *parser, SqlValue *out);
static bool parse_number(Parser *parser, SqlValue *out);
static bool parse_string(Parser *parser, SqlValue *out);
static bool parse_identifier(Parser *parser, char **out);
static bool append_value(ParseResult *result, SqlValue value);

/* 메타데이터 로딩/검증 */
static bool init_table_metadata(
    TableMetadata *table,
    const char *table_name,
    const char *data_file_path,
    const char *const *column_names,
    const ColumnType *column_types,
    size_t column_count
);
static void free_table_metadata(TableMetadata *table);
static bool duplicate_text(char **out, const char *text);
static bool is_value_compatible(ColumnType column_type, ValueType value_type);

/* 파서 최하위 유틸리티 */
static void consume_ws(Parser *parser);
static bool consume_req_ws(Parser *parser);
static bool consume_literal(Parser *parser, const char *literal);
static char *copy_range(const char *start, size_t length);
static char current_char(const Parser *parser);
static bool is_ascii_letter(char ch);
static bool is_ascii_digit(char ch);
static bool is_sql_whitespace(char ch);
static bool is_identifier_start(char ch);
static bool is_identifier_char(char ch);
static bool is_string_char(char ch);
static void set_parse_error(Parser *parser, const char *message);

/*************************************************************************
 * Level 1: Program Flow
 *************************************************************************/

#ifndef SIJUN_TEST_BUILD
int main(void) {
    return run_repl(stdin, stdout, stderr);
}
#endif

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
    MetadataLoadResult metadata_result = load_metadata();

    if (metadata_result.error_message != NULL) {
        fprintf(error_stream, "Metadata error: %s\n", metadata_result.error_message);
        return ERR;
    }

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

        {
            ParseResult parse_result = parse(line);

            if (parse_result.error_message == NULL) {
                SemanticCheckResult check = validate_query_against_metadata(&metadata_result.metadata, &parse_result);

                if (!check.ok) {
                    fprintf(error_stream, "Semantic error: %s\n", check.error_message);
                    free_parse_result(&parse_result);
                    continue;
                }
            }

            free_parse_result(&parse_result);
        }

        fprintf(output_stream, "%s\n", action.message);
    }

    free_metadata(&metadata_result.metadata);
    free(line);
    return OK;
}

/*************************************************************************
 * Level 2: Line Interpretation
 *************************************************************************/

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

/*************************************************************************
 * Level 1: SQL Parse Entry
 *************************************************************************/

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
 * @brief 실행에 필요한 메타데이터를 로드한다.
 * @return 메타데이터 로딩 결과
 */
MetadataLoadResult load_metadata(void) {
    static const char *const users_column_names[] = {"id", "name", "role"};
    static const ColumnType users_column_types[] = {COLUMN_TYPE_NUMBER, COLUMN_TYPE_TEXT, COLUMN_TYPE_TEXT};
    static const char *const logs_column_names[] = {"message", "level", "created_at"};
    static const ColumnType logs_column_types[] = {COLUMN_TYPE_TEXT, COLUMN_TYPE_TEXT, COLUMN_TYPE_NUMBER};
    MetadataLoadResult result;

    result.metadata.tables = calloc(2, sizeof(TableMetadata));
    result.metadata.table_count = 0;
    result.error_message = NULL;

    if (result.metadata.tables == NULL) {
        result.error_message = "out of memory";
        return result;
    }

    if (!init_table_metadata(&result.metadata.tables[0], "users", "users.csv", users_column_names, users_column_types, 3)) {
        result.error_message = "out of memory";
        free_metadata(&result.metadata);
        return result;
    }
    result.metadata.table_count = 1;

    if (!init_table_metadata(&result.metadata.tables[1], "logs", "logs.csv", logs_column_names, logs_column_types, 3)) {
        result.error_message = "out of memory";
        free_metadata(&result.metadata);
        return result;
    }

    result.metadata.table_count = 2;
    return result;
}

/**
 * @brief load_metadata()가 할당한 메모리를 해제한다.
 * @param metadata 해제할 메타데이터
 */
void free_metadata(DatabaseMetadata *metadata) {
    size_t index;

    if (metadata == NULL) {
        return;
    }

    for (index = 0; index < metadata->table_count; ++index) {
        free_table_metadata(&metadata->tables[index]);
    }

    free(metadata->tables);
    metadata->tables = NULL;
    metadata->table_count = 0;
}

/**
 * @brief 이름으로 테이블 메타데이터를 찾는다.
 * @param metadata 검색 대상 메타데이터
 * @param table_name 찾을 테이블명
 * @return 찾은 테이블 메타데이터, 없으면 NULL
 */
const TableMetadata *find_table_metadata(const DatabaseMetadata *metadata, const char *table_name) {
    size_t index;

    if (metadata == NULL || table_name == NULL) {
        return NULL;
    }

    for (index = 0; index < metadata->table_count; ++index) {
        if (strcmp(metadata->tables[index].name, table_name) == 0) {
            return &metadata->tables[index];
        }
    }

    return NULL;
}

/**
 * @brief 파싱된 쿼리를 메타데이터 기준으로 검증한다.
 * @param metadata 검증에 사용할 메타데이터
 * @param result 파싱 결과
 * @return 의미 검증 결과
 */
SemanticCheckResult validate_query_against_metadata(const DatabaseMetadata *metadata, const ParseResult *result) {
    const TableMetadata *table;
    size_t index;
    SemanticCheckResult check;

    check.ok = false;
    check.error_message = "invalid query";

    if (metadata == NULL || result == NULL || result->table_name == NULL) {
        return check;
    }

    table = find_table_metadata(metadata, result->table_name);
    if (table == NULL) {
        check.error_message = "unknown table";
        return check;
    }

    if (result->type == QUERY_TYPE_SELECT) {
        check.ok = true;
        check.error_message = NULL;
        return check;
    }

    if (result->type != QUERY_TYPE_INSERT) {
        return check;
    }

    if (result->value_count != table->column_count) {
        check.error_message = "column count does not match value count";
        return check;
    }

    for (index = 0; index < result->value_count; ++index) {
        if (!is_value_compatible(table->columns[index].type, result->values[index].type)) {
            check.error_message = "value type does not match column type";
            return check;
        }
    }

    check.ok = true;
    check.error_message = NULL;
    return check;
}

/*************************************************************************
 * Level 2: Query Parsing
 *************************************************************************/

/**
 * @brief select 쿼리를 파싱한다.
 * @param parser 파서 상태
 * @param result 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
static bool parse_select_query(Parser *parser, ParseResult *result) {
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
static bool parse_insert_query(Parser *parser, ParseResult *result) {
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
static bool parse_value_list(Parser *parser, ParseResult *result) {
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

/*************************************************************************
 * Level 3: Value Parsing
 *************************************************************************/

/**
 * @brief 값 하나를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
static bool parse_value(Parser *parser, SqlValue *out) {
    char ch = current_char(parser);

    out->text = NULL;

    if (is_ascii_digit(ch)) {
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
    return false;
}

/**
 * @brief 숫자를 파싱한다.
 * @param parser 파서 상태
 * @param out 결과 값
 * @return 성공하면 1, 실패하면 0
 */
static bool parse_number(Parser *parser, SqlValue *out) {
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
static bool parse_string(Parser *parser, SqlValue *out) {
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
static bool parse_identifier(Parser *parser, char **out) {
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
static bool append_value(ParseResult *result, SqlValue value) {
    SqlValue *next_values = realloc(result->values, sizeof(SqlValue) * (result->value_count + 1));

    if (next_values == NULL) {
        return false;
    }

    result->values = next_values;
    result->values[result->value_count++] = value;
    return true;
}

/**
 * @brief 하드코딩된 테이블 메타데이터 하나를 초기화한다.
 * @param table 초기화할 테이블 메타데이터
 * @param table_name 테이블명
 * @param data_file_path 데이터 파일 경로
 * @param column_names 컬럼명 배열
 * @param column_types 컬럼 타입 배열
 * @param column_count 컬럼 개수
 * @return 성공하면 1, 실패하면 0
 */
static bool init_table_metadata(
    TableMetadata *table,
    const char *table_name,
    const char *data_file_path,
    const char *const *column_names,
    const ColumnType *column_types,
    size_t column_count
) {
    size_t index;

    table->name = NULL;
    table->data_file_path = NULL;
    table->columns = NULL;
    table->column_count = 0;

    if (!duplicate_text(&table->name, table_name)) {
        return false;
    }

    if (!duplicate_text(&table->data_file_path, data_file_path)) {
        free_table_metadata(table);
        return false;
    }

    table->columns = calloc(column_count, sizeof(ColumnMetadata));
    if (table->columns == NULL) {
        free_table_metadata(table);
        return false;
    }

    for (index = 0; index < column_count; ++index) {
        if (!duplicate_text(&table->columns[index].name, column_names[index])) {
            table->column_count = column_count;
            free_table_metadata(table);
            return false;
        }
        table->columns[index].type = column_types[index];
    }

    table->column_count = column_count;
    return true;
}

/**
 * @brief 테이블 메타데이터 하나가 가진 메모리를 해제한다.
 * @param table 해제할 테이블 메타데이터
 */
static void free_table_metadata(TableMetadata *table) {
    size_t index;

    if (table == NULL) {
        return;
    }

    for (index = 0; index < table->column_count; ++index) {
        free(table->columns[index].name);
        table->columns[index].name = NULL;
    }

    free(table->columns);
    free(table->name);
    free(table->data_file_path);
    table->columns = NULL;
    table->name = NULL;
    table->data_file_path = NULL;
    table->column_count = 0;
}

/**
 * @brief 문자열을 heap에 복사한다.
 * @param out 복사본을 받을 포인터
 * @param text 복사할 문자열
 * @return 성공하면 1, 실패하면 0
 */
static bool duplicate_text(char **out, const char *text) {
    *out = copy_range(text, strlen(text));
    return *out != NULL;
}

/**
 * @brief 컬럼 타입과 SQL 값 타입이 호환되는지 검사한다.
 * @param column_type 컬럼 타입
 * @param value_type SQL 값 타입
 * @return 호환되면 1, 아니면 0
 */
static bool is_value_compatible(ColumnType column_type, ValueType value_type) {
    if (column_type == COLUMN_TYPE_NUMBER) {
        return value_type == VALUE_TYPE_NUMBER;
    }

    return value_type == VALUE_TYPE_STRING || value_type == VALUE_TYPE_IDENTIFIER;
}

/*************************************************************************
 * Level 4: Parser Primitives
 *************************************************************************/

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
static bool consume_req_ws(Parser *parser) {
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
static bool consume_literal(Parser *parser, const char *literal) {
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
 * @brief 현재 문자를 반환한다.
 * @param parser 파서 상태
 * @return 현재 문자
 */
static char current_char(const Parser *parser) {
    return parser->input[parser->position];
}

/**
 * @brief ASCII 영문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 영문자면 1, 아니면 0
 */
static bool is_ascii_letter(char ch) {
    return ('a' <= ch && ch <= 'z') || ('A' <= ch && ch <= 'Z');
}

/**
 * @brief ASCII 숫자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 숫자면 1, 아니면 0
 */
static bool is_ascii_digit(char ch) {
    return '0' <= ch && ch <= '9';
}

/**
 * @brief 공백 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 공백이면 1, 아니면 0
 */
static bool is_sql_whitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n';
}

/**
 * @brief 식별자 시작 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 문자면 1, 아니면 0
 */
static bool is_identifier_start(char ch) {
    return is_ascii_letter(ch);
}

/**
 * @brief 식별자 본문 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 허용 문자면 1, 아니면 0
 */
static bool is_identifier_char(char ch) {
    return is_ascii_letter(ch) || is_ascii_digit(ch) || ch == '_';
}

/**
 * @brief 문자열 내부 허용 문자 여부를 검사한다.
 * @param ch 검사할 문자
 * @return 허용 문자면 1, 아니면 0
 */
static bool is_string_char(char ch) {
    return is_identifier_char(ch) || ch == ' ';
}

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
