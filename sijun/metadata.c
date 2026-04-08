#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include "parser_internal.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool init_table_metadata(
    TableMetadata *table,
    const char *table_name,
    const char *data_file_path,
    const char *const *column_names,
    const ColumnType *column_types,
    size_t column_count
);
static void free_table_metadata(TableMetadata *table);
static bool duplicate_text(char **out, const char *text);
static bool is_value_compatible(ColumnType column_type, ValueType value_type);
static bool build_metadata_file_path(char **out, const char *db_directory);
static bool load_metadata_rows(FILE *stream, DatabaseMetadata *metadata);
static bool parse_metadata_line(char *line, char **table_name, char **column_name, ColumnType *column_type);
static bool append_metadata_column(
    DatabaseMetadata *metadata,
    const char *table_name,
    const char *column_name,
    ColumnType column_type
);
static bool ensure_table_slot(DatabaseMetadata *metadata, const char *table_name, TableMetadata **out_table);
static bool has_any_table(const DatabaseMetadata *metadata);

/**
 * @brief 실행에 필요한 메타데이터를 로드한다.
 * @return 메타데이터 로딩 결과
 */
MetadataLoadResult load_metadata(void) {
    return load_metadata_from_directory("sijun/db");
}

/**
 * @brief 지정한 DB 디렉터리에서 메타데이터를 로드한다.
 * @param db_directory metadata.csv가 있는 디렉터리 경로
 * @return 메타데이터 로딩 결과
 */
MetadataLoadResult load_metadata_from_directory(const char *db_directory) {
    char *metadata_file_path = NULL;
    FILE *stream = NULL;
    MetadataLoadResult result;

    result.metadata.tables = NULL;
    result.metadata.table_count = 0;
    result.error_message = NULL;

    if (db_directory == NULL) {
        result.error_message = "invalid metadata directory";
        return result;
    }

    if (!build_metadata_file_path(&metadata_file_path, db_directory)) {
        result.error_message = "out of memory";
        return result;
    }

    stream = fopen(metadata_file_path, "r");
    free(metadata_file_path);
    if (stream == NULL) {
        result.error_message = "failed to open metadata.csv";
        return result;
    }

    if (!load_metadata_rows(stream, &result.metadata)) {
        fclose(stream);
        free_metadata(&result.metadata);
        result.error_message = "out of memory";
        return result;
    }

    fclose(stream);

    if (!has_any_table(&result.metadata)) {
        free_metadata(&result.metadata);
        result.error_message = "no table in metadata";
        return result;
    }

    return result;
}

/**
 * @brief load_metadata()가 할당한 메모리를 해제한다.
 * @param metadata 해제할 메타데이터
 */
void free_metadata(DatabaseMetadata *metadata) {
    size_t index;

    if (metadata == NULL) {
        return;
    }

    for (index = 0; index < metadata->table_count; ++index) {
        free_table_metadata(&metadata->tables[index]);
    }

    free(metadata->tables);
    metadata->tables = NULL;
    metadata->table_count = 0;
}

/**
 * @brief 이름으로 테이블 메타데이터를 찾는다.
 * @param metadata 검색 대상 메타데이터
 * @param table_name 찾을 테이블명
 * @return 찾은 테이블 메타데이터, 없으면 NULL
 */
const TableMetadata *find_table_metadata(const DatabaseMetadata *metadata, const char *table_name) {
    size_t index;

    if (metadata == NULL || table_name == NULL) {
        return NULL;
    }

    for (index = 0; index < metadata->table_count; ++index) {
        if (strcmp(metadata->tables[index].name, table_name) == 0) {
            return &metadata->tables[index];
        }
    }

    return NULL;
}

/**
 * @brief 파싱된 쿼리를 메타데이터 기준으로 검증한다.
 * @param metadata 검증에 사용할 메타데이터
 * @param result 파싱 결과
 * @return 의미 검증 결과
 */
SemanticCheckResult validate_query_against_metadata(const DatabaseMetadata *metadata, const ParseResult *result) {
    const TableMetadata *table;
    size_t index;
    SemanticCheckResult check;

    check.ok = false;
    check.error_message = "invalid query";

    if (metadata == NULL || result == NULL || result->table_name == NULL) {
        return check;
    }

    table = find_table_metadata(metadata, result->table_name);
    if (table == NULL) {
        check.error_message = "unknown table";
        return check;
    }

    if (result->type == QUERY_TYPE_SELECT) {
        check.ok = true;
        check.error_message = NULL;
        return check;
    }

    if (result->type != QUERY_TYPE_INSERT) {
        return check;
    }

    if (result->value_count != table->column_count) {
        check.error_message = "column count does not match value count";
        return check;
    }

    for (index = 0; index < result->value_count; ++index) {
        if (!is_value_compatible(table->columns[index].type, result->values[index].type)) {
            check.error_message = "value type does not match column type";
            return check;
        }
    }

    check.ok = true;
    check.error_message = NULL;
    return check;
}

