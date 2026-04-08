#include <stdio.h>
#include "utils.h"

/**
 * @brief 사용자 인자가 정확히 하나인지 확인하고 출력할 문자열을 반환한다.
 * @param argc 전달된 전체 인자 개수
 * @param argv 전달된 전체 인자 배열
 * @return 성공하면 첫 번째 사용자 인자 문자열, 실패하면 NULL
 */
static const char *process_args(int argc, char *argv[]) {
    if (argc != 2) {
        return NULL;
    }

    return argv[1];
}

int main(int argc, char *argv[]) {
    const char *result = process_args(argc, argv);

    if (result == NULL) {
        return ERR;
    }

    printf("%s\n", result);
    return OK;
}
