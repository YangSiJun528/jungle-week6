#include "main.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    CLI_INPUT_BUFFER_SIZE = 1024,
    EXECUTE_TABLE_PATH_SIZE = 256,
    EXECUTE_ROW_BUFFER_SIZE = (STATEMENT_MAX_VALUES * (PARSED_VALUE_TEXT_SIZE + 4)) + 4
};

static const char *DEFAULT_DB_DIR = "haegeon/data";

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
 * @brief 파일 열기 실패 원인을 실행 오류 코드로 변환한다.
 * @param error_number errno 값이다.
 * @return 대응되는 EXECUTE_ERROR_* 코드를 반환한다.
 */
static int map_file_open_error(int error_number) {
    if (error_number == ENOENT) {
        return EXECUTE_ERROR_TABLE_NOT_FOUND;
    }

    return EXECUTE_ERROR_FILE_OPEN_FAILED;
}

/**
 * @brief 테이블 이름을 실제 파일 경로 문자열로 만든다.
 * @param db_dir 테이블 파일이 들어 있는 디렉토리 경로다.
 * @param table_name 테이블 이름이다.
 * @param buffer 생성된 경로를 저장할 버퍼다.
 * @param buffer_size 버퍼 크기다.
 * @return 성공 시 EXECUTE_OK, 실패 시 관련 EXECUTE_ERROR_* 를 반환한다.
 */
static int build_table_path(const char *db_dir,
                            const char *table_name,
                            char *buffer,
                            size_t buffer_size) {
    int written;

    written = snprintf(buffer, buffer_size, "%s/%s.table", db_dir, table_name);
    if (written < 0 || (size_t)written >= buffer_size) {
        return EXECUTE_ERROR_PATH_TOO_LONG;
    }

    return EXECUTE_OK;
}

/**
 * @brief 문자열 버퍼 뒤에 형식화된 텍스트를 이어 붙인다.
 * @param buffer 결과를 저장할 버퍼다.
 * @param buffer_size 버퍼 크기다.
 * @param offset 현재까지 채운 길이다.
 * @param format 추가할 형식 문자열이다.
 * @return 성공 시 EXECUTE_OK, 실패 시 EXECUTE_ERROR_ROW_FORMAT_TOO_LONG 을 반환한다.
 */
static int append_format(char *buffer,
                         size_t buffer_size,
                         size_t *offset,
                         const char *format,
                         ...) {
    va_list args;
    int written;

    if (*offset >= buffer_size) {
        return EXECUTE_ERROR_ROW_FORMAT_TOO_LONG;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, buffer_size - *offset, format, args);
    va_end(args);

    if (written < 0 || (size_t)written >= buffer_size - *offset) {
        return EXECUTE_ERROR_ROW_FORMAT_TOO_LONG;
    }

    *offset += (size_t)written;
    return EXECUTE_OK;
}

/**
 * @brief INSERT 문장의 값 목록을 저장용 한 줄 문자열로 직렬화한다.
 * @param statement 직렬화할 INSERT 문장 구조체다.
 * @param buffer 직렬화 결과를 저장할 버퍼다.
 * @param buffer_size 버퍼 크기다.
 * @return 성공 시 EXECUTE_OK, 실패 시 관련 EXECUTE_ERROR_* 를 반환한다.
 */
static int serialize_insert_row(const Statement *statement,
                                char *buffer,
                                size_t buffer_size) {
    size_t offset = 0;
    int index;
    int result;

    result = append_format(buffer, buffer_size, &offset, "(");
    if (result != EXECUTE_OK) {
        return result;
    }

    for (index = 0; index < statement->value_count; index++) {
        if (index != 0) {
            result = append_format(buffer, buffer_size, &offset, ", ");
            if (result != EXECUTE_OK) {
                return result;
            }
        }

        if (statement->values[index].type == VALUE_STRING) {
            result = append_format(buffer, buffer_size, &offset, "'%s'",
                                   statement->values[index].text);
        } else {
            result = append_format(buffer, buffer_size, &offset, "%s",
                                   statement->values[index].text);
        }

        if (result != EXECUTE_OK) {
            return result;
        }
    }

    return append_format(buffer, buffer_size, &offset, ")");
}

/**
 * @brief INSERT 문장을 기존 테이블 파일 끝에 한 줄 추가한다.
 * @param statement 실행할 INSERT 문장 구조체다.
 * @param db_dir 테이블 파일이 들어 있는 디렉토리 경로다.
 * @param out_response 실행 결과 요약 구조체다.
 * @return 성공 시 EXECUTE_OK, 실패 시 관련 EXECUTE_ERROR_* 를 반환한다.
 */
