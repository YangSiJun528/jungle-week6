#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "main.h"
#include "repl_internal.h"
#include "test_util.h"

/**
 * @brief 스트림 전체를 읽어 문자열 버퍼에 담는다.
 * @param stream 읽을 스트림
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 */
static void read_stream(FILE *stream, char *buffer, size_t buffer_size) {
    size_t length = 0;
    int ch;

    rewind(stream);

    while ((ch = fgetc(stream)) != EOF && length + 1 < buffer_size) {
        buffer[length++] = (char) ch;
    }

    buffer[length] = '\0';
}

/**
 * @brief 파일 전체를 읽어 문자열 버퍼에 담는다.
 * @param path 읽을 파일 경로
 * @param buffer 결과를 저장할 버퍼
 * @param buffer_size 버퍼 크기
 */
static void read_file_text(const char *path, char *buffer, size_t buffer_size) {
    FILE *stream = fopen(path, "r");

    assertTrue(stream != NULL);
    read_stream(stream, buffer, buffer_size);
    fclose(stream);
}

/**
 * @brief 경로의 텍스트 파일을 덮어쓴다.
 * @param path 기록할 파일 경로
 * @param text 기록할 문자열
 */
static void write_text_file(const char *path, const char *text) {
    FILE *stream = fopen(path, "w");

    assertTrue(stream != NULL);
    assertTrue(fputs(text, stream) != EOF);
    fclose(stream);
}

/**
 * @brief mkstemp 템플릿 경로를 실제 디렉터리로 바꾼다.
 * @param root_template 수정 가능한 템플릿 버퍼
 */
static void create_temp_directory(char *root_template) {
    int file_descriptor = mkstemp(root_template);

    assertTrue(file_descriptor != -1);
    close(file_descriptor);
    assertEq(unlink(root_template), 0);
    assertEq(mkdir(root_template, 0700), 0);
}

/**
 * @brief 테스트용 DB 디렉터리 집합을 만든다.
 * @param root_template mkdtemp에 넘길 템플릿 버퍼
 * @param db_directory 생성된 db 경로를 받을 버퍼
 * @param db_directory_size db 경로 버퍼 크기
 */
static void create_test_db(char *root_template, char *db_directory, size_t db_directory_size) {
    char metadata_path[PATH_MAX];
    char users_path[PATH_MAX];
    char posts_path[PATH_MAX];

    create_temp_directory(root_template);
    assertTrue(snprintf(db_directory, db_directory_size, "%s/db", root_template) > 0);
    assertEq(mkdir(db_directory, 0700), 0);

    assertTrue(snprintf(metadata_path, sizeof(metadata_path), "%s/metadata.csv", db_directory) > 0);
    assertTrue(snprintf(users_path, sizeof(users_path), "%s/users.csv", db_directory) > 0);
    assertTrue(snprintf(posts_path, sizeof(posts_path), "%s/posts.csv", db_directory) > 0);

    write_text_file(
        metadata_path,
        "users,id,NUMBER\n"
        "users,name,TEXT\n"
        "users,role,TEXT\n"
        "posts,id,NUMBER\n"
        "posts,title,TEXT\n"
        "posts,content,TEXT\n"
    );
    write_text_file(users_path, "1,kim min,admin\n2,lee,member\n");
    write_text_file(posts_path, "10,hello,first post\n11,notice,second post\n");
}

/**
 * @brief 테스트용 메타데이터 파일 경로를 만든다.
 * @param path 결과 경로를 받을 버퍼
 * @param path_size 버퍼 크기
 * @param db_directory 기준 db 경로
 */
static void build_metadata_path(char *path, size_t path_size, const char *db_directory) {
    assertTrue(snprintf(path, path_size, "%s/metadata.csv", db_directory) > 0);
}

/**
 * @brief 테스트용 임시 디렉터리를 재귀적으로 지운다.
 * @param root_directory 제거할 루트 디렉터리
 */
static void remove_test_db(const char *root_directory) {
    char command[PATH_MAX * 2];

    assertTrue(snprintf(command, sizeof(command), "rm -rf \"%s\"", root_directory) > 0);
    assertEq(system(command), 0);
}

/**
 * @brief 메모리 기반 입력으로 run_repl 결과를 검증하기 위한 헬퍼다.
 * @param input_text 입력 문자열
 * @param db_directory 실행에 사용할 db 경로
 * @param output_buffer 표준 출력 결과 버퍼
 * @param output_buffer_size 표준 출력 버퍼 크기
 * @param error_buffer 표준 에러 결과 버퍼
 * @param error_buffer_size 표준 에러 버퍼 크기
 * @return run_repl 종료 코드
 */
