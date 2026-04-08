#include "sql_engine.h"

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

/*
학습용 의사코드

main(argc, argv):
  1. getopt로 옵션을 읽는다
     - -h 이면 사용법을 stdout에 출력하고 종료한다
     - 알 수 없는 옵션이면 사용법을 stderr에 출력하고 실패 종료한다

  2. getopt가 끝난 뒤 남은 위치 인자가 있는지 본다
     - 남은 인자가 있으면
       "이 단계는 파일명 인자를 받지 않고 stdin 리다이렉션만 받는다"를 출력하고 종료한다

  3. stdin이 키보드(터미널)인지 확인한다
     - 터미널이면 사용자가 그냥 ./program 만 실행한 상태이므로
       "예: ./program < sample.sql" 을 안내하고 종료한다

  4. 고정 크기 문자 버퍼를 준비한다
     - 이번 단계는 최소 구현이 목적이므로 동적 메모리 대신 고정 버퍼를 쓴다

  5. stdin에서 EOF까지 읽고 마지막에 '\0' 을 붙인다
     - 빈 입력이면 실패
     - 버퍼를 넘치면 실패

  6. parse(sql, &stmt)를 호출한다
     - 지금은 SQL을 진짜 해석하지 않고, 다음 단계로 넘길 최소 정보만 만든다

  7. execute(&stmt, &result)를 호출한다
     - 지금은 파일 저장이나 SELECT 실행 없이 고정 성공값만 만든다

  8. 성공이면 결과를 출력하고 0으로 종료한다
     - 실패면 stderr에 메시지를 출력하고 1로 종료한다

parse(sql, out):
  1. sql 또는 out 이 NULL이면 실패한다
  2. 공백만 있는 문자열이면 실패한다
  3. 아니면 out->raw_sql = sql 로 연결하고 성공한다

execute(stmt, out):
  1. stmt, out, stmt->raw_sql 중 하나라도 없으면 실패한다
  2. out->message = "OK" 를 넣고 성공한다
*/

/*
역할:
  SQL 문자열이 공백뿐인지 확인한다.

왜 필요한가:
  사용자가 엔터나 공백만 입력했을 때도 문자열 자체는 존재하므로,
  "아무 내용 없는 SQL"을 정상 입력으로 착각하지 않기 위해 필요하다.

동작 원리:
  문자열의 각 문자를 처음부터 끝까지 보면서,
  공백 문자가 아닌 것이 하나라도 나오면 0을 반환한다.
  끝까지 갔는데 모두 공백이면 1을 반환한다.
*/
static int is_blank_sql(const char *sql) {
    while (*sql != '\0') {
        if (!isspace((unsigned char) *sql)) {
            return 0;
        }
        sql++;
    }

    return 1;
}

/*
역할:
  프로그램 사용법 한 줄을 출력한다.

왜 필요한가:
  -h 옵션을 받았을 때,
  또는 잘못 실행했을 때 사용자가 어떻게 실행해야 하는지 바로 안내해야 한다.

출력 대상:
  성공성 안내일 때는 stdout,
  잘못된 실행 안내일 때는 stderr로 보낼 수 있게 FILE *stream을 인자로 받는다.
*/
void print_usage(FILE *stream, const char *program_name) {
    fprintf(stream, "Usage: %s [-h] < input.sql\n", program_name);
}

#ifndef UNIT_TEST
/*
역할:
  CLI 실행 방식이 이번 단계 규칙을 만족하는지 확인한다.

왜 필요한가:
  main 안에 옵션 처리, 위치 인자 검사, stdin 검사까지 모두 늘어놓으면
  "실행 흐름"보다 "예외 처리"가 먼저 보여서 전체 구조를 읽기 어려워진다.
  그래서 이 함수가 초반 검사를 한곳에 모은다.

검사 내용:
  1. getopt로 -h 옵션만 받는다.
  2. 위치 인자가 남아 있으면 실패한다.
  3. stdin이 터미널이면 리다이렉션 입력이 없다고 보고 실패한다.

반환값:
  0  다음 단계로 계속 진행
  1  help 출력 후 정상 종료
 -1  잘못된 실행 방식이라 실패 종료
*/
static int validate_cli(int argc, char **argv) {
    int option;

    opterr = 0;

    while ((option = getopt(argc, argv, "h")) != -1) {
        switch (option) {
            case 'h':
                print_usage(stdout, argv[0]);
                return 1;
            default:
                print_usage(stderr, argv[0]);
                return -1;
        }
    }

    if (optind != argc) {
        fprintf(stderr, "Positional arguments are not supported. Use stdin redirection.\n");
        return -1;
    }

    if (isatty(fileno(stdin))) {
        fprintf(stderr, "Provide SQL with stdin redirection. Example: ./program < sample.sql\n");
        return -1;
    }

    return 0;
}
#endif

/*
역할:
  stdin으로 들어온 SQL 텍스트를 고정 크기 버퍼에 모두 읽는다.

왜 필요한가:
  parse는 C 문자열을 입력으로 받기 때문에,
  먼저 사용자 입력 전체를 메모리 안의 문자열로 만들어야 한다.

동작 원리:
  1. fgetc로 문자를 하나씩 읽는다.
  2. 읽은 문자를 buffer에 차례대로 저장한다.
  3. 버퍼 끝 직전까지 차면 더 이상 안전하게 저장할 수 없으므로 길이 초과로 실패한다.
  4. EOF까지 읽고 마지막에 '\0'을 붙여 C 문자열로 만든다.

반환값:
  READ_SQL_OK       정상 입력
  READ_SQL_EMPTY    한 글자도 읽지 못한 경우
  READ_SQL_TOO_LONG 버퍼 크기를 넘긴 경우
  READ_SQL_ERROR    FILE 또는 버퍼가 잘못됐거나 읽기 오류가 난 경우
*/
int read_sql_input(FILE *input, char *buffer, size_t buffer_size) {
    size_t index = 0;
    int ch;

    if (input == NULL || buffer == NULL || buffer_size == 0) {
        return READ_SQL_ERROR;
    }

    while ((ch = fgetc(input)) != EOF) {
        if (index + 1 >= buffer_size) {
            buffer[0] = '\0';
            return READ_SQL_TOO_LONG;
        }

        buffer[index++] = (char) ch;
    }

    if (ferror(input)) {
        buffer[0] = '\0';
        return READ_SQL_ERROR;
    }

    if (index == 0) {
        buffer[0] = '\0';
        return READ_SQL_EMPTY;
    }

    buffer[index] = '\0';
    return READ_SQL_OK;
}

