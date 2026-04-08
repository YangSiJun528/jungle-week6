#ifndef CSV_DIR
#define CSV_DIR "."
#endif

#include <unistd.h>

/* execute 단계는 parse가 만든 Statement를 실제 파일 입출력으로 연결한다. */
typedef enum {
    EXECUTE_OK,
    EXECUTE_ERROR
} ExecutionStatus;

typedef enum {
    EXECUTE_NONE,
    EXECUTE_OPEN_FAILED,
    EXECUTE_WRITE_FAILED,
    EXECUTE_EMPTY_RESULT
} ExecutionError;

typedef struct {
    ExecutionStatus status;
    ExecutionError error_code;
    char *output;
} ExecutionResult;

/* 화면에 보여줄 결과 문자열은 malloc으로 복사해 main이 free할 수 있게 만든다. */
static char *copy_text(const char *text) {
    size_t length = strlen(text);
    char *copy = malloc(length + 1);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, text, length + 1);
    return copy;
}

/* 공백만 정리할 때 쓰는 공통 helper다. CSV 셀은 따옴표를 벗기지 않기 때문에 trim_token과 분리한다. */
static char *trim_spaces(char *text) {
    char *end;

    while (*text == ' ' || *text == '\t') {
        text++;
    }

    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';

    return text;
}

/* 테이블 이름을 "{dir}/{table}.csv" 경로로 바꾼다. */
static char *build_csv_path(const char *table_name) {
    const char *extension = ".csv";
    size_t length = strlen(CSV_DIR) + 1 + strlen(table_name) + strlen(extension) + 1;
    char *path = malloc(length);

    if (path == NULL) {
        return NULL;
    }

    snprintf(path, length, "%s/%s%s", CSV_DIR, table_name, extension);
    return path;
}

/* SELECT는 파일 전체 내용을 한 번에 읽어 출력 문자열로 돌려준다. */
static char *read_file(const char *path) {
    FILE *file = fopen(path, "r");
    char *content;
    long size;

    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    content = malloc((size_t) size + 1);
    if (content == NULL) {
        fclose(file);
        return NULL;
    }

    if (size > 0 && fread(content, 1, (size_t) size, file) != (size_t) size) {
        free(content);
        fclose(file);
        return NULL;
    }

    content[size] = '\0';
    fclose(file);
    return content;
}

/* INSERT 값 하나를 CSV 셀로 쓰기 좋게 다듬는다.
 * 앞뒤 공백을 없애고, 작은따옴표 한 쌍이 있으면 바깥쪽만 벗긴다.
 */
static char *trim_token(char *token) {
    char *end;

    token = trim_spaces(token);
    end = token + strlen(token);

    if (strlen(token) >= 2 && token[0] == '\'' && end[-1] == '\'') {
        token++;
        end[-1] = '\0';
    }

    return token;
}

/* "a, b, 'Seoul, Korea'" 같은 values 문자열을 나눈다.
 * 작은따옴표 안의 쉼표는 값 내부 문자로 보고, 바깥쪽 쉼표만 구분자로 사용한다.
 */
static size_t split_values(char *values, char ***tokens_out) {
    size_t count = 1;
    size_t index = 0;
    char **tokens;
    char *cursor = values;
    bool in_quotes = false;

    while (*cursor != '\0') {
        if (*cursor == '\'') {
            in_quotes = !in_quotes;
        } else if (*cursor == ',' && !in_quotes) {
            count++;
        }
        cursor++;
    }

    tokens = malloc(sizeof(char *) * count);
    if (tokens == NULL) {
        *tokens_out = NULL;
        return 0;
    }

    cursor = values;
    tokens[index++] = cursor;
    while (*cursor != '\0') {
        if (*cursor == '\'') {
            in_quotes = !in_quotes;
            cursor++;
            continue;
        }

        if (*cursor == ',' && !in_quotes) {
            *cursor = '\0';
            cursor++;
            tokens[index++] = cursor;
            continue;
        }
        cursor++;
    }

    for (index = 0; index < count; index++) {
        tokens[index] = trim_token(tokens[index]);
    }

    *tokens_out = tokens;
    return count;
}

