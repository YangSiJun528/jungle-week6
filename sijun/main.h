#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/**
 * @brief 프로그램 종료 코드를 나타낸다.
 */
typedef enum {
    OK = 0,
    ERR = 1
} ProgramExitCode;

/**
 * @brief SQL 쿼리 종류를 나타낸다.
 */
typedef enum {
    QUERY_TYPE_INVALID,
    QUERY_TYPE_SELECT,
    QUERY_TYPE_INSERT
} QueryType;

/**
 * @brief SQL 값 종류를 나타낸다.
 */
typedef enum {
    VALUE_TYPE_NUMBER,
    VALUE_TYPE_STRING,
    VALUE_TYPE_IDENTIFIER
} ValueType;

/**
 * @brief 파싱된 SQL 값을 담는다.
 */
typedef struct {
    ValueType type;
    char *text;
} SqlValue;

/**
 * @brief 파싱된 SQL 쿼리 결과를 담는다.
 */
typedef struct {
    QueryType type;
    char *table_name;
    SqlValue *values;
    size_t value_count;
    const char *error_message;
    size_t error_position;
} ParseResult;

/**
 * @brief 컬럼의 저장 타입을 나타낸다.
 */
typedef enum {
    COLUMN_TYPE_NUMBER,
    COLUMN_TYPE_TEXT
} ColumnType;

/**
 * @brief 테이블 컬럼 메타데이터를 담는다.
 */
typedef struct {
    char *name;
    ColumnType type;
} ColumnMetadata;

/**
 * @brief 테이블 메타데이터를 담는다.
 */
typedef struct {
    char *name;
    char *data_file_path;
    ColumnMetadata *columns;
    size_t column_count;
} TableMetadata;

/**
 * @brief 데이터베이스 메타데이터를 담는다.
 */
typedef struct {
    TableMetadata *tables;
    size_t table_count;
} DatabaseMetadata;

/**
 * @brief 메타데이터 로딩 결과를 담는다.
 */
typedef struct {
    DatabaseMetadata metadata;
    const char *error_message;
} MetadataLoadResult;

/**
 * @brief 쿼리 의미 검증 결과를 담는다.
 */
typedef struct {
    bool ok;
    const char *error_message;
} SemanticCheckResult;

/**
 * @brief 쿼리 실행 결과를 담는다.
 */
typedef struct {
    bool ok;
    const char *error_message;
} QueryExecutionResult;

/**
 * @brief 경량 SQL 문자열을 EBNF 규칙에 따라 파싱한다.
 * @param input 파싱할 SQL 문자열
 * @return 파싱 결과
 */
ParseResult parse(const char *input);

/**
 * @brief parse()가 할당한 메모리를 해제한다.
 * @param result 해제할 파싱 결과
 */
void free_parse_result(ParseResult *result);

/**
 * @brief 실행에 필요한 메타데이터를 로드한다.
 * @return 메타데이터 로딩 결과
 */
MetadataLoadResult load_metadata(void);

/**
 * @brief 지정한 DB 디렉터리에서 메타데이터를 로드한다.
 * @param db_directory metadata.csv가 있는 디렉터리 경로
 * @return 메타데이터 로딩 결과
 */
MetadataLoadResult load_metadata_from_directory(const char *db_directory);

/**
 * @brief load_metadata()가 할당한 메모리를 해제한다.
 * @param metadata 해제할 메타데이터
 */
void free_metadata(DatabaseMetadata *metadata);

/**
 * @brief 이름으로 테이블 메타데이터를 찾는다.
 * @param metadata 검색 대상 메타데이터
 * @param table_name 찾을 테이블명
 * @return 찾은 테이블 메타데이터, 없으면 NULL
 */
const TableMetadata *find_table_metadata(const DatabaseMetadata *metadata, const char *table_name);

/**
 * @brief 파싱된 쿼리를 메타데이터 기준으로 검증한다.
 * @param metadata 검증에 사용할 메타데이터
 * @param result 파싱 결과
 * @return 의미 검증 결과
 */
SemanticCheckResult validate_query_against_metadata(const DatabaseMetadata *metadata, const ParseResult *result);

/**
 * @brief 파싱된 쿼리를 CSV 기반 저장소에 실행한다.
 * @param metadata 실행에 사용할 메타데이터
 * @param result 실행할 파싱 결과
 * @param db_directory CSV 파일이 있는 디렉터리 경로
 * @param output_stream 실행 결과를 기록할 출력 스트림
 * @return 실행 결과
 */
QueryExecutionResult execute_query(
    const DatabaseMetadata *metadata,
    const ParseResult *result,
    const char *db_directory,
    FILE *output_stream
);

/**
 * @brief 입력 스트림을 읽어 출력 스트림으로 결과를 기록한다.
 * @param input_stream 입력 스트림
 * @param output_stream 표준 출력용 스트림
 * @param error_stream 표준 에러용 스트림
 * @return 프로그램 종료 코드
 */
int run_repl(FILE *input_stream, FILE *output_stream, FILE *error_stream);

/**
 * @brief 지정한 DB 디렉터리를 사용해 입력 스트림을 읽어 결과를 기록한다.
 * @param input_stream 입력 스트림
 * @param output_stream 표준 출력용 스트림
 * @param error_stream 표준 에러용 스트림
 * @param db_directory CSV 파일이 있는 디렉터리 경로
 * @return 프로그램 종료 코드
 */
int run_repl_with_database_dir(
    FILE *input_stream,
    FILE *output_stream,
    FILE *error_stream,
    const char *db_directory
);

#endif
