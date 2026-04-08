#define TESTING
#include "main.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char buffer[1024];
    FILE *input = tmpfile();

    assert(input != NULL);

    fputs("insert 1 a@a.com\n", input);
    fputs("\n", input);
    fputs("select\n", input);
    fputs(".exit\n", input);
    rewind(input);

    assert(read_sql_line(input, buffer, sizeof(buffer)) == 1);
    assert(strcmp(buffer, "insert 1 a@a.com") == 0);

    assert(read_sql_line(input, buffer, sizeof(buffer)) == 1);
    assert(strcmp(buffer, "") == 0);

    assert(read_sql_line(input, buffer, sizeof(buffer)) == 1);
    assert(strcmp(buffer, "select") == 0);

    assert(read_sql_line(input, buffer, sizeof(buffer)) == 1);
    assert(strcmp(buffer, ".exit") == 0);

    assert(read_sql_line(input, buffer, sizeof(buffer)) == 0);

    fclose(input);
    return 0;
}