/* CSV 셀 안에 쉼표나 큰따옴표가 있으면 CSV 규칙에 맞게 큰따옴표로 감싼다. */
static char *escape_csv_cell(const char *cell) {
    bool needs_quotes = false;
    size_t extra = 0;
    size_t index;
    char *escaped;
    char *cursor;

    for (index = 0; cell[index] != '\0'; index++) {
        if (cell[index] == ',' || cell[index] == '"' || cell[index] == '\n') {
            needs_quotes = true;
        }
        if (cell[index] == '"') {
            extra++;
        }
    }

    escaped = malloc(index + extra + (needs_quotes ? 3 : 1));
    if (escaped == NULL) {
        return NULL;
    }

    cursor = escaped;
    if (needs_quotes) {
        *cursor++ = '"';
    }

    for (index = 0; cell[index] != '\0'; index++) {
        if (cell[index] == '"') {
            *cursor++ = '"';
        }
        *cursor++ = cell[index];
    }

    if (needs_quotes) {
        *cursor++ = '"';
    }
    *cursor = '\0';
    return escaped;
}

/* 열 개수에 맞춰 "col1,col2,..." 헤더 한 줄을 만든다. */
static char *build_header(size_t column_count) {
    size_t length = 1;
    size_t index;
    char *header;
    size_t used = 0;

    for (index = 0; index < column_count; index++) {
        length += 4 + 20;
    }

    header = malloc(length);
    if (header == NULL) {
        return NULL;
    }

    for (index = 0; index < column_count; index++) {
        used += (size_t) snprintf(header + used, length - used, "%scol%zu",
                                  index == 0 ? "" : ",", index + 1);
    }

    snprintf(header + used, length - used, "\n");
    return header;
}

/* 기존 CSV 헤더 첫 줄을 보고 현재 열 개수를 계산한다. */
static size_t count_header_columns(const char *header_line) {
    size_t count = 1;
    const char *cursor = header_line;

    if (*cursor == '\0') {
        return 0;
    }

    while (*cursor != '\0' && *cursor != '\n') {
        if (*cursor == ',') {
            count++;
        }
        cursor++;
    }

    return count;
}

/* 저장할 행을 현재 열 개수에 맞춰 만든다.
 * 값이 부족하면 뒤를 빈 칸으로 채워서 열 수를 맞춘다.
 */
static char *build_row(char **tokens, size_t token_count, size_t column_count) {
    size_t length = 2;
    size_t index;
    char *row;
    char **escaped_cells;
    size_t used = 0;

    escaped_cells = calloc(column_count, sizeof(char *));
    if (escaped_cells == NULL) {
        return NULL;
    }

    for (index = 0; index < column_count; index++) {
        escaped_cells[index] = escape_csv_cell(index < token_count ? tokens[index] : "");
        if (escaped_cells[index] == NULL) {
            while (index > 0) {
                free(escaped_cells[--index]);
            }
            free(escaped_cells);
            return NULL;
        }
        length += strlen(escaped_cells[index]) + 1;
    }

    row = malloc(length);
    if (row == NULL) {
        for (index = 0; index < column_count; index++) {
            free(escaped_cells[index]);
        }
        free(escaped_cells);
        return NULL;
    }

    for (index = 0; index < column_count; index++) {
        used += (size_t) snprintf(row + used, length - used, "%s%s",
                                  index == 0 ? "" : ",",
                                  escaped_cells[index]);
    }

    snprintf(row + used, length - used, "\n");
    for (index = 0; index < column_count; index++) {
        free(escaped_cells[index]);
    }
    free(escaped_cells);
    return row;
}

