#include "parse.h"

#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(array) ((int)(sizeof(array) / sizeof((array)[0])))

static int g_failed_tests = 0;
static int g_current_test_failed = 0;

#define ASSERT_INT_EQ(expected, actual)                                              \
    do {                                                                             \
        if ((expected) != (actual)) {                                                \
            fprintf(stderr, "assertion failed at %s:%d: expected %d, got %d\n",      \
                    __FILE__, __LINE__, (expected), (actual));                       \
            g_current_test_failed = 1;                                               \
            return;                                                                  \
        }                                                                            \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                                              \
    do {                                                                             \
        if (strcmp((expected), (actual)) != 0) {                                     \
            fprintf(stderr, "assertion failed at %s:%d: expected \"%s\", got \"%s\"\n", \
                    __FILE__, __LINE__, (expected), (actual));                       \
            g_current_test_failed = 1;                                               \
            return;                                                                  \
        }                                                                            \
    } while (0)

/**
 * @brief 테스트 하나를 실행하고 실패 여부를 집계한다.
 * @param test_name 실행할 테스트 이름이다.
 * @param test_function 실행할 테스트 함수다.
 * @return 반환값은 없다.
 */
static void run_test(const char *test_name, void (*test_function)(void)) {
    g_current_test_failed = 0;

    test_function();

    if (g_current_test_failed == 0) {
        printf("[PASS] %s\n", test_name);
    } else {
        printf("[FAIL] %s\n", test_name);
        g_failed_tests++;
    }
}

/**
 * @brief SELECT 파싱 성공 결과를 검증한다.
 * @param statement 검증할 Statement 구조체다.
 * @param expected_table 기대하는 테이블 이름이다.
 * @return 반환값은 없다.
 */
static void assert_select_statement(const Statement *statement, const char *expected_table) {
    ASSERT_INT_EQ(STATEMENT_SELECT, statement->type);
    ASSERT_STR_EQ(expected_table, statement->table_name);
    ASSERT_INT_EQ(0, statement->value_count);
}

/**
 * @brief INSERT 파싱 성공 결과를 검증한다.
 * @param statement 검증할 Statement 구조체다.
 * @param expected_table 기대하는 테이블 이름이다.
 * @param expected_count 기대하는 값 개수다.
 * @return 반환값은 없다.
 */
static void assert_insert_statement(const Statement *statement,
                                    const char *expected_table,
                                    int expected_count) {
    ASSERT_INT_EQ(STATEMENT_INSERT, statement->type);
    ASSERT_STR_EQ(expected_table, statement->table_name);
    ASSERT_INT_EQ(expected_count, statement->value_count);
}

/**
 * @brief SELECT 소문자 키워드는 정상 파싱되어야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_select_with_lowercase_keywords(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "select * from users;";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_OK, result);
    assert_select_statement(&statement, "users");
}

/**
 * @brief SELECT 대문자 키워드는 정상 파싱되어야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_select_with_uppercase_keywords(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "SELECT * FROM users;";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_OK, result);
    assert_select_statement(&statement, "users");
}

/**
 * @brief 여러 공백이 있어도 SELECT는 정상 파싱되어야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_select_with_extra_spaces(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = " \t select   *\nfrom   users ; \n";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_OK, result);
    assert_select_statement(&statement, "users");
}

/**
 * @brief 문자열과 숫자가 섞인 INSERT는 정상 파싱되어야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_with_string_and_integer_values(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users values ('kim', 1, 'db2');";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_OK, result);
    assert_insert_statement(&statement, "users", 3);
    ASSERT_INT_EQ(VALUE_STRING, statement.values[0].type);
    ASSERT_STR_EQ("kim", statement.values[0].text);
    ASSERT_INT_EQ(VALUE_INTEGER, statement.values[1].type);
    ASSERT_STR_EQ("1", statement.values[1].text);
    ASSERT_INT_EQ(VALUE_STRING, statement.values[2].type);
    ASSERT_STR_EQ("db2", statement.values[2].text);
}

/**
 * @brief 한글 문자열은 UTF-8 바이트 그대로 보존되어야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_with_korean_strings(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users values ('덕배', '감기');";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_OK, result);
    assert_insert_statement(&statement, "users", 2);
    ASSERT_STR_EQ("덕배", statement.values[0].text);
    ASSERT_STR_EQ("감기", statement.values[1].text);
}

/**
 * @brief values 뒤에 공백이 없어도 INSERT는 정상 파싱되어야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_without_space_after_parenthesis(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users values(1,'kim');";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_OK, result);
    assert_insert_statement(&statement, "users", 2);
    ASSERT_INT_EQ(VALUE_INTEGER, statement.values[0].type);
    ASSERT_STR_EQ("1", statement.values[0].text);
    ASSERT_INT_EQ(VALUE_STRING, statement.values[1].type);
    ASSERT_STR_EQ("kim", statement.values[1].text);
}

/**
 * @brief 입력 포인터가 NULL이면 NULL 입력 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_input_is_null(void) {
    Statement statement = {0};
    int result;

    /* given */

    /* when */
    result = parse(NULL, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_NULL_INPUT, result);
}

