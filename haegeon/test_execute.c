#include "cli.h"
#include "execute.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARRAY_SIZE(array) ((int)(sizeof(array) / sizeof((array)[0])))

enum {
    TEST_BUFFER_SIZE = 8192,
    TEST_PATH_SIZE = 256
};

static int g_failed_tests = 0;
static int g_current_test_failed = 0;

#define ASSERT_TRUE(condition)                                                      \
    do {                                                                           \
        if (!(condition)) {                                                        \
            fprintf(stderr, "assertion failed at %s:%d: %s\n",                     \
                    __FILE__, __LINE__, #condition);                               \
            g_current_test_failed = 1;                                             \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_INT_EQ(expected, actual)                                            \
    do {                                                                           \
        if ((expected) != (actual)) {                                              \
            fprintf(stderr, "assertion failed at %s:%d: expected %d, got %d\n",    \
                    __FILE__, __LINE__, (expected), (actual));                     \
            g_current_test_failed = 1;                                             \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_STR_EQ(expected, actual)                                            \
    do {                                                                           \
        if (strcmp((expected), (actual)) != 0) {                                   \
            fprintf(stderr, "assertion failed at %s:%d: expected \"%s\", got \"%s\"\n", \
                    __FILE__, __LINE__, (expected), (actual));                     \
            g_current_test_failed = 1;                                             \
            return;                                                                \
        }                                                                          \
    } while (0)

#define ASSERT_STR_CONTAINS(haystack, needle)                                      \
    do {                                                                           \
        if (strstr((haystack), (needle)) == NULL) {                                \
            fprintf(stderr, "assertion failed at %s:%d: \"%s\" not found in \"%s\"\n", \
                    __FILE__, __LINE__, (needle), (haystack));                     \
            g_current_test_failed = 1;                                             \
            return;                                                                \
        }                                                                          \
    } while (0)

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

static void build_table_path(const char *db_dir,
                             const char *table_name,
                             char *buffer,
                             size_t buffer_size) {
    int written;

    written = snprintf(buffer, buffer_size, "%s/%s.table", db_dir, table_name);
    ASSERT_TRUE(written >= 0);
    ASSERT_TRUE((size_t)written < buffer_size);
}

static void create_temp_directory(char *buffer, size_t buffer_size) {
    int written;
    char *result;

    written = snprintf(buffer, buffer_size, "/tmp/haegeon-execute-XXXXXX");
    ASSERT_TRUE(written >= 0);
    ASSERT_TRUE((size_t)written < buffer_size);

    result = mkdtemp(buffer);
    ASSERT_TRUE(result != NULL);
}

static void write_table_file(const char *db_dir,
                             const char *table_name,
                             const char *contents) {
    char path[TEST_PATH_SIZE];
    FILE *file;

    build_table_path(db_dir, table_name, path, sizeof(path));

    file = fopen(path, "w");
    ASSERT_TRUE(file != NULL);

    if (contents != NULL) {
        ASSERT_TRUE(fputs(contents, file) != EOF);
    }

    ASSERT_INT_EQ(0, fclose(file));
}

static void read_text_file(const char *path, char *buffer, size_t buffer_size) {
    FILE *file;
    size_t read_size;

    file = fopen(path, "r");
    ASSERT_TRUE(file != NULL);

    read_size = fread(buffer, 1, buffer_size - 1, file);
    buffer[read_size] = '\0';

    ASSERT_TRUE(!ferror(file));
    ASSERT_INT_EQ(0, fclose(file));
}

static void read_stream(FILE *stream, char *buffer, size_t buffer_size) {
    size_t read_size;

    ASSERT_INT_EQ(0, fflush(stream));
    rewind(stream);

    read_size = fread(buffer, 1, buffer_size - 1, stream);
    buffer[read_size] = '\0';

    ASSERT_TRUE(!ferror(stream));
}

static void cleanup_table_file(const char *db_dir, const char *table_name) {
    char path[TEST_PATH_SIZE];

    build_table_path(db_dir, table_name, path, sizeof(path));
    remove(path);
}

static void initialize_select_statement(Statement *statement, const char *table_name) {
    memset(statement, 0, sizeof(*statement));
    statement->type = STATEMENT_SELECT;
    snprintf(statement->table_name, sizeof(statement->table_name), "%s", table_name);
}

static void initialize_insert_statement(Statement *statement, const char *table_name) {
    memset(statement, 0, sizeof(*statement));
    statement->type = STATEMENT_INSERT;
    snprintf(statement->table_name, sizeof(statement->table_name), "%s", table_name);
}

static void execute_insert_appends_row_to_existing_table(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char table_path[TEST_PATH_SIZE];
    char file_contents[TEST_BUFFER_SIZE];
    Statement statement;
    ExecuteResponse response = {0};
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    write_table_file(temp_dir, "users", "('lee', 2)\n");
    build_table_path(temp_dir, "users", table_path, sizeof(table_path));
    initialize_insert_statement(&statement, "users");
    statement.value_count = 2;
    statement.values[0].type = VALUE_STRING;
    snprintf(statement.values[0].text, sizeof(statement.values[0].text), "%s", "kim");
    statement.values[1].type = VALUE_INTEGER;
    snprintf(statement.values[1].text, sizeof(statement.values[1].text), "%s", "1");

    /* when */
    result = execute(&statement, temp_dir, NULL, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_OK, result);
    ASSERT_INT_EQ(1, response.rows_affected);
    ASSERT_INT_EQ(0, response.rows_returned);
    read_text_file(table_path, file_contents, sizeof(file_contents));
    ASSERT_STR_EQ("('lee', 2)\n('kim', 1)\n", file_contents);

    cleanup_table_file(temp_dir, "users");
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void execute_insert_preserves_string_and_integer_format(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char table_path[TEST_PATH_SIZE];
    char file_contents[TEST_BUFFER_SIZE];
    Statement statement;
    ExecuteResponse response = {0};
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    write_table_file(temp_dir, "users", NULL);
    build_table_path(temp_dir, "users", table_path, sizeof(table_path));
    initialize_insert_statement(&statement, "users");
    statement.value_count = 3;
    statement.values[0].type = VALUE_STRING;
    snprintf(statement.values[0].text, sizeof(statement.values[0].text), "%s", "kim");
    statement.values[1].type = VALUE_INTEGER;
    snprintf(statement.values[1].text, sizeof(statement.values[1].text), "%s", "1");
    statement.values[2].type = VALUE_STRING;
    snprintf(statement.values[2].text, sizeof(statement.values[2].text), "%s", "db2");

    /* when */
    result = execute(&statement, temp_dir, NULL, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_OK, result);
    read_text_file(table_path, file_contents, sizeof(file_contents));
    ASSERT_STR_EQ("('kim', 1, 'db2')\n", file_contents);

    cleanup_table_file(temp_dir, "users");
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void execute_select_prints_all_rows(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char output_text[TEST_BUFFER_SIZE];
    Statement statement;
    ExecuteResponse response = {0};
    FILE *output;
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    write_table_file(temp_dir, "users", "('kim', 1)\n('lee', 2)\n");
    initialize_select_statement(&statement, "users");
    output = tmpfile();
    ASSERT_TRUE(output != NULL);

    /* when */
    result = execute(&statement, temp_dir, output, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_OK, result);
    ASSERT_INT_EQ(0, response.rows_affected);
    ASSERT_INT_EQ(2, response.rows_returned);
    read_stream(output, output_text, sizeof(output_text));
    ASSERT_STR_EQ("('kim', 1)\n('lee', 2)\n", output_text);

    ASSERT_INT_EQ(0, fclose(output));
    cleanup_table_file(temp_dir, "users");
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void execute_select_returns_zero_rows_for_empty_table(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char output_text[TEST_BUFFER_SIZE];
    Statement statement;
    ExecuteResponse response = {0};
    FILE *output;
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    write_table_file(temp_dir, "users", NULL);
    initialize_select_statement(&statement, "users");
    output = tmpfile();
    ASSERT_TRUE(output != NULL);

    /* when */
    result = execute(&statement, temp_dir, output, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_OK, result);
    ASSERT_INT_EQ(0, response.rows_affected);
    ASSERT_INT_EQ(0, response.rows_returned);
    read_stream(output, output_text, sizeof(output_text));
    ASSERT_STR_EQ("", output_text);

    ASSERT_INT_EQ(0, fclose(output));
    cleanup_table_file(temp_dir, "users");
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void execute_fails_when_statement_is_null(void) {
    ExecuteResponse response = {0};
    int result;

    /* given */

    /* when */
    result = execute(NULL, "/tmp", NULL, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_ERROR_NULL_STATEMENT, result);
}

static void execute_fails_when_db_dir_is_null(void) {
    Statement statement;
    ExecuteResponse response = {0};
    int result;

    /* given */
    initialize_insert_statement(&statement, "users");

    /* when */
    result = execute(&statement, NULL, NULL, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_ERROR_NULL_DB_DIR, result);
}

static void execute_fails_when_output_is_null_for_select(void) {
    Statement statement;
    ExecuteResponse response = {0};
    int result;

    /* given */
    initialize_select_statement(&statement, "users");

    /* when */
    result = execute(&statement, "/tmp", NULL, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_ERROR_NULL_OUTPUT, result);
}

static void execute_fails_when_response_is_null(void) {
    Statement statement;
    int result;

    /* given */
    initialize_insert_statement(&statement, "users");

    /* when */
    result = execute(&statement, "/tmp", NULL, NULL);

    /* then */
    ASSERT_INT_EQ(EXECUTE_ERROR_NULL_RESPONSE, result);
}

static void execute_fails_when_table_file_does_not_exist(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    Statement statement;
    ExecuteResponse response = {0};
    FILE *output;
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    initialize_select_statement(&statement, "missing_users");
    output = tmpfile();
    ASSERT_TRUE(output != NULL);

    /* when */
    result = execute(&statement, temp_dir, output, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_ERROR_TABLE_NOT_FOUND, result);

    ASSERT_INT_EQ(0, fclose(output));
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void execute_fails_when_statement_type_is_invalid(void) {
    Statement statement = {0};
    ExecuteResponse response = {0};
    int result;

    /* given */
    statement.type = (StatementType)99;
    snprintf(statement.table_name, sizeof(statement.table_name), "%s", "users");

    /* when */
    result = execute(&statement, "/tmp", NULL, &response);

    /* then */
    ASSERT_INT_EQ(EXECUTE_ERROR_UNSUPPORTED_STATEMENT, result);
}

static void run_cli_insert_then_select_round_trips_data(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char table_path[TEST_PATH_SIZE];
    char file_contents[TEST_BUFFER_SIZE];
    char output_text[TEST_BUFFER_SIZE];
    char error_text[TEST_BUFFER_SIZE];
    FILE *input;
    FILE *output;
    FILE *error;
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    write_table_file(temp_dir, "users", NULL);
    build_table_path(temp_dir, "users", table_path, sizeof(table_path));
    input = tmpfile();
    output = tmpfile();
    error = tmpfile();
    ASSERT_TRUE(input != NULL);
    ASSERT_TRUE(output != NULL);
    ASSERT_TRUE(error != NULL);
    ASSERT_TRUE(fputs("INSERT INTO users VALUES ('kim', 1);\n", input) != EOF);
    ASSERT_TRUE(fputs("SELECT * FROM users;\n", input) != EOF);
    rewind(input);

    /* when */
    result = run_cli(input, output, error, temp_dir);

    /* then */
    ASSERT_INT_EQ(0, result);
    read_text_file(table_path, file_contents, sizeof(file_contents));
    ASSERT_STR_EQ("('kim', 1)\n", file_contents);
    read_stream(output, output_text, sizeof(output_text));
    ASSERT_STR_CONTAINS(output_text, "('kim', 1)\n");
    read_stream(error, error_text, sizeof(error_text));
    ASSERT_STR_EQ("", error_text);

    ASSERT_INT_EQ(0, fclose(input));
    ASSERT_INT_EQ(0, fclose(output));
    ASSERT_INT_EQ(0, fclose(error));
    cleanup_table_file(temp_dir, "users");
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void run_cli_continues_after_parse_error(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char table_path[TEST_PATH_SIZE];
    char file_contents[TEST_BUFFER_SIZE];
    char error_text[TEST_BUFFER_SIZE];
    FILE *input;
    FILE *output;
    FILE *error;
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    write_table_file(temp_dir, "users", NULL);
    build_table_path(temp_dir, "users", table_path, sizeof(table_path));
    input = tmpfile();
    output = tmpfile();
    error = tmpfile();
    ASSERT_TRUE(input != NULL);
    ASSERT_TRUE(output != NULL);
    ASSERT_TRUE(error != NULL);
    ASSERT_TRUE(fputs("SELECT name FROM users;\n", input) != EOF);
    ASSERT_TRUE(fputs("INSERT INTO users VALUES ('kim', 1);\n", input) != EOF);
    rewind(input);

    /* when */
    result = run_cli(input, output, error, temp_dir);

    /* then */
    ASSERT_INT_EQ(1, result);
    read_text_file(table_path, file_contents, sizeof(file_contents));
    ASSERT_STR_EQ("('kim', 1)\n", file_contents);
    read_stream(error, error_text, sizeof(error_text));
    ASSERT_STR_CONTAINS(error_text, "[parser] parse failed with error code");

    ASSERT_INT_EQ(0, fclose(input));
    ASSERT_INT_EQ(0, fclose(output));
    ASSERT_INT_EQ(0, fclose(error));
    cleanup_table_file(temp_dir, "users");
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void run_cli_writes_parse_error_to_err_stream(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char error_text[TEST_BUFFER_SIZE];
    FILE *input;
    FILE *output;
    FILE *error;
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    write_table_file(temp_dir, "users", NULL);
    input = tmpfile();
    output = tmpfile();
    error = tmpfile();
    ASSERT_TRUE(input != NULL);
    ASSERT_TRUE(output != NULL);
    ASSERT_TRUE(error != NULL);
    ASSERT_TRUE(fputs("SELECT name FROM users;\n", input) != EOF);
    rewind(input);

    /* when */
    result = run_cli(input, output, error, temp_dir);

    /* then */
    ASSERT_INT_EQ(1, result);
    read_stream(error, error_text, sizeof(error_text));
    ASSERT_STR_CONTAINS(error_text, "[parser] parse failed with error code");

    ASSERT_INT_EQ(0, fclose(input));
    ASSERT_INT_EQ(0, fclose(output));
    ASSERT_INT_EQ(0, fclose(error));
    cleanup_table_file(temp_dir, "users");
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

static void run_cli_writes_execute_error_to_err_stream(void) {
    char temp_dir[] = "/tmp/haegeon-execute-XXXXXX";
    char error_text[TEST_BUFFER_SIZE];
    FILE *input;
    FILE *output;
    FILE *error;
    int result;

    /* given */
    create_temp_directory(temp_dir, sizeof(temp_dir));
    input = tmpfile();
    output = tmpfile();
    error = tmpfile();
    ASSERT_TRUE(input != NULL);
    ASSERT_TRUE(output != NULL);
    ASSERT_TRUE(error != NULL);
    ASSERT_TRUE(fputs("SELECT * FROM users;\n", input) != EOF);
    rewind(input);

    /* when */
    result = run_cli(input, output, error, temp_dir);

    /* then */
    ASSERT_INT_EQ(1, result);
    read_stream(error, error_text, sizeof(error_text));
    ASSERT_STR_CONTAINS(error_text, "[executor] execute failed with error code");

    ASSERT_INT_EQ(0, fclose(input));
    ASSERT_INT_EQ(0, fclose(output));
    ASSERT_INT_EQ(0, fclose(error));
    ASSERT_INT_EQ(0, rmdir(temp_dir));
}

int main(void) {
    void (*tests[])(void) = {
        execute_insert_appends_row_to_existing_table,
        execute_insert_preserves_string_and_integer_format,
        execute_select_prints_all_rows,
        execute_select_returns_zero_rows_for_empty_table,
        execute_fails_when_statement_is_null,
        execute_fails_when_db_dir_is_null,
        execute_fails_when_output_is_null_for_select,
        execute_fails_when_response_is_null,
        execute_fails_when_table_file_does_not_exist,
        execute_fails_when_statement_type_is_invalid,
        run_cli_insert_then_select_round_trips_data,
        run_cli_continues_after_parse_error,
        run_cli_writes_parse_error_to_err_stream,
        run_cli_writes_execute_error_to_err_stream
    };
    const char *test_names[] = {
        "execute_insert_appends_row_to_existing_table",
        "execute_insert_preserves_string_and_integer_format",
        "execute_select_prints_all_rows",
        "execute_select_returns_zero_rows_for_empty_table",
        "execute_fails_when_statement_is_null",
        "execute_fails_when_db_dir_is_null",
        "execute_fails_when_output_is_null_for_select",
        "execute_fails_when_response_is_null",
        "execute_fails_when_table_file_does_not_exist",
        "execute_fails_when_statement_type_is_invalid",
        "run_cli_insert_then_select_round_trips_data",
        "run_cli_continues_after_parse_error",
        "run_cli_writes_parse_error_to_err_stream",
        "run_cli_writes_execute_error_to_err_stream"
    };
    int index;

    for (index = 0; index < ARRAY_SIZE(tests); index++) {
        run_test(test_names[index], tests[index]);
    }

    if (g_failed_tests != 0) {
        fprintf(stderr, "%d test(s) failed.\n", g_failed_tests);
        return 1;
    }

    printf("all execute tests passed.\n");
    return 0;
}