/* 표 출력에서는 각 셀 폭이 다르므로, 셀 내용 뒤를 공백으로 맞춘다. */
static char *append_padded_cell(char *dest, const char *cell, size_t width) {
    size_t length = strlen(cell);

    memcpy(dest, cell, length);
    dest += length;

    while (length < width) {
        *dest++ = ' ';
        length++;
    }

    return dest;
}

/* CSV 한 줄을 토큰으로 나눌 때는 큰따옴표 안의 쉼표를 셀 내용으로 유지한다. */
static size_t split_csv_line(char *line, char ***tokens_out) {
    size_t count = 1;
    size_t index = 0;
    char **tokens;
    char *read_cursor = line;
    char *write_cursor = line;
    bool in_quotes = false;

    while (*read_cursor != '\0') {
        if (*read_cursor == '"') {
            if (in_quotes && read_cursor[1] == '"') {
                read_cursor++;
            } else {
                in_quotes = !in_quotes;
            }
        } else if (*read_cursor == ',' && !in_quotes) {
            count++;
        }
        read_cursor++;
    }

    tokens = malloc(sizeof(char *) * count);
    if (tokens == NULL) {
        *tokens_out = NULL;
        return 0;
    }

    read_cursor = line;
    tokens[index++] = write_cursor;
    while (*read_cursor != '\0') {
        if (*read_cursor == '"') {
            if (in_quotes && read_cursor[1] == '"') {
                *write_cursor++ = '"';
                read_cursor += 2;
                continue;
            }
            in_quotes = !in_quotes;
            read_cursor++;
            continue;
        }

        if (*read_cursor == ',' && !in_quotes) {
            *write_cursor++ = '\0';
            read_cursor++;
            tokens[index++] = write_cursor;
            continue;
        }

        *write_cursor++ = *read_cursor++;
    }
    *write_cursor = '\0';

    for (index = 0; index < count; index++) {
        tokens[index] = trim_spaces(tokens[index]);
    }

    *tokens_out = tokens;
    return count;
}

/* 기존 CSV를 직접 "w"로 열면 쓰기 실패 시 원본을 잃을 수 있다.
 * 그래서 임시 파일에 완성된 내용을 먼저 쓰고, 성공하면 rename으로 교체한다.
 */
static bool write_text_atomic(const char *path, const char *text) {
    size_t length = strlen(path) + 16;
    char *temp_path = malloc(length);
    int file_descriptor;
    FILE *file;
    bool wrote_text = false;
    int close_result;
    bool success = false;

    if (temp_path == NULL) {
        return false;
    }

    snprintf(temp_path, length, "%s.tmp.XXXXXX", path);
    file_descriptor = mkstemp(temp_path);
    if (file_descriptor == -1) {
        free(temp_path);
        return false;
    }

    file = fdopen(file_descriptor, "w");
    if (file == NULL) {
        close(file_descriptor);
        unlink(temp_path);
        free(temp_path);
        return false;
    }

    if (fputs(text, file) != EOF && fflush(file) == 0) {
        wrote_text = true;
    }

    close_result = fclose(file);
    if (wrote_text && close_result == 0 && rename(temp_path, path) == 0) {
        success = true;
    }

    if (!success) {
        unlink(temp_path);
    }
    free(temp_path);
    return success;
}

/* 헤더, 기존 데이터 본문, 새 행을 합쳐서 한 번에 쓸 전체 파일 내용을 만든다. */
static char *build_file_content(const char *header, const char *body, const char *row) {
    size_t length = strlen(header) + strlen(body) + strlen(row) + 1;
    char *content = malloc(length);

    if (content == NULL) {
        return NULL;
    }

    snprintf(content, length, "%s%s%s", header, body, row);
    return content;
}

