#include "db.h"

#include <stdio.h>
#include <string.h>

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
