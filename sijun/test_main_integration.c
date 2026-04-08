#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "main.h"
#include "test_util.h"

/**
 * @brief 외부 프로그램을 실행하고 출력을 수집한다.
 * @param command 실행할 셸 명령어
 * @param output 결과를 저장할 버퍼
 * @param output_size 결과 버퍼 크기
 * @return 종료 코드
 */
static int run_command(const char *command, char *output, size_t output_size) {
    FILE *pipe = popen(command, "r");
    size_t length = 0;
    int ch;
    int status;

    assertTrue(pipe != NULL);

    while ((ch = fgetc(pipe)) != EOF && length + 1 < output_size) {
        output[length++] = (char) ch;
    }
    output[length] = '\0';

    status = pclose(pipe);
    assertTrue(status != -1);
    assertTrue(WIFEXITED(status));
    return WEXITSTATUS(status);
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
 * @brief 통합 테스트용 작업 디렉터리를 만든다.
 * @param root_template mkdtemp에 넘길 템플릿 버퍼
 */
static void create_test_workspace(char *root_template) {
    char sijun_directory[PATH_MAX];
    char db_directory[PATH_MAX];
    char metadata_path[PATH_MAX];
    char users_path[PATH_MAX];
    char posts_path[PATH_MAX];

    create_temp_directory(root_template);
    assertTrue(snprintf(sijun_directory, sizeof(sijun_directory), "%s/sijun", root_template) > 0);
    assertTrue(snprintf(db_directory, sizeof(db_directory), "%s/sijun/db", root_template) > 0);
    assertEq(mkdir(sijun_directory, 0700), 0);
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
    write_text_file(posts_path, "10,hello,first post\n");
}

/**
 * @brief 테스트용 임시 디렉터리를 재귀적으로 지운다.
 * @param root_directory 제거할 루트 디렉터리
 */
static void remove_test_workspace(const char *root_directory) {
    char command[PATH_MAX * 2];

    assertTrue(snprintf(command, sizeof(command), "rm -rf \"%s\"", root_directory) > 0);
    assertEq(system(command), 0);
}

/* 일반 텍스트는 해석 실패 메시지를 출력해야 한다. */
static void reports_uninterpretable_plain_text(const char *program_path) {
    char output[128];
    char root_template[] = "/tmp/jungle-week6-sijun-int-XXXXXX";
    char command[PATH_MAX * 3];

    /* given */
    create_test_workspace(root_template);
    snprintf(command, sizeof(command), "cd \"%s\" && printf 'hello\\n.exit\\n' | \"%s\"", root_template, program_path);

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assertEq(exit_code, OK);
    assertStrEq(output, "> 해석할 수 없는 입력입니다.\n> ");

    remove_test_workspace(root_template);
}

/* insert 후 select 결과가 CSV 파일 기반으로 반영되어야 한다. */
static void executes_insert_then_select(const char *program_path) {
    char output[512];
    char root_template[] = "/tmp/jungle-week6-sijun-int-XXXXXX";
    char command[PATH_MAX * 3];

    /* given */
    create_test_workspace(root_template);
    snprintf(
        command,
        sizeof(command),
        "cd \"%s\" && printf \"insert into posts values (11, 'notice', 'draft');\\nselect * from posts;\\n.exit\\n\" | \"%s\"",
        root_template,
        program_path
    );

    /* when */
    int exit_code = run_command(command, output, sizeof(output));

    /* then */
    assertEq(exit_code, OK);
    assertStrEq(
        output,
        "> 1 row inserted\n"
        "> id | title  | content   \n"
        "---+--------+-----------\n"
        "10 | hello  | first post\n"
        "11 | notice | draft     \n"
        "> "
    );

    remove_test_workspace(root_template);
}

int main(int argc, char *argv[]) {
    assertEq(argc, 2);

    reports_uninterpretable_plain_text(argv[1]);
    executes_insert_then_select(argv[1]);
    return 0;
}