/* SELECT 결과는 CSV 원문 그대로보다 읽기 쉽게, 열 폭을 맞춘 표 문자열로 변환한다. */
static char *format_csv_as_table(const char *content) {
    char *buffer = copy_text(content);
    char *line;
    char *line_end;
    char ***rows = NULL;
    size_t *row_counts = NULL;
    size_t *column_widths = NULL;
    size_t row_count = 0;
    size_t max_columns = 0;
    size_t index;
    size_t column;
    size_t total_length = 1;
    char *table = NULL;
    char *cursor;

    if (buffer == NULL) {
        return NULL;
    }

    line = buffer;
    while (*line != '\0') {
        char **tokens;
        size_t token_count;

        line_end = strchr(line, '\n');
        if (line_end != NULL) {
            *line_end = '\0';
        }

        tokens = NULL;
        token_count = split_csv_line(line, &tokens);
        if (tokens == NULL || token_count == 0) {
            free(tokens);
            goto cleanup;
        }

        rows = realloc(rows, sizeof(char **) * (row_count + 1));
        row_counts = realloc(row_counts, sizeof(size_t) * (row_count + 1));
        if (rows == NULL || row_counts == NULL) {
            free(tokens);
            goto cleanup;
        }

        if (token_count > max_columns) {
            size_t *new_widths = realloc(column_widths, sizeof(size_t) * token_count);

            if (new_widths == NULL) {
                free(tokens);
                goto cleanup;
            }

            column_widths = new_widths;
            for (column = max_columns; column < token_count; column++) {
                column_widths[column] = 0;
            }
            max_columns = token_count;
        }

        rows[row_count] = tokens;
        row_counts[row_count] = token_count;
        for (column = 0; column < token_count; column++) {
            size_t cell_length = strlen(tokens[column]);

            if (cell_length > column_widths[column]) {
                column_widths[column] = cell_length;
            }
        }
        row_count++;

        if (line_end == NULL) {
            break;
        }
        line = line_end + 1;
    }

    for (index = 0; index < row_count; index++) {
        for (column = 0; column < max_columns; column++) {
            total_length += column_widths[column];
            if (column + 1 < max_columns) {
                total_length += 3;
            }
        }
        total_length += 1;

        if (index == 0) {
            for (column = 0; column < max_columns; column++) {
                total_length += column_widths[column];
                if (column + 1 < max_columns) {
                    total_length += 3;
                }
            }
            total_length += 1;
        }
    }

    table = malloc(total_length);
    if (table == NULL) {
        goto cleanup;
    }

    cursor = table;
    for (index = 0; index < row_count; index++) {
        for (column = 0; column < max_columns; column++) {
            const char *cell = column < row_counts[index] ? rows[index][column] : "";

            cursor = append_padded_cell(cursor, cell, column_widths[column]);
            if (column + 1 < max_columns) {
                memcpy(cursor, " | ", 3);
                cursor += 3;
            }
        }
        *cursor++ = '\n';

        if (index == 0) {
            for (column = 0; column < max_columns; column++) {
                size_t dash_count = column_widths[column];

                while (dash_count-- > 0) {
                    *cursor++ = '-';
                }
                if (column + 1 < max_columns) {
                    memcpy(cursor, "-+-", 3);
                    cursor += 3;
                }
            }
            *cursor++ = '\n';
        }
    }
    *cursor = '\0';

cleanup:
    if (rows != NULL) {
        for (index = 0; index < row_count; index++) {
            free(rows[index]);
        }
    }
    free(rows);
    free(row_counts);
    free(column_widths);
    free(buffer);
    return table;
}

/* INSERT 실행 흐름
 * 1. values를 토큰으로 분리
 * 2. 기존 CSV가 없으면 헤더 + 첫 행 생성
 * 3. 기존 CSV가 있으면 헤더 열 수를 확인하고 필요하면 확장
 * 4. 새 행을 파일 끝에 추가
 */