static int run_repl_with_text(
    const char *input_text,
    const char *db_directory,
    char *output_buffer,
    size_t output_buffer_size,
    char *error_buffer,
    size_t error_buffer_size
) {
    FILE *input_stream = tmpfile();
    FILE *output_stream = tmpfile();
    FILE *error_stream = tmpfile();
    int result;

    assertTrue(input_stream != NULL);
    assertTrue(output_stream != NULL);
    assertTrue(error_stream != NULL);

    fputs(input_text, input_stream);
    rewind(input_stream);

    result = run_repl_with_database_dir(input_stream, output_stream, error_stream, db_directory);

    read_stream(output_stream, output_buffer, output_buffer_size);
    read_stream(error_stream, error_buffer, error_buffer_size);

    fclose(input_stream);
    fclose(output_stream);
    fclose(error_stream);
    return result;
}

/* 개행이 있으면 제거하면 길이가 줄어야 한다. */
static void trims_trailing_newline(void) {
    char line[] = "hello\n";

    /* given */

    /* when */
    size_t length = trim_newline(line, 6);

    /* then */
    assertEq((long long) length, 5);
    assertStrEq(line, "hello");
}

/* .exit는 종료 동작이어야 한다. */
static void exits_on_exit_command(void) {
    char line[] = ".exit";

    /* given */

    /* when */
    LineAction action = evaluate_line(line);

    /* then */
    assertEq(action.type, LINE_ACTION_EXIT);
    assertNull(action.message);
}

/* select 문은 테이블명을 파싱해야 한다. */
static void parses_select_query(void) {
    ParseResult result = parse("select * from users;");

    /* given */

    /* when */

    /* then */
    assertNull(result.error_message);
    assertEq(result.type, QUERY_TYPE_SELECT);
    assertStrEq(result.table_name, "users");
    assertEq((long long) result.value_count, 0);

    free_parse_result(&result);
}

/* insert 문은 숫자 문자열 식별자를 모두 파싱해야 한다. */
static void parses_insert_query_values(void) {
    ParseResult result = parse("insert into users values (1, 'kim min', 'admin_1');");

    /* given */

    /* when */

    /* then */
    assertNull(result.error_message);
    assertEq(result.type, QUERY_TYPE_INSERT);
    assertStrEq(result.table_name, "users");
    assertEq((long long) result.value_count, 3);
    assertEq(result.values[0].type, VALUE_TYPE_NUMBER);
    assertStrEq(result.values[0].text, "1");
    assertEq(result.values[1].type, VALUE_TYPE_STRING);
    assertStrEq(result.values[1].text, "kim min");
    assertEq(result.values[2].type, VALUE_TYPE_STRING);
    assertStrEq(result.values[2].text, "admin_1");

    free_parse_result(&result);
}

/* insert 값의 텍스트는 작은따옴표 문자열이어야 한다. */
static void rejects_bare_identifier_value_in_insert(void) {
    ParseResult result = parse("insert into users values (1, 'kim min', admin_1);");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "expected value");
}

/* 쿼리 끝에는 세미콜론이 반드시 있어야 한다. */
static void rejects_query_without_semicolon(void) {
    ParseResult result = parse("select * from users");

    /* given */

    /* when */

    /* then */
    assertEq(result.type, QUERY_TYPE_INVALID);
    assertStrEq(result.error_message, "expected ;");
}

/* 메타데이터 로더는 파일에 있는 테이블들을 읽어야 한다. */
static void loads_metadata_from_csv_file(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    MetadataLoadResult load_result;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));

    /* when */
    load_result = load_metadata_from_directory(db_directory);

    /* then */
    assertNull(load_result.error_message);
    assertEq((long long) load_result.metadata.table_count, 2);
    assertStrEq(load_result.metadata.tables[0].name, "users");
    assertStrEq(load_result.metadata.tables[0].data_file_path, "users.csv");
    assertEq((long long) load_result.metadata.tables[0].column_count, 3);
    assertStrEq(load_result.metadata.tables[0].columns[0].name, "id");
    assertEq(load_result.metadata.tables[0].columns[0].type, COLUMN_TYPE_NUMBER);

    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* 하드코딩된 테이블명이 아니어도 메타데이터 로딩은 성공해야 한다. */
