#include "main.h"
#include "parser_internal.h"
#include <stdbool.h>
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

/**
 * @brief 실행에 필요한 메타데이터를 로드한다.
 * @return 메타데이터 로딩 결과
 */
MetadataLoadResult load_metadata(void) {
    static const char *const users_column_names[] = {"id", "name", "role"};
    static const ColumnType users_column_types[] = {COLUMN_TYPE_NUMBER, COLUMN_TYPE_TEXT, COLUMN_TYPE_TEXT};
    static const char *const logs_column_names[] = {"message", "level", "created_at"};
    static const ColumnType logs_column_types[] = {COLUMN_TYPE_TEXT, COLUMN_TYPE_TEXT, COLUMN_TYPE_NUMBER};
    MetadataLoadResult result;

    result.metadata.tables = calloc(2, sizeof(TableMetadata));
    result.metadata.table_count = 0;
    result.error_message = NULL;

    if (result.metadata.tables == NULL) {
        result.error_message = "out of memory";
        return result;
    }

    if (!init_table_metadata(&result.metadata.tables[0], "users", "users.csv", users_column_names, users_column_types, 3)) {
        result.error_message = "out of memory";
        free_metadata(&result.metadata);
        return result;
    }
    result.metadata.table_count = 1;

    if (!init_table_metadata(&result.metadata.tables[1], "logs", "logs.csv", logs_column_names, logs_column_types, 3)) {
        result.error_message = "out of memory";
        free_metadata(&result.metadata);
        return result;
    }

    result.metadata.table_count = 2;
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

    return value_type == VALUE_TYPE_STRING || value_type == VALUE_TYPE_IDENTIFIER;
}
