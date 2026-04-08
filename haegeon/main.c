#include <stdio.h>
#include <string.h>

int main(void) {
    char sql[1024];

    /* stdin에서 한 줄씩 입력받는다. */
    while (fgets(sql, sizeof(sql), stdin) != NULL) {
        /* 종료 명령이 들어오면 반복을 끝내고 프로그램을 종료한다. */
        if (strcmp(sql, ".exit\n") == 0 || strcmp(sql, ".quit\n") == 0) {
            break;
        }

        /* 빈 줄은 무시한다. */
        if (sql[0] == '\n') {
            continue;
        }

        /* 지금은 parser / executor가 없으므로 흐름만 출력으로 흉내 낸다. */
        printf("[main] input: %s", sql);
        printf("[parser placeholder] parser should make a statement here.\n");
        printf("[executor placeholder] executor should run the statement here.\n");
    }

    return 0;
}
