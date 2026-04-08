#ifndef SQL_ENGINE_H
#define SQL_ENGINE_H

#include <stddef.h>
#include <stdio.h>

#define SQL_INPUT_BUFFER_SIZE 4096

enum ReadSqlStatus {
    READ_SQL_OK = 0,
    READ_SQL_EMPTY = 1,
    READ_SQL_TOO_LONG = 2,
    READ_SQL_ERROR = 3
};

typedef struct {
    const char *raw_sql;
} ParsedStatement;

typedef struct {
    const char *message;
} ExecuteResult;

void print_usage(FILE *stream, const char *program_name);
int read_sql_input(FILE *input, char *buffer, size_t buffer_size);
int parse(const char *sql, ParsedStatement *out);
int execute(const ParsedStatement *stmt, ExecuteResult *out);

#endif
