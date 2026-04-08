#ifndef HAEGEON_EXECUTE_H
#define HAEGEON_EXECUTE_H

#include <stdio.h>

#include "parse.h"

typedef enum {
    EXECUTE_OK = 0,
    EXECUTE_ERROR_NULL_STATEMENT,
    EXECUTE_ERROR_NULL_DB_DIR,
    EXECUTE_ERROR_NULL_OUTPUT,
    EXECUTE_ERROR_NULL_RESPONSE,
    EXECUTE_ERROR_UNSUPPORTED_STATEMENT,
    EXECUTE_ERROR_PATH_TOO_LONG,
    EXECUTE_ERROR_TABLE_NOT_FOUND,
    EXECUTE_ERROR_FILE_OPEN_FAILED,
    EXECUTE_ERROR_FILE_READ_FAILED,
    EXECUTE_ERROR_FILE_WRITE_FAILED,
    EXECUTE_ERROR_ROW_FORMAT_TOO_LONG
} ExecuteResult;

typedef struct {
    int rows_affected;
    int rows_returned;
} ExecuteResponse;

/**
 * @brief 파싱된 SQL 문장을 실행한다.
 * @param statement 실행할 문장 구조체다.
 * @param db_dir 테이블 파일이 들어 있는 디렉토리 경로다.
 * @param out SELECT 결과를 기록할 출력 스트림이다.
 * @param out_response 실행 결과 요약 구조체다.
 * @return 성공 시 EXECUTE_OK, 실패 시 EXECUTE_ERROR_* 를 반환한다.
 */
int execute(const Statement *statement,
            const char *db_dir,
            FILE *out,
            ExecuteResponse *out_response);

#endif
