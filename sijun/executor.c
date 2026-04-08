#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "parser_internal.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char **cells;
    size_t cell_count;
} CsvRow;

typedef struct {
    CsvRow *rows;
    size_t row_count;
} CsvTable;

static QueryExecutionResult execute_insert_query(
    const TableMetadata *table,
    const ParseResult *result,
    const char *db_directory,
    FILE *output_stream
);
static QueryExecutionResult execute_select_query(
    const TableMetadata *table,
    const char *db_directory,
    FILE *output_stream
);
static bool build_table_file_path(char **out, const char *db_directory, const char *file_name);
static bool append_insert_row(FILE *stream, const ParseResult *result);
static bool load_csv_table(FILE *stream, CsvTable *table);
static bool parse_csv_line(char *line, CsvRow *row);
static bool append_csv_cell(CsvRow *row, const char *text);
static void free_csv_row(CsvRow *row);
static void free_csv_table(CsvTable *table);
static bool validate_csv_table(const CsvTable *csv_table, const TableMetadata *table);
static bool print_select_table(const CsvTable *csv_table, const TableMetadata *table, FILE *output_stream);
static size_t max_size(size_t left, size_t right);

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
) {
    const TableMetadata *table;
    QueryExecutionResult execution;

    execution.ok = false;
    execution.error_message = "invalid query";

    if (metadata == NULL || result == NULL || db_directory == NULL || output_stream == NULL) {
        return execution;
    }

    table = find_table_metadata(metadata, result->table_name);
    if (table == NULL) {
        execution.error_message = "unknown table";
        return execution;
    }

    if (result->type == QUERY_TYPE_INSERT) {
        return execute_insert_query(table, result, db_directory, output_stream);
    }
    if (result->type == QUERY_TYPE_SELECT) {
        return execute_select_query(table, db_directory, output_stream);
    }

    return execution;
}

/**
 * @brief insert 쿼리를 대상 CSV 파일에 append한다.
 * @param table 대상 테이블 메타데이터
 * @param result 실행할 파싱 결과
 * @param db_directory CSV 파일이 있는 디렉터리 경로
 * @param output_stream 실행 결과를 기록할 출력 스트림
 * @return 실행 결과
 */
static QueryExecutionResult execute_insert_query(
    const TableMetadata *table,
    const ParseResult *result,
    const char *db_directory,
    FILE *output_stream
) {
    char *file_path = NULL;
    FILE *stream = NULL;
    QueryExecutionResult execution;

    execution.ok = false;
    execution.error_message = "failed to open table file";

    if (!build_table_file_path(&file_path, db_directory, table->data_file_path)) {
        execution.error_message = "out of memory";
        return execution;
    }

    stream = fopen(file_path, "a");
    free(file_path);
    if (stream == NULL) {
        return execution;
    }

    if (!append_insert_row(stream, result)) {
        fclose(stream);
        execution.error_message = "failed to write table file";
        return execution;
    }

    fclose(stream);

    if (fprintf(output_stream, "1 row inserted\n") < 0) {
        execution.error_message = "failed to write output";
        return execution;
    }

    execution.ok = true;
    execution.error_message = NULL;
    return execution;
}

/**
 * @brief select 쿼리를 대상 CSV 파일에서 읽어 표 형태로 출력한다.
 * @param table 대상 테이블 메타데이터
 * @param db_directory CSV 파일이 있는 디렉터리 경로
 * @param output_stream 실행 결과를 기록할 출력 스트림
 * @return 실행 결과
 */
static QueryExecutionResult execute_select_query(
    const TableMetadata *table,
    const char *db_directory,
    FILE *output_stream
) {
    char *file_path = NULL;
    FILE *stream = NULL;
    CsvTable csv_table;
    QueryExecutionResult execution;

    csv_table.rows = NULL;
    csv_table.row_count = 0;
    execution.ok = false;
    execution.error_message = "failed to open table file";

    if (!build_table_file_path(&file_path, db_directory, table->data_file_path)) {
        execution.error_message = "out of memory";
        return execution;
    }

    stream = fopen(file_path, "r");
    free(file_path);
    if (stream == NULL) {
        return execution;
    }

    if (!load_csv_table(stream, &csv_table)) {
        fclose(stream);
        execution.error_message = "out of memory";
        return execution;
    }
    fclose(stream);

    if (!validate_csv_table(&csv_table, table)) {
        free_csv_table(&csv_table);
        execution.error_message = "row column count does not match metadata";
        return execution;
    }

    if (!print_select_table(&csv_table, table, output_stream)) {
        free_csv_table(&csv_table);
        execution.error_message = "failed to write output";
        return execution;
    }

    free_csv_table(&csv_table);
    execution.ok = true;
    execution.error_message = NULL;
    return execution;
}

