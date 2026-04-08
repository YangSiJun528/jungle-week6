#include "../src/sql_engine.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static FILE *open_input_stream(const char *text) {
    FILE *input = tmpfile();

    assert(input != NULL);

    if (text != NULL) {
        assert(fwrite(text, 1, strlen(text), input) == strlen(text));
    }

    rewind(input);
    return input;
}

static void test_read_sql_input_single_line(void) {
    char buffer[SQL_INPUT_BUFFER_SIZE];
    FILE *input = open_input_stream("SELECT * FROM users;\n");

    assert(read_sql_input(input, buffer, sizeof(buffer)) == READ_SQL_OK);
    assert(strcmp(buffer, "SELECT * FROM users;\n") == 0);

    fclose(input);
}

static void test_read_sql_input_multi_line(void) {
    char buffer[SQL_INPUT_BUFFER_SIZE];
    FILE *input = open_input_stream("SELECT *\nFROM users;\n");

    assert(read_sql_input(input, buffer, sizeof(buffer)) == READ_SQL_OK);
    assert(strcmp(buffer, "SELECT *\nFROM users;\n") == 0);

    fclose(input);
}

static void test_read_sql_input_empty(void) {
    char buffer[SQL_INPUT_BUFFER_SIZE];
    FILE *input = open_input_stream("");

    assert(read_sql_input(input, buffer, sizeof(buffer)) == READ_SQL_EMPTY);
    assert(strcmp(buffer, "") == 0);

    fclose(input);
}

static void test_read_sql_input_too_long(void) {
    char text[SQL_INPUT_BUFFER_SIZE + 1];
    char buffer[SQL_INPUT_BUFFER_SIZE];
    FILE *input;
    size_t i;

    for (i = 0; i < SQL_INPUT_BUFFER_SIZE; i++) {
        text[i] = 'a';
    }
    text[SQL_INPUT_BUFFER_SIZE] = '\0';

    input = open_input_stream(text);

    assert(read_sql_input(input, buffer, sizeof(buffer)) == READ_SQL_TOO_LONG);
    assert(strcmp(buffer, "") == 0);

    fclose(input);
}

static void test_parse_rejects_blank_sql(void) {
    ParsedStatement stmt;

    assert(parse("   \n\t", &stmt) == 1);
}

static void test_parse_stores_raw_sql(void) {
    ParsedStatement stmt;
    const char *sql = "SELECT * FROM users;";

    assert(parse(sql, &stmt) == 0);
    assert(stmt.raw_sql == sql);
}

static void test_execute_returns_ok_message(void) {
    ParsedStatement stmt = { "SELECT * FROM users;" };
    ExecuteResult result;

    assert(execute(&stmt, &result) == 0);
    assert(strcmp(result.message, "OK") == 0);
}

int main(void) {
    test_read_sql_input_single_line();
    test_read_sql_input_multi_line();
    test_read_sql_input_empty();
    test_read_sql_input_too_long();
    test_parse_rejects_blank_sql();
    test_parse_stores_raw_sql();
    test_execute_returns_ok_message();

    return 0;
}