static void loads_metadata_without_hardcoded_table_requirement(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char metadata_path[PATH_MAX];
    MetadataLoadResult load_result;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    build_metadata_path(metadata_path, sizeof(metadata_path), db_directory);
    write_text_file(
        metadata_path,
        "members,uid,NUMBER\n"
        "members,display_name,TEXT\n"
        "articles,id,NUMBER\n"
        "articles,body,TEXT\n"
    );

    /* when */
    load_result = load_metadata_from_directory(db_directory);

    /* then */
    assertNull(load_result.error_message);
    assertEq((long long) load_result.metadata.table_count, 2);
    assertStrEq(load_result.metadata.tables[0].name, "members");
    assertStrEq(load_result.metadata.tables[1].name, "articles");

    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* 메타데이터에 테이블이 하나도 없으면 로딩이 실패해야 한다. */
static void rejects_metadata_when_no_table_exists(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char metadata_path[PATH_MAX];
    MetadataLoadResult load_result;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    build_metadata_path(metadata_path, sizeof(metadata_path), db_directory);
    write_text_file(metadata_path, "");

    /* when */
    load_result = load_metadata_from_directory(db_directory);

    /* then */
    assertStrEq(load_result.error_message, "no table in metadata");
    assertEq((long long) load_result.metadata.table_count, 0);

    remove_test_db(root_template);
}

/* 존재하는 테이블 메타데이터는 이름으로 찾아야 한다. */
static void finds_table_metadata_by_name(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    MetadataLoadResult load_result;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    load_result = load_metadata_from_directory(db_directory);

    /* when */
    const TableMetadata *table = find_table_metadata(&load_result.metadata, "posts");

    /* then */
    assertNull(load_result.error_message);
    assertTrue(table != NULL);
    assertStrEq(table->name, "posts");
    assertStrEq(table->data_file_path, "posts.csv");
    assertEq((long long) table->column_count, 3);
    assertStrEq(table->columns[1].name, "title");

    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* insert 값 개수는 컬럼 개수와 같아야 한다. */
static void rejects_insert_when_value_count_differs(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    MetadataLoadResult load_result;
    ParseResult result;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    load_result = load_metadata_from_directory(db_directory);
    result = parse("insert into users values (1, 'kim min');");

    /* when */
    SemanticCheckResult check = validate_query_against_metadata(&load_result.metadata, &result);

    /* then */
    assertEq(check.ok, 0);
    assertStrEq(check.error_message, "column count does not match value count");

    free_parse_result(&result);
    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* insert 실행은 테이블 CSV 마지막에 한 줄을 추가해야 한다. */
static void executes_insert_query_to_csv(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char users_path[PATH_MAX];
    char file_text[256];
    MetadataLoadResult load_result;
    ParseResult result;
    FILE *output_stream;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    assertTrue(snprintf(users_path, sizeof(users_path), "%s/users.csv", db_directory) > 0);
    load_result = load_metadata_from_directory(db_directory);
    result = parse("insert into users values (3, 'park', 'guest');");
    output_stream = tmpfile();
    assertTrue(output_stream != NULL);

    /* when */
    QueryExecutionResult execution = execute_query(&load_result.metadata, &result, db_directory, output_stream);

    /* then */
    assertEq(execution.ok, 1);
    read_file_text(users_path, file_text, sizeof(file_text));
    assertStrEq(file_text, "1,kim min,admin\n2,lee,member\n3,park,guest\n");

    fclose(output_stream);
    free_parse_result(&result);
    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* select 실행은 모든 컬럼을 표 형태로 출력해야 한다. */
static void executes_select_query_to_ascii_table(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char output_buffer[512];
    MetadataLoadResult load_result;
    ParseResult result;
    FILE *output_stream;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    load_result = load_metadata_from_directory(db_directory);
    result = parse("select * from users;");
    output_stream = tmpfile();
    assertTrue(output_stream != NULL);

    /* when */
    QueryExecutionResult execution = execute_query(&load_result.metadata, &result, db_directory, output_stream);

    /* then */
    assertEq(execution.ok, 1);
    read_stream(output_stream, output_buffer, sizeof(output_buffer));
    assertStrEq(
        output_buffer,
        "id | name    | role  \n"
        "---+---------+-------\n"
        "1  | kim min | admin \n"
        "2  | lee     | member\n"
    );

    fclose(output_stream);
    free_parse_result(&result);
    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* 빈 테이블 조회도 헤더와 구분선은 출력해야 한다. */
static void executes_select_query_for_empty_table(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char posts_path[PATH_MAX];
    char output_buffer[256];
    MetadataLoadResult load_result;
    ParseResult result;
    FILE *output_stream;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    assertTrue(snprintf(posts_path, sizeof(posts_path), "%s/posts.csv", db_directory) > 0);
    write_text_file(posts_path, "");
    load_result = load_metadata_from_directory(db_directory);
    result = parse("select * from posts;");
    output_stream = tmpfile();
    assertTrue(output_stream != NULL);

    /* when */
    QueryExecutionResult execution = execute_query(&load_result.metadata, &result, db_directory, output_stream);

    /* then */
    assertEq(execution.ok, 1);
    read_stream(output_stream, output_buffer, sizeof(output_buffer));
    assertStrEq(
        output_buffer,
        "id | title | content\n"
        "---+-------+--------\n"
    );

    fclose(output_stream);
    free_parse_result(&result);
    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* CSV row 컬럼 수가 메타데이터와 다르면 실행이 실패해야 한다. */
static void rejects_select_when_row_column_count_differs(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char users_path[PATH_MAX];
    MetadataLoadResult load_result;
    ParseResult result;
    FILE *output_stream;

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));
    assertTrue(snprintf(users_path, sizeof(users_path), "%s/users.csv", db_directory) > 0);
    write_text_file(users_path, "1,broken\n");
    load_result = load_metadata_from_directory(db_directory);
    result = parse("select * from users;");
    output_stream = tmpfile();
    assertTrue(output_stream != NULL);

    /* when */
    QueryExecutionResult execution = execute_query(&load_result.metadata, &result, db_directory, output_stream);

    /* then */
    assertEq(execution.ok, 0);
    assertStrEq(execution.error_message, "row column count does not match metadata");

    fclose(output_stream);
    free_parse_result(&result);
    free_metadata(&load_result.metadata);
    remove_test_db(root_template);
}

/* SQL parse 실패에도 프롬프트는 출력하고 parse error를 남겨야 한다. */
static void reports_parse_error_for_sql_like_input(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char output_buffer[64];
    char error_buffer[128];

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));

    /* when */
    int result = run_repl_with_text("select from users;\n.exit\n", db_directory, output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assertEq(result, OK);
    assertStrEq(output_buffer, "> > ");
    assertStrEq(error_buffer, "Parse error: unexpected token\n");

    remove_test_db(root_template);
}

/* SQL이 아닌 일반 입력은 해석 실패 메시지를 출력해야 한다. */
static void reports_uninterpretable_plain_text(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char output_buffer[128];
    char error_buffer[32];

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));

    /* when */
    int result = run_repl_with_text("asdf\n.exit\n", db_directory, output_buffer, sizeof(output_buffer), error_buffer, sizeof(error_buffer));

    /* then */
    assertEq(result, OK);
    assertStrEq(output_buffer, "> 해석할 수 없는 입력입니다.\n> ");
    assertStrEq(error_buffer, "");

    remove_test_db(root_template);
}

/* 따옴표 없는 텍스트 값은 parse error가 나야 한다. */
static void reports_parse_error_for_unquoted_text_value(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char output_buffer[1024];
    char error_buffer[64];

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));

    /* when */
    int result = run_repl_with_text(
        "insert into posts values (12, 'draft', note);\n"
        "select * from posts;\n"
        ".exit\n",
        db_directory,
        output_buffer,
        sizeof(output_buffer),
        error_buffer,
        sizeof(error_buffer)
    );

    /* then */
    assertEq(result, OK);
    assertStrEq(
        output_buffer,
        "> > id | title  | content    \n"
        "---+--------+------------\n"
        "10 | hello  | first post \n"
        "11 | notice | second post\n"
        "> "
    );
    assertStrEq(error_buffer, "Parse error: expected value\n");

    remove_test_db(root_template);
}

