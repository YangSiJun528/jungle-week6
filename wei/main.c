#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <ctype.h>
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

#include "execute.c"

static bool end_with_semicolon(char *text) {
    size_t length = strlen(text);

    if (length == 0 || text[length - 1] != ';') {
        return false;
    }

    text[length - 1] = '\0';
    return true;
}

static bool match_keyword(char **text, const char *keyword) {
    size_t index = 0;

    while (keyword[index] != '\0') {
        if ((*text)[index] == '\0' ||
            tolower((unsigned char)(*text)[index]) != tolower((unsigned char)keyword[index])) {
            return false;
        }
        index++;
    }

    *text += index;
    return true;
}

static bool skip_spaces(char **text) {
    bool found_space = false;

    while (**text == ' ' || **text == '\t') {
        found_space = true;
        (*text)++;
    }

    return found_space;
}

/* parse는 입력 문자열 하나를 "해석된 문장"으로 바꾸는 가장 작은 parser 역할을 한다. */
/* statement는 시작부터 PARSE_ERROR로 만들어 두고, 문법이 맞는 분기에서만 PARSE_OK로 바꿔 반환한다. */
Statement parse(char *input) {
    Statement statement = {STATEMENT_NONE, PARSE_ERROR, NULL, NULL};
    char *cursor = input;
    char *values_end;

    /* 1. SELECT는 키워드 대소문자를 무시하되, 문법 순서는 그대로 지킨다. */
    if (match_keyword(&cursor, "select")) {
        if (!skip_spaces(&cursor) || *cursor != '*') {
            return statement;
        }

        cursor++;
        if (!skip_spaces(&cursor) || !match_keyword(&cursor, "from") || !skip_spaces(&cursor)) {
            return statement;
        }

        statement.table_name = cursor;
        if (*statement.table_name == '\0' || !end_with_semicolon(statement.table_name)) {
            return statement;
        }

        if (*statement.table_name == '\0' || strpbrk(statement.table_name, " \t") != NULL) {
            return (Statement){STATEMENT_NONE, PARSE_ERROR, NULL, NULL};
        }

        statement.type = STATEMENT_SELECT;
        statement.status = PARSE_OK;
        return statement;
    }

    cursor = input;

    /* 2. INSERT도 같은 방식으로 키워드와 공백을 순서대로 해석한다. */
    if (!match_keyword(&cursor, "insert") || !skip_spaces(&cursor) ||
        !match_keyword(&cursor, "into") || !skip_spaces(&cursor)) {
        return statement;
    }

    statement.table_name = cursor;

    while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t') {
        cursor++;
    }
    if (cursor == statement.table_name) {
        return statement;
    }

    if (*cursor == '\0') {
        return statement;
    }

    /* 테이블 이름 끝을 먼저 끊어 두면 이후 values 키워드를 읽어도 이름 문자열이 섞이지 않는다. */
    *cursor = '\0';
    cursor++;

    skip_spaces(&cursor);
    if (!match_keyword(&cursor, "values") || !skip_spaces(&cursor) || *cursor != '(') {
        return statement;
    }

    cursor++;
    statement.values = cursor;

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

        /* 파싱이 끝나면 실행 단계로 넘기고, 결과 문자열을 그대로 출력한다. */
        Statement statement = parse(line);
        ExecutionResult result;

        if (statement.status == PARSE_ERROR) {
            printf("Unrecognized command\n");
            continue;
        }

        result = execute(statement);
        if (result.output != NULL) {
            printf("%s", result.output);
            free(result.output);
        }
    }
}