/**
 * @brief 출력 포인터가 NULL이면 NULL 출력 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_output_is_null(void) {
    int result;

    /* given */
    const char *sql = "select * from users;";

    /* when */
    result = parse(sql, NULL);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_NULL_OUTPUT, result);
}

/**
 * @brief 빈 입력은 EMPTY_INPUT 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_input_is_empty(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = " \t \n ";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_EMPTY_INPUT, result);
}

/**
 * @brief 지원하지 않는 문장은 UNKNOWN_STATEMENT 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_for_unknown_statement(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "delete from users;";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_UNKNOWN_STATEMENT, result);
}

/**
 * @brief SELECT 대상이 *가 아니면 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_select_fails_when_target_is_not_star(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "select name from users;";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_INVALID_SELECT_TARGET, result);
}

/**
 * @brief FROM 키워드가 없으면 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_select_fails_when_from_is_missing(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "select * users;";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_EXPECTED_FROM, result);
}

/**
 * @brief 세미콜론이 없으면 MISSING_SEMICOLON 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_semicolon_is_missing(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "select * from users";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_MISSING_SEMICOLON, result);
}

/**
 * @brief INTO 키워드가 없으면 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_fails_when_into_is_missing(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert users values ('kim');";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_EXPECTED_INTO, result);
}

/**
 * @brief VALUES 키워드가 없으면 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_fails_when_values_keyword_is_missing(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users ('kim');";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_EXPECTED_VALUES, result);
}

/**
 * @brief 값 목록이 비어 있으면 EMPTY_VALUE_LIST 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_fails_when_value_list_is_empty(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users values ();";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_EMPTY_VALUE_LIST, result);
}

/**
 * @brief 문자열이 닫히지 않으면 UNTERMINATED_STRING 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_fails_for_unterminated_string(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users values ('kim);";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_UNTERMINATED_STRING, result);
}

/**
 * @brief 값 사이 콤마가 없으면 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_fails_for_missing_comma(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users values ('kim' 1);";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_EXPECTED_COMMA_OR_CLOSE_PAREN, result);
}

/**
 * @brief 마지막 콤마 뒤 값이 없으면 INVALID_VALUE 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_insert_fails_for_trailing_comma(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "insert into users values ('kim',);";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_INVALID_VALUE, result);
}

/**
 * @brief 세미콜론 뒤 토큰이 있으면 TRAILING_TOKENS 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_tokens_exist_after_semicolon(void) {
    Statement statement = {0};
    int result;

    /* given */
    const char *sql = "select * from users; extra";

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_TRAILING_TOKENS, result);
}

/**
 * @brief 테이블 이름이 너무 길면 IDENTIFIER_TOO_LONG 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_table_name_is_too_long(void) {
    Statement statement = {0};
    char sql[160];
    char table_name[STATEMENT_TABLE_NAME_SIZE + 1];
    int index;
    int result;

    /* given */
    for (index = 0; index < STATEMENT_TABLE_NAME_SIZE; index++) {
        table_name[index] = 'a';
    }
    table_name[STATEMENT_TABLE_NAME_SIZE] = '\0';
    snprintf(sql, sizeof(sql), "select * from %s;", table_name);

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_IDENTIFIER_TOO_LONG, result);
}

