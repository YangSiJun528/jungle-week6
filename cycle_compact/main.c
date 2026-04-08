#include <stdio.h>
#include <string.h>

typedef enum { BAD, INSERT, SELECT } Type;

typedef struct {
    Type type;
    int id;
    char mail[256];
} Stmt;

Stmt parse(char *sql) {
    Stmt s = { BAD, 0, "" };

    if (sscanf(sql, "insert %d %255s", &s.id, s.mail) == 2) s.type = INSERT;
    else if (!strcmp(sql, "select")) s.type = SELECT;
    return s;
}

int run(Stmt s, const char *file, FILE *out) {
    FILE *fp;
    char line[512];
    Stmt row;

    if (s.type == INSERT) {
        fp = fopen(file, "a");
        if (!fp) return 1;
        fprintf(fp, "%d,%s\n", s.id, s.mail);
        fclose(fp);
        fprintf(out, "insert ok\n");
        return 0;
    }

    fp = fopen(file, "r");
    if (!fp) return 1;
    while (fgets(line, sizeof(line), fp))
        if (sscanf(line, "%d,%255[^,\n]", &row.id, row.mail) == 2)
            fprintf(out, "%d %s\n", row.id, row.mail);
    fclose(fp);
    return 0;
}

int execute(Stmt s) {
    return run(s, "data.csv", stdout);
}

#ifndef TESTING
int main(void) {
    char sql[1024];
    Stmt s;

    while (fgets(sql, sizeof(sql), stdin)) {
        sql[strcspn(sql, "\n")] = 0;
        if (!sql[0]) continue;
        if (!strcmp(sql, ".exit")) break;
        s = parse(sql);
        if (s.type == BAD) printf("parse error\n");
        else if (execute(s)) printf("execute error\n");
    }
    return 0;
}
#endif
