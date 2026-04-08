#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* 현재 입력 줄이 어떤 명령인지 구분하기 위한 값이다. */
typedef enum { NONE, BAD, INSERT, SELECT, EXIT } Kind;

/* parse 결과를 execute로 넘기기 위한 가장 단순한 구조체다. */
typedef struct {
    Kind kind;
    /* values(...) 안의 원본 문자열을 그대로 담는다. */
    char vals[256];
    /* 접근할 테이블 이름을 저장한다. */
    char table[128];
} Cmd;

/*
 * 사용자가 입력한 문자열을 파싱하기 쉽게 정리한다.
 * 바깥 공백은 정리하고 여러 칸 공백은 한 칸으로 줄인다.
 * 작은따옴표 안쪽은 데이터이므로 그대로 둔다.
 */
void clean(char *s) {
    int i = 0;
    int j = 0;
    int space = 1;
    int quote = 0;

    while (s[i]) {
        if (s[i] == '\'') {
            s[j++] = s[i++];
            quote = !quote;
            space = 0;
            continue;
        }

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

    if (j > 0 && s[j - 1] == ' ') {
        j--;
    }

    s[j] = '\0';
}

/* main.c가 있는 폴더를 기준으로 테이블 CSV 파일 경로를 만든다. */
void table_path(char *path, const char *table) {
    const char *a = strrchr(__FILE__, '/');
    const char *b = strrchr(__FILE__, '\\');
    const char *slash = a;
    int n = 0;

    if (slash == NULL || (b != NULL && b > slash)) {
        slash = b;
    }

    if (slash != NULL) {
        n = (int)(slash - __FILE__ + 1);
        memcpy(path, __FILE__, n);
    }

    path[n] = '\0';
    strcat(path, table);
    strcat(path, ".csv");
}

/* values(...) 형태를 CSV 한 줄 형태로 바꾼다. */
void vals_to_csv(const char *vals, char *row) {
    int i = 0;
    int j = 0;
    int quote = 0;

    while (vals[i]) {
        if (vals[i] == '\'') {
            quote = !quote;
            i++;
            continue;
        }

        if (!quote && (vals[i] == '(' || vals[i] == ')' || vals[i] == ' ')) {
            i++;
            continue;
        }

        row[j++] = vals[i++];
    }

    row[j] = '\0';
}

/*
 * 지원하는 문법만 아주 단순하게 해석한다.
 * 1. .exit
 * 2. insert into 테이블 values (...)
 * 3. select * from 테이블
 */
Cmd parse(char *line) {
    Cmd cmd = { BAD, "", "" };
    char *p;
    char *q;
    char *tok[5];
    int n = 0;

    clean(line);

    if (line[0] == '\0') {
        cmd.kind = NONE;
        return cmd;
    }

    if (strcmp(line, ".exit") == 0) {
        cmd.kind = EXIT;
        return cmd;
    }

    if (strncmp(line, "insert into ", 12) == 0) {
        p = line + 12;
        q = strstr(p, " values ");

        if (q == NULL) {
            return cmd;
        }

        *q = '\0';
        q += 8;

        if (p[0] == '\0' || q[0] != '(' || q[strlen(q) - 1] != ')') {
            return cmd;
        }

        cmd.kind = INSERT;
        strcpy(cmd.table, p);
        strcpy(cmd.vals, q);
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

    if (n == 4 && tok[4] == NULL &&
        strcmp(tok[0], "select") == 0 &&
        strcmp(tok[1], "*") == 0 &&
        strcmp(tok[2], "from") == 0) {
        cmd.kind = SELECT;
        strcpy(cmd.table, tok[3]);
        return cmd;
    }

    return cmd;
}

/*
 * parse가 만든 Cmd를 실제로 처리한다.
 * INSERT는 테이블 이름.csv 파일에 한 줄 추가한다.
 * SELECT는 테이블 이름.csv 내용을 그대로 출력한다.
 */
int execute(Cmd cmd, FILE *out) {
    char path[512];
    char row[256];
    char buf[256];
    FILE *fp;

    table_path(path, cmd.table);

    if (cmd.kind == INSERT) {
        vals_to_csv(cmd.vals, row);

        fp = fopen(path, "a");
        if (fp == NULL) {
            fprintf(out, "error\n");
            return 0;
        }

        fprintf(fp, "%s\n", row);
        fclose(fp);
        fprintf(out, "saved\n");
        return 1;
    }

    if (cmd.kind == SELECT) {
        fp = fopen(path, "r");
        if (fp == NULL) {
            fprintf(out, "no table\n");
            return 0;
        }

        while (fgets(buf, sizeof(buf), fp) != NULL) {
            fputs(buf, out);
        }

        fclose(fp);
        return 1;
    }

    fprintf(out, "error\n");
    return 0;
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

        execute(cmd, stdout);
    }

    return 0;
}
#endif
