#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* 현재 입력 줄이 어떤 종류의 명령인지 구분하기 위한 값이다. */
typedef enum { NONE, BAD, INSERT, SELECT, EXIT } Kind;

/* parse 결과를 execute 직전까지 넘기기 위한 가장 단순한 구조체다. */
typedef struct {
    Kind kind;
    /* values(...) 안의 원본 문자열을 그대로 담아 둔다. */
    char vals[256];
    /* 접근할 테이블 이름을 저장한다. */
    char table[128];
} Cmd;

/* 
 * 사용자가 입력한 문자열을 파싱하기 쉽게 정리한다.
 * 바깥 공백은 정리하고, 여러 칸 공백은 한 칸으로 줄인다.
 * 단, 작은따옴표 안의 값은 데이터이므로 공백을 건드리지 않는다.
 */
void clean(char *s) {
    int i = 0;
    int j = 0;
    int space = 1;
    int quote = 0;

    while (s[i]) {
        /* 작은따옴표를 만나면 문자열 내부/외부 상태를 바꾼다. */
        if (s[i] == '\'') {
            s[j++] = s[i++];
            quote = !quote;
            space = 0;
            continue;
        }

        /* 문자열 바깥에서만 공백을 하나로 압축한다. */
        if (!quote && isspace((unsigned char)s[i])) {
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

    /* 마지막이 공백이면 제거한다. */
    if (j > 0 && s[j - 1] == ' ') {
        j--;
    }

    s[j] = '\0';
}

/*
 * 지원하는 문법만 아주 단순하게 해석한다.
 * 1. .exit
 * 2. insert into 테이블 values (...)
 * 3. select * from 테이블
 * 나머지는 전부 BAD로 돌려보낸다.
 */
Cmd parse(char *line) {
    Cmd cmd = { BAD, "", "" };
    char *p;
    char *q;
    char *tok[5];
    int n = 0;

    /* 먼저 공백을 정리해서 입력 형태를 최대한 일정하게 맞춘다. */
    clean(line);

    /* 빈 줄은 무시할 수 있게 NONE으로 돌려보낸다. */
    if (line[0] == '\0') {
        cmd.kind = NONE;
        return cmd;
    }

    /* REPL 종료 명령은 따로 가장 먼저 처리한다. */
    if (strcmp(line, ".exit") == 0) {
        cmd.kind = EXIT;
        return cmd;
    }

    /*
     * insert 문은 values 안에 공백과 쉼표가 섞일 수 있어서
     * strtok로 먼저 자르지 않고 " values " 위치를 직접 찾는다.
     */
    if (strncmp(line, "insert into ", 12) == 0) {
        p = line + 12;
        q = strstr(p, " values ");

        /* values 키워드가 없으면 올바른 insert 문이 아니다. */
        if (q == NULL) {
            return cmd;
        }

        /* 테이블 이름과 values 부분을 둘로 나눈다. */
        *q = '\0';
        q += 8;

        /* 테이블 이름이 비었거나 괄호 형태가 아니면 오류다. */
        if (p[0] == '\0' || q[0] != '(' || q[strlen(q) - 1] != ')') {
            return cmd;
        }

        cmd.kind = INSERT;
        strcpy(cmd.table, p);
        strcpy(cmd.vals, q);
        return cmd;
    }

    /* select 문은 형태가 단순해서 공백 기준으로 잘라도 충분하다. */
    tok[n] = strtok(line, " ");
    while (tok[n] != NULL && n < 4) {
        n++;
        tok[n] = strtok(NULL, " ");
    }

    /* 혹시라도 토큰이 전혀 없으면 빈 입력처럼 처리한다. */
    if (tok[0] == NULL) {
        cmd.kind = NONE;
        return cmd;
    }

    /* select * from 테이블 한 형태만 통과시킨다. */
    if (n == 4 && tok[4] == NULL && strcmp(tok[0], "select") == 0 && strcmp(tok[1], "*") == 0 && strcmp(tok[2], "from") == 0) {
        cmd.kind = SELECT;
        strcpy(cmd.table, tok[3]);
        return cmd;
    }

    /* 여기까지 걸리지 않으면 지원하지 않는 문장이다. */
    return cmd;
}

#ifndef TESTING
int main(void) {
    char line[256];
    Cmd cmd;

    /*
     * 가장 단순한 REPL 구조다.
     * 한 줄 입력 -> parse -> 결과 확인 -> execute 직전까지 전달
     */
    while (1) {
        printf("db > ");

        /* Ctrl+Z/Ctrl+D 등으로 입력이 끝나면 프로그램도 종료한다. */
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        cmd = parse(line);

        /* 빈 줄은 아무 일도 하지 않고 다음 입력으로 넘어간다. */
        if (cmd.kind == NONE) {
            continue;
        }

        /* .exit를 만나면 REPL을 끝낸다. */
        if (cmd.kind == EXIT) {
            break;
        }

        /* 문법이 틀리면 간단히 error만 출력한다. */
        if (cmd.kind == BAD) {
            printf("error\n");
            continue;
        }

        /* 지금은 execute가 없으므로 파싱 결과만 눈으로 확인한다. */
        if (cmd.kind == INSERT) {
            printf("insert %s %s\n", cmd.table, cmd.vals);
        } else {
            printf("select %s\n", cmd.table);
        }

        /* 다음 사이클에서 execute(cmd)를 여기서 호출하면 된다. */
        /* execute(cmd); */
    }

    return 0;
}
#endif
