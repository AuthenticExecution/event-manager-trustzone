#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <unistd.h>

typedef struct ParseState ParseState;

ParseState* create_parse_state(uint8_t* buf, size_t len);
void free_parse_state(ParseState* state);
int parse_byte(ParseState* state, uint8_t* byte);
int parse_int(ParseState* state, uint16_t* i);
int parse_string(ParseState* state, char** str);
int parse_raw_data(ParseState* state, size_t len, uint8_t** buf);
int parse_all_raw_data(ParseState* state, uint8_t** buf, size_t* len);

#endif