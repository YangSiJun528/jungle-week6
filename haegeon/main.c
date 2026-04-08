#include <stdio.h>

#include "cli.h"

static const char *DEFAULT_DB_DIR = "haegeon/data";

/**
 * @brief 표준 입출력 기반 CLI를 실행한다.
 * @param 없음
 * @return 프로그램 종료 코드를 반환한다.
 */
int main(void) {
    return run_cli(stdin, stdout, stderr, DEFAULT_DB_DIR);
}