/**
 * @brief DB 디렉터리와 파일명을 합쳐 새 경로 문자열을 만든다.
 * @param out 결과 경로를 받을 포인터
 * @param db_directory DB 디렉터리 경로
 * @param file_name 파일 이름
 * @return 성공하면 1, 실패하면 0
 */
static bool build_table_file_path(char **out, const char *db_directory, const char *file_name) {
    size_t directory_length = strlen(db_directory);
    size_t file_length = strlen(file_name);
    size_t needs_separator = directory_length > 0 && db_directory[directory_length - 1] != '/';

    *out = malloc(directory_length + needs_separator + file_length + 1);
    if (*out == NULL) {
        return false;
    }

    memcpy(*out, db_directory, directory_length);
    if (needs_separator) {
        (*out)[directory_length] = '/';
    }
    memcpy(*out + directory_length + needs_separator, file_name, file_length);
    (*out)[directory_length + needs_separator + file_length] = '\0';
    return true;
}

/**
 * @brief insert 값 목록을 CSV 한 줄로 기록한다.
 * @param stream 기록할 스트림
 * @param result 실행할 파싱 결과
 * @return 성공하면 1, 실패하면 0
 */
static bool append_insert_row(FILE *stream, const ParseResult *result) {
    size_t index;

    for (index = 0; index < result->value_count; ++index) {
        if (index > 0 && fputc(',', stream) == EOF) {
            return false;
        }
        if (fputs(result->values[index].text, stream) == EOF) {
            return false;
        }
    }

    if (fputc('\n', stream) == EOF) {
        return false;
    }

    return true;
}

/**
 * @brief CSV 파일 전체를 메모리로 읽는다.
 * @param stream 읽을 스트림
 * @param table 결과 테이블
 * @return 성공하면 1, 실패하면 0
 */
static bool load_csv_table(FILE *stream, CsvTable *table) {
    char *line = NULL;
    size_t capacity = 0;
    ssize_t line_length;

    while ((line_length = getline(&line, &capacity, stream)) != -1) {
        CsvRow row;
        CsvRow *next_rows;

        row.cells = NULL;
        row.cell_count = 0;

        while (line_length > 0 && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r')) {
            line[--line_length] = '\0';
        }

        if (!parse_csv_line(line, &row)) {
            free(line);
            free_csv_table(table);
            return false;
        }

        next_rows = realloc(table->rows, sizeof(CsvRow) * (table->row_count + 1));
        if (next_rows == NULL) {
            free_csv_row(&row);
            free(line);
            free_csv_table(table);
            return false;
        }

        table->rows = next_rows;
        table->rows[table->row_count++] = row;
    }

    free(line);
    return true;
}

/**
 * @brief comma로 구분된 한 줄을 셀 배열로 파싱한다.
 * @param line 수정 가능한 입력 한 줄
 * @param row 결과 row
 * @return 성공하면 1, 실패하면 0
 */
static bool parse_csv_line(char *line, CsvRow *row) {
    char *cursor = line;

    if (line[0] == '\0') {
        return true;
    }

    while (1) {
        char *comma = strchr(cursor, ',');

        if (comma != NULL) {
            *comma = '\0';
        }

        if (!append_csv_cell(row, cursor)) {
            free_csv_row(row);
            return false;
        }

        if (comma == NULL) {
            break;
        }

        cursor = comma + 1;
    }

    return true;
}

/**
 * @brief CSV row에 셀 하나를 추가한다.
 * @param row 수정할 row
 * @param text 셀 문자열
 * @return 성공하면 1, 실패하면 0
 */
static bool append_csv_cell(CsvRow *row, const char *text) {
    char **next_cells = realloc(row->cells, sizeof(char *) * (row->cell_count + 1));

    if (next_cells == NULL) {
        return false;
    }

    row->cells = next_cells;
    row->cells[row->cell_count] = copy_range(text, strlen(text));
    if (row->cells[row->cell_count] == NULL) {
        return false;
    }

    row->cell_count++;
    return true;
}

