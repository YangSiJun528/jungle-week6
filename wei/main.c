#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char *line = NULL;
    size_t line_capacity = 0;

    /* 프로그램 전체는 "프롬프트 출력 -> 한 줄 입력 -> 명령 판별" REPL로 반복된다. */
    while (true) {
        /* 사용자가 다음 명령을 입력할 수 있도록 고정 프롬프트를 보여준다. */
        printf("db > ");
        fflush(stdout);

        /* getline()이 한 줄 전체를 읽어 line 버퍼에 저장한다. EOF면 루프를 끝낸다. */
        if (getline(&line, &line_capacity, stdin) == -1) {
            free(line);
            return 0;
        }

        /* 비교가 쉬워지도록 getline()이 남긴 개행 문자를 문자열 끝으로 바꾼다. */
        line[strcspn(line, "\n")] = '\0';

        /* 메타 명령 .exit 이면 버퍼를 정리하고 프로그램을 종료한다. */
        if (strcmp(line, ".exit") == 0) {
            free(line);
            return 0;
        }

        /* 현재는 .exit 외 명령을 지원하지 않으므로 모두 미인식 명령으로 처리한다. */
        printf("Unrecognized command\n");
    }
}