/**
 * @brief 하드코딩된 테이블 메타데이터 하나를 초기화한다.
 * @param table 초기화할 테이블 메타데이터
 * @param table_name 테이블명
 * @param data_file_path 데이터 파일 경로
 * @param column_names 컬럼명 배열
 * @param column_types 컬럼 타입 배열
 * @param column_count 컬럼 개수
 * @return 성공하면 1, 실패하면 0
 */
static bool init_table_metadata(
    TableMetadata *table,
    const char *table_name,
    const char *data_file_path,
    const char *const *column_names,
    const ColumnType *column_types,
    size_t column_count
) {
    size_t index;

    table->name = NULL;
    table->data_file_path = NULL;
    table->columns = NULL;
    table->column_count = 0;

    if (!duplicate_text(&table->name, table_name)) {
        return false;
    }

    if (!duplicate_text(&table->data_file_path, data_file_path)) {
        free_table_metadata(table);
        return false;
    }

    table->columns = calloc(column_count, sizeof(ColumnMetadata));
    if (table->columns == NULL) {
        free_table_metadata(table);
        return false;
    }

    for (index = 0; index < column_count; ++index) {
        if (!duplicate_text(&table->columns[index].name, column_names[index])) {
            table->column_count = column_count;
            free_table_metadata(table);
            return false;
        }
        table->columns[index].type = column_types[index];
    }

    table->column_count = column_count;
    return true;
}

/**
 * @brief 테이블 메타데이터 하나가 가진 메모리를 해제한다.
 * @param table 해제할 테이블 메타데이터
 */
static void free_table_metadata(TableMetadata *table) {
    size_t index;

    if (table == NULL) {
        return;
    }

    for (index = 0; index < table->column_count; ++index) {
        free(table->columns[index].name);
        table->columns[index].name = NULL;
    }

    free(table->columns);
    free(table->name);
    free(table->data_file_path);
    table->columns = NULL;
    table->name = NULL;
    table->data_file_path = NULL;
    table->column_count = 0;
}

/**
 * @brief 문자열을 heap에 복사한다.
 * @param out 복사본을 받을 포인터
 * @param text 복사할 문자열
 * @return 성공하면 1, 실패하면 0
 */
static bool duplicate_text(char **out, const char *text) {
    *out = copy_range(text, strlen(text));
    return *out != NULL;
}

/**
 * @brief 컬럼 타입과 SQL 값 타입이 호환되는지 검사한다.
 * @param column_type 컬럼 타입
 * @param value_type SQL 값 타입
 * @return 호환되면 1, 아니면 0
 */
static bool is_value_compatible(ColumnType column_type, ValueType value_type) {
    if (column_type == COLUMN_TYPE_NUMBER) {
        return value_type == VALUE_TYPE_NUMBER;
    }

    return value_type == VALUE_TYPE_STRING;
}

/**
 * @brief metadata.csv 절대 경로 문자열을 만든다.
 * @param out 결과 경로를 받을 포인터
 * @param db_directory DB 디렉터리 경로
 * @return 성공하면 1, 실패하면 0
 */
static bool build_metadata_file_path(char **out, const char *db_directory) {
    size_t directory_length = strlen(db_directory);
    size_t needs_separator = directory_length > 0 && db_directory[directory_length - 1] != '/';
    const char *file_name = "metadata.csv";
    size_t file_name_length = strlen(file_name);

    *out = malloc(directory_length + needs_separator + file_name_length + 1);
    if (*out == NULL) {
        return false;
    }

    memcpy(*out, db_directory, directory_length);
    if (needs_separator) {
        (*out)[directory_length] = '/';
    }
    memcpy(*out + directory_length + needs_separator, file_name, file_name_length);
    (*out)[directory_length + needs_separator + file_name_length] = '\0';
    return true;
}

/**
 * @brief metadata.csv의 모든 row를 읽어 메타데이터 구조로 만든다.
 * @param stream 읽을 스트림
 * @param metadata 결과 메타데이터
 * @return 성공하면 1, 실패하면 0
 */
static bool load_metadata_rows(FILE *stream, DatabaseMetadata *metadata) {
    char *line = NULL;
    size_t capacity = 0;
    ssize_t line_length;

    while ((line_length = getline(&line, &capacity, stream)) != -1) {
        char *table_name = NULL;
        char *column_name = NULL;
        ColumnType column_type;

        while (line_length > 0 && (line[line_length - 1] == '\n' || line[line_length - 1] == '\r')) {
            line[--line_length] = '\0';
        }

        if (line[0] == '\0') {
            continue;
        }

        if (!parse_metadata_line(line, &table_name, &column_name, &column_type)) {
            free(line);
            return false;
        }

        if (!append_metadata_column(metadata, table_name, column_name, column_type)) {
            free(table_name);
            free(column_name);
            free(line);
            return false;
        }

        free(table_name);
        free(column_name);
    }

    free(line);
    return true;
}

