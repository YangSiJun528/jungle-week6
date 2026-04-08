#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef enum { NONE, BAD, INSERT, SELECT, EXIT } Kind;

typedef struct {
    Kind kind;
    char value[128];
    char table[128];
} Cmd;

void clean(char *s) {
    int i = 0;
    int j = 0;
    int space = 1;

    while (s[i]) {
        if (isspace((unsigned char)s[i])) {
            if (!space) {
                s[j++] = ' ';
            }
            space = 1;
        } else {
            s[j++] = s[i];
            space = 0;
        }
        i++;
    }

    if (j > 0 && s[j - 1] == ' ') {
        j--;
    }

    s[j] = '\0';
}

Cmd parse(char *line) {
    Cmd cmd = { BAD, "", "" };
    char *tok[5];
    int n = 0;

    clean(line);

    if (line[0] == '\0') {
        cmd.kind = NONE;
        return cmd;
    }

    tok[n] = strtok(line, " ");
    while (tok[n] != NULL && n < 4) {
        n++;
        tok[n] = strtok(NULL, " ");
    }

    if (tok[0] == NULL) {
        cmd.kind = NONE;
        return cmd;
    }

    if (strcmp(tok[0], ".exit") == 0 && n == 1) {
        cmd.kind = EXIT;
        return cmd;
    }

    if (n == 4 && tok[4] == NULL && strcmp(tok[0], "insert") == 0 && strcmp(tok[2], "into") == 0) {
        cmd.kind = INSERT;
        strcpy(cmd.value, tok[1]);
        strcpy(cmd.table, tok[3]);
        return cmd;
    }

    if (n == 4 && tok[4] == NULL && strcmp(tok[0], "select") == 0 && strcmp(tok[1], "*") == 0 && strcmp(tok[2], "from") == 0) {
        cmd.kind = SELECT;
        strcpy(cmd.table, tok[3]);
        return cmd;
    }

    return cmd;
}

#ifndef TESTING
int main(void) {
    char line[256];
    Cmd cmd;

    while (1) {
        printf("db > ");

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        cmd = parse(line);

        if (cmd.kind == NONE) {
            continue;
        }

        if (cmd.kind == EXIT) {
            break;
        }

        if (cmd.kind == BAD) {
            printf("error\n");
            continue;
        }

        if (cmd.kind == INSERT) {
            printf("insert %s %s\n", cmd.value, cmd.table);
        } else {
            printf("select %s\n", cmd.table);
        }

        /* execute(cmd); */
    }

    return 0;
}
#endif
