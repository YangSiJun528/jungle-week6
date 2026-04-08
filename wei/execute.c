#ifndef CSV_DIR
#define CSV_DIR "."
#endif

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

    while (*token == ' ' || *token == '\t') {
        token++;
    }

    end = token + strlen(token);
    while (end > token && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    *end = '\0';

    if (strlen(token) >= 2 && token[0] == '\'' && end[-1] == '\'') {
        token++;
        end[-1] = '\0';
    }

    return token;
}

/* "a, b, 'c'" 같은 values 문자열을 쉼표 기준 토큰 배열로 나눈다. */
static size_t split_values(char *values, char ***tokens_out) {
    size_t count = 1;
    size_t index = 0;
    char **tokens;
    char *cursor = values;

    while (*cursor != '\0') {
        if (*cursor == ',') {
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
        if (*cursor == ',') {
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
    size_t used = 0;

    for (index = 0; index < column_count; index++) {
        length += (index < token_count ? strlen(tokens[index]) : 0) + 1;
    }

    row = malloc(length);
    if (row == NULL) {
        return NULL;
    }

    for (index = 0; index < column_count; index++) {
        used += (size_t) snprintf(row + used, length - used, "%s%s",
                                  index == 0 ? "" : ",",
                                  index < token_count ? tokens[index] : "");
    }

    snprintf(row + used, length - used, "\n");
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

/* CSV 한 줄도 결국 쉼표 기반이므로 SELECT 출력용 토큰 분해에 재사용한다. */
static size_t split_csv_line(char *line, char ***tokens_out) {
    size_t count = 1;
    size_t index = 0;
    char **tokens;
    char *cursor = line;

    while (*cursor != '\0') {
        if (*cursor == ',') {
            count++;
        }
        cursor++;
    }

    tokens = malloc(sizeof(char *) * count);
    if (tokens == NULL) {
        *tokens_out = NULL;
        return 0;
    }

    cursor = line;
    tokens[index++] = cursor;
    while (*cursor != '\0') {
        if (*cursor == ',') {
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
    char *first_newline;
    size_t token_count;
    size_t column_count;
    FILE *file;

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

        file = fopen(path, "w");
        if (file == NULL || fputs(header, file) == EOF || fputs(row, file) == EOF) {
            if (file != NULL) {
                fclose(file);
            }
            goto cleanup;
        }
        fclose(file);
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
        file = fopen(path, "w");
        if (file == NULL || fputs(header, file) == EOF) {
            if (file != NULL) {
                fclose(file);
            }
            goto cleanup;
        }
        if (first_newline != NULL && first_newline[1] != '\0' && fputs(first_newline + 1, file) == EOF) {
            fclose(file);
            goto cleanup;
        }
        if (fputs(row, file) == EOF) {
            fclose(file);
            goto cleanup;
        }
        fclose(file);
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