#ifndef UNIT_TEST
/*
역할:
  입력 읽기 결과 코드를 사용자에게 보이는 에러 메시지로 바꿔 출력한다.

왜 필요한가:
  read_sql_input은 재사용 가능한 helper라서 숫자 상태만 반환하고,
  사용자 메시지 결정은 별도 함수로 분리해 main의 분기 수를 줄인다.
*/
static int read_sql_or_print_error(FILE *input, char *buffer, size_t buffer_size) {
    int read_status = read_sql_input(input, buffer, buffer_size);

    if (read_status == READ_SQL_OK) {
        return 0;
    }

    if (read_status == READ_SQL_EMPTY) {
        fprintf(stderr, "Input is empty.\n");
        return 1;
    }

    if (read_status == READ_SQL_TOO_LONG) {
        fprintf(stderr, "Input is too long. Maximum is %d bytes.\n", SQL_INPUT_BUFFER_SIZE - 1);
        return 1;
    }

    fprintf(stderr, "Failed to read input.\n");
    return 1;
}
#endif

/*
역할:
  입력 문자열을 "해석된 문장"의 최소 형태로 바꾼다.

왜 필요한가:
  main이 곧바로 문자열을 실행하지 않고,
  "입력 -> 해석 -> 실행"의 단계를 분리해서 보여주기 위해 필요하다.

이번 단계에서 일부러 단순화한 점:
  아직 INSERT, SELECT를 진짜 구분하지 않는다.
  지금은 "유효한 비어 있지 않은 문자열인가?"만 확인하고,
  통과하면 raw_sql 포인터만 저장한다.

즉:
  지금 parse의 목표는 정교한 SQL 분석이 아니라,
  다음 단계에서 진짜 파서를 넣을 자리를 만드는 것이다.
*/
int parse(const char *sql, ParsedStatement *out) {
    if (sql == NULL || out == NULL) {
        return 1;
    }

    if (is_blank_sql(sql)) {
        return 1;
    }

    out->raw_sql = sql;
    return 0;
}

/*
역할:
  parse가 만든 문장을 받아 실행 결과를 만든다.

왜 필요한가:
  main이 "해석" 단계와 "실행/저장" 단계를 분리해서 연결하는 구조를 보여주기 위해 필요하다.

이번 단계에서 일부러 단순화한 점:
  아직 파일 저장도 하지 않고, SELECT 결과 조회도 하지 않는다.
  stmt가 정상인지 최소한만 확인한 뒤 결과 메시지를 "OK"로 고정한다.

즉:
  지금 execute의 목표는 진짜 데이터베이스 동작이 아니라,
  main이 실행 결과를 받아 출력하는 흐름을 완성하는 것이다.
*/
int execute(const ParsedStatement *stmt, ExecuteResult *out) {
    if (stmt == NULL || out == NULL || stmt->raw_sql == NULL) {
        return 1;
    }

    out->message = "OK";
    return 0;
}

#ifndef UNIT_TEST
/*
역할:
  CLI 프로그램의 시작점이다.

책임:
  1. 사용자가 프로그램을 올바르게 실행했는지 확인한다.
  2. stdin에서 SQL 텍스트를 읽는다.
  3. parse와 execute를 순서대로 호출한다.
  4. 최종 결과를 사용자에게 출력한다.

중요한 의도:
  이번 단계의 main은 기능을 많이 넣기보다,
  "전체 사이클이 어떻게 연결되는지"를 가장 짧고 읽기 쉽게 보여주는 데 목적이 있다.
  그래서 실제 SQL 처리 로직은 최소로 두고,
  흐름 제어를 main에서 눈에 보이게 유지한다.
*/
int main(int argc, char **argv) {
    ParsedStatement stmt;
    ExecuteResult result;
    char buffer[SQL_INPUT_BUFFER_SIZE];
    int cli_status;

    /* 1. 실행 방식이 이번 단계 규칙에 맞는지 확인한다. */
    cli_status = validate_cli(argc, argv);
    if (cli_status != 0) {
        return cli_status > 0 ? 0 : 1;
    }

    /* 2. stdin에서 SQL 문자열을 읽어온다. */
    if (read_sql_or_print_error(stdin, buffer, sizeof(buffer)) != 0) {
        return 1;
    }

    /* 3. parse는 지금 단계에서 raw SQL을 다음 단계로 넘길 최소 단위만 만든다. */
    if (parse(buffer, &stmt) != 0) {
        fprintf(stderr, "parse failed\n");
        return 1;
    }

    /* 4. execute는 지금 단계에서 실제 DB 작업 없이 고정 결과만 돌려준다. */
    if (execute(&stmt, &result) != 0) {
        fprintf(stderr, "execute failed\n");
        return 1;
    }

    /* 5. 성공이면 최종 결과를 출력한다. */
    fprintf(stdout, "%s\n", result.message);
    return 0;
}
#endif
