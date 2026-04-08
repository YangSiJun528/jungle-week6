#define TESTING
#include "main.c"

#include <assert.h>
#include <string.h>

int main(void) {
    char a[] = "  insert   hi   into   table2  \n";
    char b[] = "select   *   from   table1\n";
    char c[] = ".exit\n";
    char d[] = "insert hi table2\n";
    char e[] = "select * from table1 now\n";
    Cmd cmd;

    cmd = parse(a);
    assert(cmd.kind == INSERT);
    assert(strcmp(cmd.value, "hi") == 0);
    assert(strcmp(cmd.table, "table2") == 0);

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
