#define TESTING
#include "main.c"

#include <assert.h>
#include <string.h>

int main(void) {
    /* 정상 insert 문 */
    char a[] = " insert into usertbl values ('dukbae', 2, 'db2', '010-1234-5678', 'cold') \n";
    /* 정상 select 문 */
    char b[] = "select   *   from   table1\n";
    /* 종료 명령 */
    char c[] = ".exit\n";
    /* values 키워드가 빠진 잘못된 insert 문 */
    char d[] = "insert into usertbl ('dukbae', 2)\n";
    /* 토큰이 더 붙은 잘못된 select 문 */
    char e[] = "select * from table1 now\n";
    Cmd cmd;

    cmd = parse(a);
    assert(cmd.kind == INSERT);
    assert(strcmp(cmd.table, "usertbl") == 0);
    assert(strcmp(cmd.vals, "('dukbae', 2, 'db2', '010-1234-5678', 'cold')") == 0);

    cmd = parse(b);
    assert(cmd.kind == SELECT);
    assert(strcmp(cmd.table, "table1") == 0);

    cmd = parse(c);
    assert(cmd.kind == EXIT);

    cmd = parse(d);
    assert(cmd.kind == BAD);

    cmd = parse(e);
    assert(cmd.kind == BAD);

    return 0;
}
