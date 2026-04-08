#ifndef MAIN_H
#define MAIN_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

/**
 * @brief 입력 한 줄을 처리한 결과 종류를 나타낸다.
 */
typedef enum {
    LINE_ACTION_SKIP,
    LINE_ACTION_PRINT,
    LINE_ACTION_ERROR,
    LINE_ACTION_EXIT
} LineActionType;

/**
 * @brief 한 줄 처리 결과 데이터를 담는다.
 */
typedef struct {
    LineActionType type;
    const char *message;
} LineAction;

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
 * @brief 문자열 끝의 개행 문자를 제거한다.
 * @param line 수정할 문자열
 * @param length getline이 반환한 문자열 길이
 * @return 개행 제거 후 문자열 길이
 */
size_t trim_newline(char *line, ssize_t length);

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
 * @brief 입력 한 줄을 해석해 다음 동작을 결정한다.
 * @param line 개행이 제거된 입력 문자열
 * @return 입력 처리 결과
 */
LineAction evaluate_line(char *line);

/**
 * @brief 입력 스트림을 읽어 출력 스트림으로 결과를 기록한다.
 * @param input_stream 입력 스트림
 * @param output_stream 표준 출력용 스트림
 * @param error_stream 표준 에러용 스트림
 * @return 프로그램 종료 코드
 */
int run_repl(FILE *input_stream, FILE *output_stream, FILE *error_stream);

#endif
