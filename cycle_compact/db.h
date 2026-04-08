#ifndef DB_H
#define DB_H

#include <stdio.h>

typedef enum { BAD, INSERT, SELECT } Type;

typedef struct {
    Type type;
    int id;
    char mail[256];
} Stmt;

Stmt parse(char *sql);
int execute(Stmt s);
int run(Stmt s, const char *file, FILE *out);

#endif
