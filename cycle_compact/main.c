#include <stdio.h>
#include <string.h>

typedef enum { INSERT, SELECT } Type;

typedef struct {
    Type type;
} Stmt;

int parse(char *sql, Stmt *s) {
    if (!strncmp(sql, "insert", 6)) return s->type = INSERT, 0;
    if (!strncmp(sql, "select", 6)) return s->type = SELECT, 0;
    return 1;
}

int execute(Stmt *s) {
    if (s->type == INSERT) printf("insert ok\n");
    else printf("select ok\n");
    return 0;
}

int main(void) {
    char sql[1024];
    Stmt s;

    while (fgets(sql, sizeof(sql), stdin)) {
        sql[strcspn(sql, "\n")] = 0;
        if (!sql[0]) continue;
        if (!strcmp(sql, ".exit")) break;
        if (parse(sql, &s)) printf("parse error\n");
        else if (execute(&s)) printf("execute error\n");
    }
    return 0;
}
