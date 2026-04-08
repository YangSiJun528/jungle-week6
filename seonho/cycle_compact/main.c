#include <stdio.h>
#include <string.h>

typedef enum { BAD, INSERT, SELECT } Type;




#ifndef TESTING
int main(void) {
    char sql[1024];


    while (fgets(sql, sizeof(sql), stdin)) {
        sql[strcspn(sql, "\n")] = 0;
        if (!sql[0]) continue;
        if (!strcmp(sql, ".exit")) break;
        printf(sql);
        if (s.type == BAD) printf("parse error\n");
    }
    return 0;
}
#endif