/**
 * @brief metadata.csv 한 줄을 분해한다.
 * @param line 수정 가능한 입력 한 줄
 * @param table_name 테이블명을 받을 포인터
 * @param column_name 컬럼명을 받을 포인터
 * @param column_type 컬럼 타입을 받을 포인터
 * @return 성공하면 1, 실패하면 0
 */
static bool parse_metadata_line(char *line, char **table_name, char **column_name, ColumnType *column_type) {
    char *first_comma = strchr(line, ',');
    char *second_comma;

    if (first_comma == NULL) {
        return false;
    }

    *first_comma = '\0';
    second_comma = strchr(first_comma + 1, ',');
    if (second_comma == NULL || strchr(second_comma + 1, ',') != NULL) {
        return false;
    }
    *second_comma = '\0';

    if (!duplicate_text(table_name, line)) {
        return false;
    }
    if (!duplicate_text(column_name, first_comma + 1)) {
        free(*table_name);
        *table_name = NULL;
        return false;
    }

    if (strcmp(second_comma + 1, "NUMBER") == 0) {
        *column_type = COLUMN_TYPE_NUMBER;
        return true;
    }
    if (strcmp(second_comma + 1, "TEXT") == 0) {
        *column_type = COLUMN_TYPE_TEXT;
        return true;
    }

    free(*table_name);
    free(*column_name);
    *table_name = NULL;
    *column_name = NULL;
    return false;
}

/**
 * @brief 메타데이터에 컬럼 하나를 추가한다.
 * @param metadata 수정할 메타데이터
 * @param table_name 컬럼이 속한 테이블명
 * @param column_name 추가할 컬럼명
 * @param column_type 추가할 컬럼 타입
 * @return 성공하면 1, 실패하면 0
 */
static bool append_metadata_column(
    DatabaseMetadata *metadata,
    const char *table_name,
    const char *column_name,
    ColumnType column_type
) {
    TableMetadata *table;
    ColumnMetadata *next_columns;

    if (!ensure_table_slot(metadata, table_name, &table)) {
        return false;
    }

    next_columns = realloc(table->columns, sizeof(ColumnMetadata) * (table->column_count + 1));
    if (next_columns == NULL) {
        return false;
    }

    table->columns = next_columns;
    table->columns[table->column_count].name = NULL;
    table->columns[table->column_count].type = column_type;

    if (!duplicate_text(&table->columns[table->column_count].name, column_name)) {
        return false;
    }

    table->column_count++;
    return true;
}

/**
 * @brief 지정한 테이블명의 메타데이터 슬롯을 확보한다.
 * @param metadata 수정할 메타데이터
 * @param table_name 찾거나 만들 테이블명
 * @param out_table 찾은 테이블을 받을 포인터
 * @return 성공하면 1, 실패하면 0
 */
static bool ensure_table_slot(DatabaseMetadata *metadata, const char *table_name, TableMetadata **out_table) {
    const TableMetadata *found_table = find_table_metadata(metadata, table_name);
    TableMetadata *next_tables;
    char *data_file_path;
    size_t table_name_length;

    if (found_table != NULL) {
        *out_table = &metadata->tables[found_table - metadata->tables];
        return true;
    }

    next_tables = realloc(metadata->tables, sizeof(TableMetadata) * (metadata->table_count + 1));
    if (next_tables == NULL) {
        return false;
    }

    metadata->tables = next_tables;
    *out_table = &metadata->tables[metadata->table_count];
    (*out_table)->name = NULL;
    (*out_table)->data_file_path = NULL;
    (*out_table)->columns = NULL;
    (*out_table)->column_count = 0;

    table_name_length = strlen(table_name);
    data_file_path = malloc(table_name_length + 5);
    if (data_file_path == NULL) {
        return false;
    }
    memcpy(data_file_path, table_name, table_name_length);
    memcpy(data_file_path + table_name_length, ".csv", 5);

    if (!init_table_metadata(*out_table, table_name, data_file_path, NULL, NULL, 0)) {
        free(data_file_path);
        return false;
    }

    free(data_file_path);
    metadata->table_count++;
    return true;
}

/**
 * @brief 로드된 메타데이터에 테이블이 하나라도 있는지 검사한다.
 * @param metadata 검사할 메타데이터
 * @return 하나 이상 있으면 1, 아니면 0
 */
static bool has_any_table(const DatabaseMetadata *metadata) {
    return metadata != NULL && metadata->table_count > 0;
}
