#pragma once

#include <stdint.h>
#include <string.h>

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

uint32_t inline decode(uint32_t* state, uint32_t* codep, uint32_t byte);
uint32_t validate_utf8(uint32_t *state, char *str, size_t len);
