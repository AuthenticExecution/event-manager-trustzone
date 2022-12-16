#include "utils.h"

#include <stdlib.h>
#include <string.h>

struct ParseState
{
    uint8_t* buf;
    size_t   len;
};

ParseState* create_parse_state(uint8_t* buf, size_t len)
{
    ParseState* state = malloc(sizeof(ParseState));
    state->buf = buf;
    state->len = len;
    return state;
}

void free_parse_state(ParseState* state)
{
    free(state);
}

static void advance_state(ParseState* state, size_t len)
{
    state->buf += len;
    state->len -= len;
}

int parse_byte(ParseState* state, uint8_t* byte)
{
    if (state->len < 1)
        return 0;

    *byte = state->buf[0];
    advance_state(state, 1);
    return 1;
}

int parse_int(ParseState* state, uint16_t* i)
{
    if (state->len < 2)
        return 0;

    uint8_t msb = state->buf[0];
    uint8_t lsb = state->buf[1];
    *i = (msb << 8) | lsb;
    advance_state(state, 2);
    return 1;
}

int parse_string(ParseState* state, char** str)
{
    uint8_t* end = memchr(state->buf, 0x00, state->len);

    if (end == NULL)
        return 0;

    size_t str_len = end - state->buf;
    *str = (char*)state->buf;
    advance_state(state, str_len + 1);
    return 1;
}

int parse_raw_data(ParseState* state, size_t len, uint8_t** buf)
{
    if (state->len < len)
        return 0;

    *buf = state->buf;
    advance_state(state, len);
    return 1;
}

int parse_all_raw_data(ParseState* state, uint8_t** buf, size_t* len)
{
    *buf = state->buf;
    *len = state->len;
    advance_state(state, state->len);
    return 1;
}