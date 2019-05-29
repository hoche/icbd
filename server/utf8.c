/*
 * This code added by hoche, 5/25/2019, to support utf-8 filtering.
 *
 * UTF-8 encoding looks like this:
 * 
 * Char. number range  |        UTF-8 octet sequence
 *     (hexadecimal)    |              (binary)
 *  --------------------+---------------------------------------------
 *  0000 0000-0000 007F | 0xxxxxxx
 *  0000 0080-0000 07FF | 110xxxxx 10xxxxxx
 *  0000 0800-0000 FFFF | 1110xxxx 10xxxxxx 10xxxxxx
 *  0001 0000-0010 FFFF | 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
 *
 *
 * The contents of this file (below this line) are subject to the following
 * license:
 *
 * Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details
 *
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "mdb.h"

#include "utf8.h"

static const uint8_t utf8d[] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
    8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
    0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
    0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
    0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
    1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
    1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
    1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

/* UTF-8 is a variable length character encoding. To decode a character one
 * or more bytes have to be read from a string. The decode function implements
 * a single step in this process. It takes two parameters maintaining state
 * and a byte, and returns the state achieved after processing the byte.
 * Specifically, it returns the value UTF8_ACCEPT (0) if enough bytes have
 * been read for a character, UTF8_REJECT (1) if the byte is not allowed to
 * occur at its position, and some other positive value if more bytes have to
 * be read.
 *
 * When decoding the first byte of a string, the caller must set the state
 * variable to UTF8_ACCEPT. If, after decoding one or more bytes the state
 * UTF8_ACCEPT is reached again, then the decoded Unicode character value is
 * available through the codep parameter. If the state UTF8_REJECT is entered,
 * that state will never be exited unless the caller intervenes. See the
 * examples below for more information on usage and error handling, and the
 * section on implementation details for how the decoder is constructed.
 */
uint32_t
decode(uint32_t* state, uint32_t* codep, uint8_t byte) {
    uint32_t type = utf8d[byte];

    *codep = (*state != UTF8_ACCEPT) ?
        (byte & 0x3fu) | (*codep << 6) :
        (0xff >> type) & (byte);

    *state = utf8d[256 + *state*16 + type];
    return *state;
}

int
countCodePoints(uint8_t* s, size_t* count) {
    uint32_t codepoint;
    uint32_t state = 0;

    for (*count = 0; *s; ++s)
        if (!decode(&state, &codepoint, *s))
            *count += 1;

    return state != UTF8_ACCEPT;
}

void
printCodePoints(uint8_t* s) {
    uint32_t codepoint;
    uint32_t state = 0;

    for (; *s; ++s) {
        if (!decode(&state, &codepoint, *s)) {
            vmdb(MSG_DEBUG, "U+%04X\n", codepoint);
            //printf("U+%04X\n", codepoint);
        }
    }

    if (state != UTF8_ACCEPT) {
        vmdb(MSG_DEBUG, "The string is not well-formed\n");
        //printf("The string is not well-formed\n");
    }

}
