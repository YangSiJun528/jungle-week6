#include <stdio.h>
#include <string.h>

int read_sql_line(FILE *input, char *buffer, int size) {
    if (fgets(buffer, size, input) == NULL) {
        return 0;
    }

    buffer[strcspn(buffer, "\n")] = '\0';
    return 1;
}

#ifndef TESTING
int main(void) {
    char sql[1024];

    while (read_sql_line(stdin, sql, sizeof(sql))) {
        if (sql[0] == '\0') {
            continue;
        }

        if (strcmp(sql, ".exit") == 0) {
            break;
        }

        printf("input: %s\n", sql);
    }

    return 0;
}
#endif
