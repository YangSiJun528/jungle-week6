#define TESTING
#include "main.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char a[] = "insert into usertbl_test values ('dukbae', 2, 'db2', '010-1234-5678', 'cold')\n";
    char b[] = "insert into usertbl_test values ('minsu', 3, 'db3', '010-0000-0000', 'good')\n";
    char c[] = "select * from usertbl_test\n";
    char d[] = ".exit\n";
    char e[] = "insert into usertbl_test ('dukbae', 2)\n";
    char f[] = "select * from nothing_test\n";
    char path[512];
    char buf[256];
    Cmd cmd;
    FILE *fp;
    FILE *out;

    table_path(path, "usertbl_test");
    remove(path);

    cmd = parse(a);
    assert(cmd.kind == INSERT);
    assert(strcmp(cmd.table, "usertbl_test") == 0);
    assert(strcmp(cmd.vals, "('dukbae', 2, 'db2', '010-1234-5678', 'cold')") == 0);

    out = tmpfile();
    assert(out != NULL);
    assert(execute(cmd, out) == 1);
    rewind(out);
    assert(fgets(buf, sizeof(buf), out));
    assert(strcmp(buf, "saved\n") == 0);
    fclose(out);

    cmd = parse(b);
    out = tmpfile();
    assert(out != NULL);
    assert(execute(cmd, out) == 1);
    fclose(out);

    fp = fopen(path, "r");
    assert(fp != NULL);
    assert(fgets(buf, sizeof(buf), fp));
    assert(strcmp(buf, "dukbae,2,db2,010-1234-5678,cold\n") == 0);
    assert(fgets(buf, sizeof(buf), fp));
    assert(strcmp(buf, "minsu,3,db3,010-0000-0000,good\n") == 0);
    fclose(fp);

    cmd = parse(c);
    assert(cmd.kind == SELECT);
    out = tmpfile();
    assert(out != NULL);
    assert(execute(cmd, out) == 1);
    rewind(out);
    assert(fgets(buf, sizeof(buf), out));
    assert(strcmp(buf, "dukbae,2,db2,010-1234-5678,cold\n") == 0);
    assert(fgets(buf, sizeof(buf), out));
    assert(strcmp(buf, "minsu,3,db3,010-0000-0000,good\n") == 0);
    fclose(out);

    cmd = parse(d);
    assert(cmd.kind == EXIT);

    cmd = parse(e);
    assert(cmd.kind == BAD);

    cmd = parse(f);
    out = tmpfile();
    assert(out != NULL);
    assert(execute(cmd, out) == 0);
    rewind(out);
    assert(fgets(buf, sizeof(buf), out));
    assert(strcmp(buf, "no table\n") == 0);
    fclose(out);

    remove(path);
    return 0;
}
