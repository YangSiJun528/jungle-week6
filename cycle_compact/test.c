#define TESTING
#include "main.c"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    char ins1[] = "insert 1 a@a.com";
    char ins2[] = "insert 2 b@b.com";
    char sel[] = "select";
    char buf[512];
    Stmt s;
    FILE *fp;
    FILE *out;

    s = parse(ins1);
    assert(s.type == INSERT);
    assert(s.id == 1);
    assert(strcmp(s.mail, "a@a.com") == 0);

    s = parse(sel);
    assert(s.type == SELECT);

    fp = fopen("test.csv", "w");
    assert(fp);
    fputs("id,mail\n", fp);
    fclose(fp);

    out = tmpfile();
    assert(out);
    s = parse(ins1);
    assert(run(s, "test.csv", out) == 0);
    s = parse(ins2);
    assert(run(s, "test.csv", out) == 0);
    fclose(out);

    out = tmpfile();
    assert(out);
    s = parse(sel);
    assert(run(s, "test.csv", out) == 0);
    rewind(out);

    assert(fgets(buf, sizeof(buf), out));
    assert(strcmp(buf, "1 a@a.com\n") == 0);
    assert(fgets(buf, sizeof(buf), out));
    assert(strcmp(buf, "2 b@b.com\n") == 0);
    assert(!fgets(buf, sizeof(buf), out));

    fclose(out);
    remove("test.csv");
    return 0;
}
