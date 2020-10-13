/*
 * Copyright 2020 Dmitry Petukhov https://github.com/dgpv
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _BIP32_TEMPLATE_H_
#define _BIP32_TEMPLATE_H_

#include <stdint.h>

/* NOTE: uint8_t is used to hold number of sections and ranges */
#ifndef BIP32_TEMPLATE_MAX_SECTIONS
#define BIP32_TEMPLATE_MAX_SECTIONS 8
#endif

#ifndef BIP32_TEMPLATE_MAX_RANGES_PER_SECTION
#define BIP32_TEMPLATE_MAX_RANGES_PER_SECTION 4
#endif

_Static_assert(BIP32_TEMPLATE_MAX_SECTIONS <= 255, "should fit into uint8_t");
_Static_assert(BIP32_TEMPLATE_MAX_SECTIONS > 0, "cannot be zero");
_Static_assert(BIP32_TEMPLATE_MAX_RANGES_PER_SECTION <= 255,
               "should fit into uint8_t");
_Static_assert(BIP32_TEMPLATE_MAX_RANGES_PER_SECTION > 0, "cannot be zero");

typedef struct {
    uint32_t range_start;
    uint32_t range_end;
} bip32_template_section_range_type;

typedef struct {
    uint8_t num_ranges; 
    bip32_template_section_range_type ranges[BIP32_TEMPLATE_MAX_RANGES_PER_SECTION];
} bip32_template_section_type;

typedef struct {
    uint8_t is_partial;
    uint8_t num_sections; 
    bip32_template_section_type sections[BIP32_TEMPLATE_MAX_SECTIONS];
} bip32_template_type;

typedef enum {
    BIP32_TEMPLATE_ERROR_UNDEFINED,

    BIP32_TEMPLATE_ERROR_FIRST = BIP32_TEMPLATE_ERROR_UNDEFINED,

    BIP32_TEMPLATE_ERROR_GETCHAR_FAILED,
    BIP32_TEMPLATE_ERROR_UNEXPECTED_HARDENED_MARKER,
    BIP32_TEMPLATE_ERROR_UNEXPECTED_SPACE,
    BIP32_TEMPLATE_ERROR_UNEXPECTED_CHAR,
    BIP32_TEMPLATE_ERROR_UNEXPECTED_FINISH,
    BIP32_TEMPLATE_ERROR_UNEXPECTED_SLASH,
    BIP32_TEMPLATE_ERROR_INVALID_CHAR,
    BIP32_TEMPLATE_ERROR_INDEX_TOO_BIG,
    BIP32_TEMPLATE_ERROR_INDEX_HAS_LEADING_ZERO,
    BIP32_TEMPLATE_ERROR_PATH_EMPTY,
    BIP32_TEMPLATE_ERROR_PATH_TOO_LONG,
    BIP32_TEMPLATE_ERROR_PATH_SECTION_TOO_LONG,
    BIP32_TEMPLATE_ERROR_RANGES_INTERSECT,
    BIP32_TEMPLATE_ERROR_RANGE_ORDER_BAD,
    BIP32_TEMPLATE_ERROR_RANGE_EQUALS_WILDCARD,
    BIP32_TEMPLATE_ERROR_SINGLE_INDEX_AS_RANGE,
    BIP32_TEMPLATE_ERROR_RANGE_START_EQUALS_END,
    BIP32_TEMPLATE_ERROR_RANGE_START_NEXT_TO_PREVIOUS,
    BIP32_TEMPLATE_ERROR_GOT_HARDENED_AFTER_UNHARDENED,
    BIP32_TEMPLATE_ERROR_DIGIT_EXPECTED,

    BIP32_TEMPLATE_ERROR_LAST = BIP32_TEMPLATE_ERROR_DIGIT_EXPECTED
} bip32_template_error_type;

typedef struct {
    unsigned int pos;
    int stop;
    union {
        void* opaque;
        const char* str;
    } data;
} bip32_template_getchar_context_type;

typedef enum {
    BIP32_TEMPLATE_FORMAT_AMBIGOUS,
    BIP32_TEMPLATE_FORMAT_UNAMBIGOUS,
    BIP32_TEMPLATE_FORMAT_ONLYPATH,
} bip32_template_format_mode_type;

typedef int (*bip32_template_getchar_func_type)(bip32_template_getchar_context_type*, char*);

void bip32_template_context_set_string(const char* template_string, bip32_template_getchar_context_type* ctx);
int bip32_template_getchar(bip32_template_getchar_context_type* ctx, char* out_p);
int bip32_template_parse(bip32_template_getchar_func_type get_char, bip32_template_getchar_context_type* ctx,
                         bip32_template_format_mode_type mode,
                         bip32_template_type* template_p, bip32_template_error_type* error_p);
int bip32_template_parse_string(const char* template_string, bip32_template_format_mode_type mode,
                                bip32_template_type* template_p, bip32_template_error_type* error_p,
                                unsigned int* last_pos_p);
int bip32_template_match(const bip32_template_type* template_p, const uint32_t* path_p, unsigned int path_len);
const char* bip32_template_error_to_string(bip32_template_error_type error);
int bip32_template_to_path(const bip32_template_type* template_p, uint32_t* path_p, unsigned int* path_len_p);

#endif /* _BIP32_TEMPLATE_H_ */
