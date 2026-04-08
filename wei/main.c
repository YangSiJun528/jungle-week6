#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    STATEMENT_NONE,
    STATEMENT_SELECT,
    STATEMENT_INSERT
} StatementType;

typedef enum {
    PARSE_OK,
    PARSE_ERROR
} ParseStatus;

// 결과로 반환할 구조체
typedef struct {
    StatementType type;
    ParseStatus status;
    char *table_name;
    char *values;
} Statement;

static bool end_with_semicolon(char *text) {
    size_t length = strlen(text);

    if (length == 0 || text[length - 1] != ';') {
        return false;
    }

    text[length - 1] = '\0';
    return true;
}

/* parse는 입력 문자열 하나를 "해석된 문장"으로 바꾸는 가장 작은 parser 역할을 한다. */
/* statement는 시작부터 PARSE_ERROR로 만들어 두고, 문법이 맞는 분기에서만 PARSE_OK로 바꿔 반환한다. */
Statement parse(char *input) {
    Statement statement = {STATEMENT_NONE, PARSE_ERROR, NULL, NULL};
    char *values_start;
    char *values_end;

    /* 1. SELECT 문법인지 먼저 확인한다. 이 단계는 접두어가 정확히 같은지만 본다(0이면 글자 수 같다, 음수는 적다, 양수는 많다). */
    if (strncmp(input, "SELECT * from ", 14) == 0) {
        /* 접두어 뒤부터가 테이블 이름이라고 가정하고 포인터를 잡는다(char :1 * 14 뒤 주소). */
        statement.table_name = input + 14;

        /* SELECT는 테이블 이름이 있어야 하고, 마지막은 반드시 ';'로 끝나야 한다. */
        if (*statement.table_name == '\0' || !end_with_semicolon(statement.table_name)) {
            return statement;
        }

        /* ';'를 떼고 나면 공백 없는 단일 테이블 이름만 남아야 한다. */
        if (*statement.table_name == '\0' || strpbrk(statement.table_name, " \t") != NULL) {
            return (Statement){STATEMENT_NONE, PARSE_ERROR, NULL, NULL};
        }

        /* SELECT 문법이 맞으면 타입과 상태를 채우고 즉시 반환한다. */
        statement.type = STATEMENT_SELECT;
        statement.status = PARSE_OK;
        return statement;
    }

    /* 2. SELECT가 아니면 INSERT 문법인지 본다. 아니면 바로 실패다. */
    if (strncmp(input, "INSERT into ", 12) != 0) {
        return statement;
    }

    /* INSERT는 "테이블 이름 values (값들)" 형태라서 중간 경계를 찾아 분리한다. */
    statement.table_name = input + 12;
    values_start = strstr(statement.table_name, " values (");
    if (values_start == NULL || values_start == statement.table_name) {
        return statement;
    }

    /* " values (" 앞을 끊어서 table_name을 독립 문자열처럼 만든다. */
    *values_start = '\0';
    statement.values = values_start + 9;

    /* INSERT도 SELECT와 마찬가지로 테이블 이름과 값 문자열을 먼저 분리한 뒤 ';'를 검사한다. */
    if (*statement.table_name == '\0' || strpbrk(statement.table_name, " \t") != NULL ||
        *statement.values == '\0' || !end_with_semicolon(statement.values)) {
        return (Statement){STATEMENT_NONE, PARSE_ERROR, NULL, NULL};
    }

    values_end = strrchr(statement.values, ')');
    if (values_end == NULL || values_end == statement.values || values_end[1] != '\0') {
        return (Statement){STATEMENT_NONE, PARSE_ERROR, NULL, NULL};
    }

    /* 마지막 ')'를 문자열 끝으로 바꿔 values만 따로 읽을 수 있게 만든다. */
    *values_end = '\0';
    statement.type = STATEMENT_INSERT;
    statement.status = PARSE_OK;
    return statement;
}

int main(void) {
    char *line = NULL;
    size_t line_capacity = 0;

    /* main은 REPL 진입점이다. 한 줄을 받고, 종료인지 SQL인지 판단하는 흐름을 반복한다. */
    while (true) {
        /* 사용자가 다음 명령을 입력할 수 있도록 항상 같은 프롬프트를 먼저 보여준다. */
        printf("db > ");
        fflush(stdout);

        /* getline은 한 줄 전체를 동적으로 읽는다. EOF면 프로그램도 조용히 끝낸다. */
        if (getline(&line, &line_capacity, stdin) == -1) {
            free(line);
            return 0;
        }

        /* 줄 끝 개행을 제거해서 비교와 파싱이 쉬운 순수 문자열로 만든다. */
        line[strcspn(line, "\n")] = '\0';

        /* .exit는 SQL이 아니라 REPL 제어용 메타 명령이라 parse 전에 먼저 처리한다. */
        if (strcmp(line, ".exit") == 0) {
            free(line);
            return 0;
        }

        /* 이번 단계는 "실행"이 아니라 "문법 해석" 단계다.
         * 따라서 parse가 성공하면 아무 출력 없이 다음 입력을 받고,
         * 실패하면 아직 지원하지 않는 문장으로 보고 에러 메시지를 보여준다.
         */
        if (parse(line).status == PARSE_ERROR) {
            printf("Unrecognized command\n");
        }
    }
}
