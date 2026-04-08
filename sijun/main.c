#define _POSIX_C_SOURCE 200809L

#include "main.h"
#include <stdio.h>

#ifndef SIJUN_TEST_BUILD
int main(void) {
    return run_repl(stdin, stdout, stderr);
}
#endif
