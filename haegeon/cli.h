#ifndef HAEGEON_CLI_H
#define HAEGEON_CLI_H

#include <stdio.h>

/**
 * @brief 입력 스트림에서 SQL을 읽어 parse와 execute를 순서대로 수행한다.
 * @param in SQL 입력을 읽을 스트림이다.
 * @param out 정상 출력 스트림이다.
 * @param err 오류 출력 스트림이다.
 * @param db_dir 테이블 파일이 들어 있는 디렉토리 경로다.
 * @return 오류가 없으면 0, 하나라도 실패가 있으면 1을 반환한다.
 */
int run_cli(FILE *in, FILE *out, FILE *err, const char *db_dir);

#endif
