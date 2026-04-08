#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "repl_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool starts_with_sql_keyword(const char *line);
static bool starts_with_keyword(const char *line, const char *keyword);
static bool print_prompt(FILE *output_stream);

/**
 * @brief 입력 스트림을 읽어 출력 스트림으로 결과를 기록한다.
 * @param input_stream 입력 스트림
 * @param output_stream 표준 출력용 스트림
 * @param error_stream 표준 에러용 스트림
 * @return 프로그램 종료 코드
 */
int run_repl(FILE *input_stream, FILE *output_stream, FILE *error_stream) {
    return run_repl_with_database_dir(input_stream, output_stream, error_stream, "sijun/db");
}

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
) {
    char *line = NULL;
    size_t capacity = 0;
    ssize_t line_length;
    MetadataLoadResult metadata_result = load_metadata_from_directory(db_directory);

    if (metadata_result.error_message != NULL) {
        fprintf(error_stream, "Metadata error: %s\n", metadata_result.error_message);
        return ERR;
    }

    while (1) {
        LineAction action;

        if (!print_prompt(output_stream)) {
            free_metadata(&metadata_result.metadata);
            free(line);
            return ERR;
        }

        line_length = getline(&line, &capacity, input_stream);
        if (line_length == -1) {
            break;
        }

        trim_newline(line, line_length);
        action = evaluate_line(line);

        if (action.type == LINE_ACTION_SKIP) {
            continue;
        }
        if (action.type == LINE_ACTION_EXIT) {
            break;
        }
        if (action.type == LINE_ACTION_ERROR) {
            fprintf(error_stream, "Unknown command: %s\n", action.message);
            continue;
        }

        {
            ParseResult parse_result = parse(line);

            if (parse_result.error_message != NULL) {
                if (starts_with_sql_keyword(line)) {
                    fprintf(error_stream, "Parse error: %s\n", parse_result.error_message);
                    free_parse_result(&parse_result);
                    continue;
                }
            } else {
                SemanticCheckResult check = validate_query_against_metadata(&metadata_result.metadata, &parse_result);

                if (!check.ok) {
                    fprintf(error_stream, "Semantic error: %s\n", check.error_message);
                    free_parse_result(&parse_result);
                    continue;
                }

                {
                    QueryExecutionResult execution = execute_query(
                        &metadata_result.metadata,
                        &parse_result,
                        db_directory,
                        output_stream
                    );

                    if (!execution.ok) {
                        fprintf(error_stream, "Execution error: %s\n", execution.error_message);
                        free_parse_result(&parse_result);
                        continue;
                    }
                }

                free_parse_result(&parse_result);
                continue;
            }

            free_parse_result(&parse_result);
        }

        fprintf(output_stream, "해석할 수 없는 입력입니다.\n");
    }

    free_metadata(&metadata_result.metadata);
    free(line);
    return OK;
}

/**
 * @brief 입력이 SQL 키워드로 시작하는지 검사한다.
 * @param line 검사할 입력 문자열
 * @return SQL 키워드로 시작하면 1, 아니면 0
 */
static bool starts_with_sql_keyword(const char *line) {
    return starts_with_keyword(line, "select") || starts_with_keyword(line, "insert");
}

/**
 * @brief 앞 공백을 무시하고 지정한 키워드로 시작하는지 검사한다.
 * @param line 검사할 입력 문자열
 * @param keyword 기대 키워드
 * @return 시작하면 1, 아니면 0
 */
static bool starts_with_keyword(const char *line, const char *keyword) {
    size_t keyword_length;

    while (*line != '\0' && isspace((unsigned char) *line)) {
        line++;
    }

    keyword_length = strlen(keyword);
    if (strncmp(line, keyword, keyword_length) != 0) {
        return false;
    }

    return line[keyword_length] == '\0' || isspace((unsigned char) line[keyword_length]);
}

/**
 * @brief 출력 스트림에 프롬프트를 기록한다.
 * @param output_stream 기록할 출력 스트림
 * @return 성공하면 1, 실패하면 0
 */
static bool print_prompt(FILE *output_stream) {
    return fputs("> ", output_stream) != EOF;
}

/**
 * @brief 입력 한 줄을 해석해 다음 동작을 결정한다.
 * @param line 개행이 제거된 입력 문자열
 * @return 입력 처리 결과
 */
LineAction evaluate_line(char *line) {
    LineAction action;

    if (line[0] == '\0') {
        action.type = LINE_ACTION_SKIP;
        action.message = NULL;
        return action;
    }

    if (line[0] == '.') {
        if (strcmp(line, ".exit") == 0) {
            action.type = LINE_ACTION_EXIT;
            action.message = NULL;
            return action;
        }

        action.type = LINE_ACTION_ERROR;
        action.message = line;
        return action;
    }

    action.type = LINE_ACTION_PRINT;
    action.message = line;
    return action;
}

/**
 * @brief 문자열 끝의 개행 문자를 제거한다.
 * @param line 수정할 문자열
 * @param length getline이 반환한 문자열 길이
 * @return 개행 제거 후 문자열 길이
 */
size_t trim_newline(char *line, ssize_t length) {
    size_t trimmed_length = (size_t) length;

    if (trimmed_length > 0 && line[trimmed_length - 1] == '\n') {
        line[--trimmed_length] = '\0';
    }

    if (trimmed_length > 0 && line[trimmed_length - 1] == '\r') {
        line[--trimmed_length] = '\0';
    }

    return trimmed_length;
}
