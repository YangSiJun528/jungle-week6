#include "cli.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "execute.h"
#include "parse.h"

enum {
    CLI_INPUT_BUFFER_SIZE = 1024
};

/**
 * @brief 종료 메타 명령인지 확인한다.
 * @param sql 확인할 입력 문자열이다.
 * @return 종료 명령이면 1, 아니면 0을 반환한다.
 */
static int is_exit_command(const char *sql) {
    return strcmp(sql, ".exit\n") == 0 ||
           strcmp(sql, ".quit\n") == 0 ||
           strcmp(sql, ".exit") == 0 ||
           strcmp(sql, ".quit") == 0;
}

/**
 * @brief 공백만 있는 입력인지 확인한다.
 * @param sql 확인할 입력 문자열이다.
 * @return 공백만 있으면 1, 아니면 0을 반환한다.
 */
static int is_blank_input(const char *sql) {
    while (*sql != '\0') {
        if (!isspace((unsigned char)*sql)) {
            return 0;
        }

        sql++;
    }

    return 1;
}

/**
 * @brief 파싱 성공 결과를 출력 스트림에 간단히 요약한다.
 * @param out 요약을 기록할 출력 스트림이다.
 * @param statement 출력할 파싱 결과 구조체다.
 * @return 반환값은 없다.
 */
static void print_statement_summary(FILE *out, const Statement *statement) {
    if (statement->type == STATEMENT_SELECT) {
        fprintf(out, "[parser] parsed SELECT from table '%s'.\n", statement->table_name);
        return;
    }

    fprintf(out, "[parser] parsed INSERT into table '%s' with %d value(s).\n",
            statement->table_name, statement->value_count);
}

/**
 * @brief 실행 성공 결과를 출력 스트림에 간단히 요약한다.
 * @param out 요약을 기록할 출력 스트림이다.
 * @param statement 실행한 문장 구조체다.
 * @param response 실행 결과 요약 구조체다.
 * @return 반환값은 없다.
 */
static void print_execute_summary(FILE *out,
                                  const Statement *statement,
                                  const ExecuteResponse *response) {
    if (statement->type == STATEMENT_SELECT) {
        fprintf(out, "[executor] selected %d row(s) from table '%s'.\n",
                response->rows_returned, statement->table_name);
        return;
    }

    fprintf(out, "[executor] inserted %d row(s) into table '%s'.\n",
            response->rows_affected, statement->table_name);
}

/**
 * @brief 입력 스트림에서 SQL을 읽어 parse와 execute를 순서대로 수행한다.
 * @param in SQL 입력을 읽을 스트림이다.
 * @param out 정상 출력 스트림이다.
 * @param err 오류 출력 스트림이다.
 * @param db_dir 테이블 파일이 들어 있는 디렉토리 경로다.
 * @return 오류가 없으면 0, 하나라도 실패가 있으면 1을 반환한다.
 */
int run_cli(FILE *in, FILE *out, FILE *err, const char *db_dir) {
    char sql[CLI_INPUT_BUFFER_SIZE];
    Statement statement;
    ExecuteResponse execute_response;
    int parse_result;
    int execute_result;
    int had_error = 0;

    if (in == NULL || out == NULL || err == NULL || db_dir == NULL) {
        return 1;
    }

    while (fgets(sql, sizeof(sql), in) != NULL) {
        if (is_exit_command(sql)) {
            break;
        }

        if (is_blank_input(sql)) {
            continue;
        }

        fprintf(out, "[main] input: %s", sql);
        if (strchr(sql, '\n') == NULL) {
            fputc('\n', out);
        }

        parse_result = parse(sql, &statement);
        if (parse_result != PARSE_OK) {
            fprintf(err, "[parser] parse failed with error code %d.\n", parse_result);
            had_error = 1;
            continue;
        }

        print_statement_summary(out, &statement);

        execute_result = execute(&statement, db_dir, out, &execute_response);
        if (execute_result != EXECUTE_OK) {
            fprintf(err, "[executor] execute failed with error code %d.\n", execute_result);
            had_error = 1;
            continue;
        }

        print_execute_summary(out, &statement, &execute_response);
    }

    if (ferror(in)) {
        fprintf(err, "[main] input read failed.\n");
        had_error = 1;
    }

    return had_error == 0 ? 0 : 1;
}