/**
 * @brief 값 문자열이 너무 길면 VALUE_TOO_LONG 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_value_text_is_too_long(void) {
    Statement statement = {0};
    char sql[400];
    int offset = 0;
    int index;
    int result;

    /* given */
    offset += snprintf(sql + offset, sizeof(sql) - (size_t)offset,
                       "insert into users values ('");
    for (index = 0; index < PARSED_VALUE_TEXT_SIZE; index++) {
        sql[offset++] = 'a';
    }
    sql[offset++] = '\'';
    sql[offset++] = ')';
    sql[offset++] = ';';
    sql[offset] = '\0';

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_VALUE_TOO_LONG, result);
}

/**
 * @brief 값 개수가 제한을 넘으면 TOO_MANY_VALUES 오류를 반환해야 한다.
 * @param 없음
 * @return 반환값은 없다.
 */
static void parse_fails_when_value_count_exceeds_limit(void) {
    Statement statement = {0};
    char sql[512];
    int offset = 0;
    int index;
    int result;

    /* given */
    offset += snprintf(sql + offset, sizeof(sql) - (size_t)offset,
                       "insert into users values (");
    for (index = 0; index < STATEMENT_MAX_VALUES + 1; index++) {
        offset += snprintf(sql + offset, sizeof(sql) - (size_t)offset,
                           "%s%d", index == 0 ? "" : ", ", index);
    }
    offset += snprintf(sql + offset, sizeof(sql) - (size_t)offset, ");");

    /* when */
    result = parse(sql, &statement);

    /* then */
    ASSERT_INT_EQ(PARSE_ERROR_TOO_MANY_VALUES, result);
}

int main(void) {
    void (*tests[])(void) = {
        parse_select_with_lowercase_keywords,
        parse_select_with_uppercase_keywords,
        parse_select_with_extra_spaces,
        parse_insert_with_string_and_integer_values,
        parse_insert_with_korean_strings,
        parse_insert_without_space_after_parenthesis,
        parse_fails_when_input_is_null,
        parse_fails_when_output_is_null,
        parse_fails_when_input_is_empty,
        parse_fails_for_unknown_statement,
        parse_select_fails_when_target_is_not_star,
        parse_select_fails_when_from_is_missing,
        parse_fails_when_semicolon_is_missing,
        parse_insert_fails_when_into_is_missing,
        parse_insert_fails_when_values_keyword_is_missing,
        parse_insert_fails_when_value_list_is_empty,
        parse_insert_fails_for_unterminated_string,
        parse_insert_fails_for_missing_comma,
        parse_insert_fails_for_trailing_comma,
        parse_fails_when_tokens_exist_after_semicolon,
        parse_fails_when_table_name_is_too_long,
        parse_fails_when_value_text_is_too_long,
        parse_fails_when_value_count_exceeds_limit
    };
    const char *test_names[] = {
        "parse_select_with_lowercase_keywords",
        "parse_select_with_uppercase_keywords",
        "parse_select_with_extra_spaces",
        "parse_insert_with_string_and_integer_values",
        "parse_insert_with_korean_strings",
        "parse_insert_without_space_after_parenthesis",
        "parse_fails_when_input_is_null",
        "parse_fails_when_output_is_null",
        "parse_fails_when_input_is_empty",
        "parse_fails_for_unknown_statement",
        "parse_select_fails_when_target_is_not_star",
        "parse_select_fails_when_from_is_missing",
        "parse_fails_when_semicolon_is_missing",
        "parse_insert_fails_when_into_is_missing",
        "parse_insert_fails_when_values_keyword_is_missing",
        "parse_insert_fails_when_value_list_is_empty",
        "parse_insert_fails_for_unterminated_string",
        "parse_insert_fails_for_missing_comma",
        "parse_insert_fails_for_trailing_comma",
        "parse_fails_when_tokens_exist_after_semicolon",
        "parse_fails_when_table_name_is_too_long",
        "parse_fails_when_value_text_is_too_long",
        "parse_fails_when_value_count_exceeds_limit"
    };
    int index;

    for (index = 0; index < ARRAY_SIZE(tests); index++) {
        run_test(test_names[index], tests[index]);
    }

    if (g_failed_tests != 0) {
        fprintf(stderr, "%d test(s) failed.\n", g_failed_tests);
        return 1;
    }

    printf("all parse tests passed.\n");
    return 0;
}