/**
 * @brief CSV row 하나가 가진 메모리를 해제한다.
 * @param row 해제할 row
 */
static void free_csv_row(CsvRow *row) {
    size_t index;

    for (index = 0; index < row->cell_count; ++index) {
        free(row->cells[index]);
    }

    free(row->cells);
    row->cells = NULL;
    row->cell_count = 0;
}

/**
 * @brief CSV 테이블 전체 메모리를 해제한다.
 * @param table 해제할 테이블
 */
static void free_csv_table(CsvTable *table) {
    size_t index;

    for (index = 0; index < table->row_count; ++index) {
        free_csv_row(&table->rows[index]);
    }

    free(table->rows);
    table->rows = NULL;
    table->row_count = 0;
}

/**
 * @brief CSV row마다 컬럼 개수가 메타데이터와 맞는지 검사한다.
 * @param csv_table 검사할 CSV 테이블
 * @param table 기준 메타데이터
 * @return 모두 맞으면 1, 아니면 0
 */
static bool validate_csv_table(const CsvTable *csv_table, const TableMetadata *table) {
    size_t index;

    for (index = 0; index < csv_table->row_count; ++index) {
        if (csv_table->rows[index].cell_count != table->column_count) {
            return false;
        }
    }

    return true;
}

/**
 * @brief select 결과를 ASCII 표 형태로 출력한다.
 * @param csv_table 출력할 CSV 데이터
 * @param table 기준 메타데이터
 * @param output_stream 결과를 기록할 스트림
 * @return 성공하면 1, 실패하면 0
 */
static bool print_select_table(const CsvTable *csv_table, const TableMetadata *table, FILE *output_stream) {
    size_t *widths;
    size_t row_index;
    size_t column_index;

    widths = calloc(table->column_count, sizeof(size_t));
    if (widths == NULL) {
        return false;
    }

    for (column_index = 0; column_index < table->column_count; ++column_index) {
        widths[column_index] = strlen(table->columns[column_index].name);
    }

    for (row_index = 0; row_index < csv_table->row_count; ++row_index) {
        for (column_index = 0; column_index < table->column_count; ++column_index) {
            widths[column_index] = max_size(widths[column_index], strlen(csv_table->rows[row_index].cells[column_index]));
        }
    }

    for (column_index = 0; column_index < table->column_count; ++column_index) {
        if (fprintf(output_stream, "%-*s", (int) widths[column_index], table->columns[column_index].name) < 0) {
            free(widths);
            return false;
        }
        if (column_index + 1 < table->column_count && fputs(" | ", output_stream) == EOF) {
            free(widths);
            return false;
        }
    }
    if (fputc('\n', output_stream) == EOF) {
        free(widths);
        return false;
    }

    for (column_index = 0; column_index < table->column_count; ++column_index) {
        size_t dash_index;

        for (dash_index = 0; dash_index < widths[column_index]; ++dash_index) {
            if (fputc('-', output_stream) == EOF) {
                free(widths);
                return false;
            }
        }
        if (column_index + 1 < table->column_count && fputs("-+-", output_stream) == EOF) {
            free(widths);
            return false;
        }
    }
    if (fputc('\n', output_stream) == EOF) {
        free(widths);
        return false;
    }

    for (row_index = 0; row_index < csv_table->row_count; ++row_index) {
        for (column_index = 0; column_index < table->column_count; ++column_index) {
            if (fprintf(output_stream, "%-*s", (int) widths[column_index], csv_table->rows[row_index].cells[column_index]) < 0) {
                free(widths);
                return false;
            }
            if (column_index + 1 < table->column_count && fputs(" | ", output_stream) == EOF) {
                free(widths);
                return false;
            }
        }
        if (fputc('\n', output_stream) == EOF) {
            free(widths);
            return false;
        }
    }

    free(widths);
    return true;
}

/**
 * @brief 두 size_t 값 중 큰 값을 반환한다.
 * @param left 비교 값
 * @param right 비교 값
 * @return 더 큰 값
 */
static size_t max_size(size_t left, size_t right) {
    if (left > right) {
        return left;
    }

    return right;
}
