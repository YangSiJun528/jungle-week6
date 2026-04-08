#include "execute.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    EXECUTE_TABLE_PATH_SIZE = 256,
    EXECUTE_ROW_BUFFER_SIZE = (STATEMENT_MAX_VALUES * (PARSED_VALUE_TEXT_SIZE + 4)) + 4
};

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