static ExecutionResult execute_insert(Statement statement) {
    ExecutionResult result = {EXECUTE_ERROR, EXECUTE_WRITE_FAILED, NULL};
    char *path = build_csv_path(statement.table_name);
    char *existing = NULL;
    char **tokens = NULL;
    char *header = NULL;
    char *row = NULL;
    char *file_content = NULL;
    char *first_newline;
    size_t token_count;
    size_t column_count;

    if (path == NULL) {
        return (ExecutionResult){EXECUTE_ERROR, EXECUTE_WRITE_FAILED, copy_text("Write failed.\n")};
    }

    token_count = split_values(statement.values, &tokens);
    if (tokens == NULL || token_count == 0) {
        free(path);
        return (ExecutionResult){EXECUTE_ERROR, EXECUTE_WRITE_FAILED, copy_text("Write failed.\n")};
    }

    existing = read_file(path);

    /* 파일이 없거나 비어 있으면 이번 INSERT가 테이블의 첫 데이터가 된다. */
    if (existing == NULL || existing[0] == '\0') {
        column_count = token_count;
        header = build_header(column_count);
        row = build_row(tokens, token_count, column_count);
        if (header == NULL || row == NULL) {
            goto cleanup;
        }

        file_content = build_file_content(header, "", row);
        if (file_content == NULL || !write_text_atomic(path, file_content)) {
            goto cleanup;
        }
    } else {
        /* 기존 테이블이 있으면 현재 헤더 열 수를 읽고, 더 넓은 행이 오면 헤더를 늘린다. */
        column_count = count_header_columns(existing);
        if (token_count > column_count) {
            column_count = token_count;
        }

        header = build_header(column_count);
        row = build_row(tokens, token_count, column_count);
        if (header == NULL || row == NULL) {
            goto cleanup;
        }

        first_newline = strchr(existing, '\n');
        file_content = build_file_content(header,
                                          (first_newline != NULL && first_newline[1] != '\0') ? first_newline + 1 : "",
                                          row);
        if (file_content == NULL || !write_text_atomic(path, file_content)) {
            goto cleanup;
        }
    }

    result.status = EXECUTE_OK;
    result.error_code = EXECUTE_NONE;
    result.output = copy_text("Executed.\n");

cleanup:
    if (result.output == NULL) {
        result.status = EXECUTE_ERROR;
        result.error_code = EXECUTE_WRITE_FAILED;
        result.output = copy_text("Write failed.\n");
    }
    free(path);
    free(existing);
    free(tokens);
    free(header);
    free(row);
    free(file_content);
    return result;
}

/* SELECT 실행 흐름
 * 1. CSV 파일 전체를 읽는다.
 * 2. 비어 있으면 에러를 반환한다.
 * 3. 읽은 CSV를 사람이 보기 쉬운 문자열로 바꿔 돌려준다.
 */
static ExecutionResult execute_select(Statement statement) {
    ExecutionResult result = {EXECUTE_ERROR, EXECUTE_OPEN_FAILED, NULL};
    char *path = build_csv_path(statement.table_name);
    char *content;
    char *table_output;

    if (path == NULL) {
        return (ExecutionResult){EXECUTE_ERROR, EXECUTE_OPEN_FAILED, copy_text("Open failed.\n")};
    }

    content = read_file(path);
    free(path);

    if (content == NULL) {
        result.output = copy_text("Open failed.\n");
        return result;
    }

    if (content[0] == '\0') {
        free(content);
        result.error_code = EXECUTE_EMPTY_RESULT;
        result.output = copy_text("Empty result.\n");
        return result;
    }

    table_output = format_csv_as_table(content);
    free(content);
    if (table_output == NULL) {
        result.output = copy_text("Open failed.\n");
        return result;
    }

    result.status = EXECUTE_OK;
    result.error_code = EXECUTE_NONE;
    result.output = table_output;
    return result;
}

/* execute는 문장 종류에 따라 INSERT 또는 SELECT 실행기로 분기하는 얇은 라우터다. */
ExecutionResult execute(Statement statement) {
    if (statement.type == STATEMENT_INSERT) {
        return execute_insert(statement);
    }

    if (statement.type == STATEMENT_SELECT) {
        return execute_select(statement);
    }

    return (ExecutionResult){EXECUTE_ERROR, EXECUTE_OPEN_FAILED, copy_text("Execution failed.\n")};
}