/* insert 후 select 하면 변경된 데이터가 조회되어야 한다. */
static void runs_insert_then_select_in_repl(void) {
    char root_template[] = "/tmp/jungle-week6-sijun-unit-XXXXXX";
    char db_directory[PATH_MAX];
    char output_buffer[1024];
    char error_buffer[64];

    /* given */
    create_test_db(root_template, db_directory, sizeof(db_directory));

    /* when */
    int result = run_repl_with_text(
        "insert into posts values (12, 'draft', 'note');\n"
        "select * from posts;\n"
        ".exit\n",
        db_directory,
        output_buffer,
        sizeof(output_buffer),
        error_buffer,
        sizeof(error_buffer)
    );

    /* then */
    assertEq(result, OK);
    assertStrEq(
        output_buffer,
        "> 1 row inserted\n"
        "> id | title  | content    \n"
        "---+--------+------------\n"
        "10 | hello  | first post \n"
        "11 | notice | second post\n"
        "12 | draft  | note       \n"
        "> "
    );
    assertStrEq(error_buffer, "");

    remove_test_db(root_template);
}

int main(void) {
    trims_trailing_newline();
    exits_on_exit_command();
    parses_select_query();
    parses_insert_query_values();
    rejects_bare_identifier_value_in_insert();
    rejects_query_without_semicolon();
    loads_metadata_from_csv_file();
    loads_metadata_without_hardcoded_table_requirement();
    rejects_metadata_when_no_table_exists();
    finds_table_metadata_by_name();
    rejects_insert_when_value_count_differs();
    executes_insert_query_to_csv();
    executes_select_query_to_ascii_table();
    executes_select_query_for_empty_table();
    rejects_select_when_row_column_count_differs();
    reports_parse_error_for_sql_like_input();
    reports_uninterpretable_plain_text();
    reports_parse_error_for_unquoted_text_value();
    runs_insert_then_select_in_repl();
    return 0;
}