static int execute_insert(const Statement *statement,
                          const char *db_dir,
                          ExecuteResponse *out_response) {
    char table_path[EXECUTE_TABLE_PATH_SIZE];
    char row_buffer[EXECUTE_ROW_BUFFER_SIZE];
    FILE *table_file;
    int result;

    result = build_table_path(db_dir, statement->table_name,
                              table_path, sizeof(table_path));
    if (result != EXECUTE_OK) {
        return result;
    }

    table_file = fopen(table_path, "r");
    if (table_file == NULL) {
        return map_file_open_error(errno);
    }
    fclose(table_file);

    result = serialize_insert_row(statement, row_buffer, sizeof(row_buffer));
    if (result != EXECUTE_OK) {
        return result;
    }

    table_file = fopen(table_path, "a");
    if (table_file == NULL) {
        return EXECUTE_ERROR_FILE_OPEN_FAILED;
    }

    if (fprintf(table_file, "%s\n", row_buffer) < 0) {
        fclose(table_file);
        return EXECUTE_ERROR_FILE_WRITE_FAILED;
    }

    if (fclose(table_file) != 0) {
        return EXECUTE_ERROR_FILE_WRITE_FAILED;
    }

    out_response->rows_affected = 1;
    out_response->rows_returned = 0;
    return EXECUTE_OK;
}

/**
 * @brief SELECT 문장을 실행해 테이블 파일의 모든 줄을 출력한다.
 * @param statement 실행할 SELECT 문장 구조체다.
 * @param db_dir 테이블 파일이 들어 있는 디렉토리 경로다.
 * @param out 조회 결과를 기록할 출력 스트림이다.
 * @param out_response 실행 결과 요약 구조체다.
 * @return 성공 시 EXECUTE_OK, 실패 시 관련 EXECUTE_ERROR_* 를 반환한다.
 */
static int execute_select(const Statement *statement,
                          const char *db_dir,
                          FILE *out,
                          ExecuteResponse *out_response) {
    char table_path[EXECUTE_TABLE_PATH_SIZE];
    char row_buffer[EXECUTE_ROW_BUFFER_SIZE];
    FILE *table_file;
    int row_count = 0;
    int result;

    result = build_table_path(db_dir, statement->table_name,
                              table_path, sizeof(table_path));
    if (result != EXECUTE_OK) {
        return result;
    }

    table_file = fopen(table_path, "r");
    if (table_file == NULL) {
        return map_file_open_error(errno);
    }

    while (fgets(row_buffer, sizeof(row_buffer), table_file) != NULL) {
        if (strchr(row_buffer, '\n') == NULL && !feof(table_file)) {
            fclose(table_file);
            return EXECUTE_ERROR_FILE_READ_FAILED;
        }

        if (fputs(row_buffer, out) == EOF) {
            fclose(table_file);
            return EXECUTE_ERROR_FILE_WRITE_FAILED;
        }

        row_count++;
    }

    if (ferror(table_file)) {
        fclose(table_file);
        return EXECUTE_ERROR_FILE_READ_FAILED;
    }

    if (fflush(out) != 0) {
        fclose(table_file);
        return EXECUTE_ERROR_FILE_WRITE_FAILED;
    }

    if (fclose(table_file) != 0) {
        return EXECUTE_ERROR_FILE_READ_FAILED;
    }

    out_response->rows_affected = 0;
    out_response->rows_returned = row_count;
    return EXECUTE_OK;
}

/**
 * @brief 파싱된 SQL 문장을 실행한다.
 * @param statement 실행할 문장 구조체다.
 * @param db_dir 테이블 파일이 들어 있는 디렉토리 경로다.
 * @param out SELECT 결과를 기록할 출력 스트림이다.
 * @param out_response 실행 결과 요약 구조체다.
 * @return 성공 시 EXECUTE_OK, 실패 시 관련 EXECUTE_ERROR_* 를 반환한다.
 */
int execute(const Statement *statement,
            const char *db_dir,
            FILE *out,
            ExecuteResponse *out_response) {
    ExecuteResponse temporary_response = {0, 0};
    int result;

    if (statement == NULL) {
        return EXECUTE_ERROR_NULL_STATEMENT;
    }

    if (db_dir == NULL) {
        return EXECUTE_ERROR_NULL_DB_DIR;
    }

    if (out_response == NULL) {
        return EXECUTE_ERROR_NULL_RESPONSE;
    }

    if (statement->type == STATEMENT_SELECT && out == NULL) {
        return EXECUTE_ERROR_NULL_OUTPUT;
    }

    if (statement->type == STATEMENT_INSERT) {
        result = execute_insert(statement, db_dir, &temporary_response);
    } else if (statement->type == STATEMENT_SELECT) {
        result = execute_select(statement, db_dir, out, &temporary_response);
    } else {
        return EXECUTE_ERROR_UNSUPPORTED_STATEMENT;
    }

    if (result != EXECUTE_OK) {
        return result;
    }

    *out_response = temporary_response;
    return EXECUTE_OK;
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

#ifndef HAEGEON_TEST
/**
 * @brief 표준 입출력 기반 CLI를 실행한다.
 * @param 없음
 * @return 프로그램 종료 코드를 반환한다.
 */
int main(void) {
    return run_cli(stdin, stdout, stderr, DEFAULT_DB_DIR);
}
#endif
