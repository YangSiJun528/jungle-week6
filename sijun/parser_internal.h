#ifndef PARSER_INTERNAL_H
#define PARSER_INTERNAL_H

#include "main.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *input;
    size_t position;
    const char *error_message;
    size_t error_position;
} Parser;

bool parse_select_query(Parser *parser, ParseResult *result);
bool parse_insert_query(Parser *parser, ParseResult *result);
bool parse_value_list(Parser *parser, ParseResult *result);

bool parse_value(Parser *parser, SqlValue *out);
bool parse_number(Parser *parser, SqlValue *out);
bool parse_string(Parser *parser, SqlValue *out);
bool parse_identifier(Parser *parser, char **out);
bool append_value(ParseResult *result, SqlValue value);

void consume_ws(Parser *parser);
bool consume_req_ws(Parser *parser);
bool consume_literal(Parser *parser, const char *literal);
char *copy_range(const char *start, size_t length);
char current_char(const Parser *parser);
bool is_ascii_letter(char ch);
bool is_ascii_digit(char ch);
bool is_sql_whitespace(char ch);
bool is_identifier_start(char ch);
bool is_identifier_char(char ch);
bool is_string_char(char ch);
void set_parse_error(Parser *parser, const char *message);

#endif
