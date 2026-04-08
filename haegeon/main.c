#include <stdio.h>
#include <string.h>

#include "parse.h"

/**
 * @brief 파싱 성공 결과를 표준 출력에 간단히 요약한다.
 * @param statement 출력할 파싱 결과 구조체다.
 * @return 반환값은 없다.
 */
static void print_statement_summary(const Statement *statement) {
    if (statement->type == STATEMENT_SELECT) {
        printf("[parser] parsed SELECT from table '%s'.\n", statement->table_name);
        return;
    }

    printf("[parser] parsed INSERT into table '%s' with %d value(s).\n",
           statement->table_name, statement->value_count);
}

/**
 * @brief 표준 입력에서 SQL을 읽어 parse 단계까지 처리한다.
 * @param 없음
 * @return 프로그램 종료 코드를 반환한다.
 */
int main(void) {
    char sql[1024];
    Statement statement;
    int result;

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

        result = parse(sql, &statement);

        printf("[main] input: %s", sql);

        if (result != PARSE_OK) {
            fprintf(stderr, "[parser] parse failed with error code %d.\n", result);
            continue;
        }

        print_statement_summary(&statement);
        printf("[executor placeholder] executor should run the statement here.\n");
    }

    return 0;
}
