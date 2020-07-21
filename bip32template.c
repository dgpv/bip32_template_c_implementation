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

#include <limits.h>
#include <assert.h>

#include "bip32template.h"

#define HARDENED_INDEX_START 0x80000000
#define MAX_INDEX_VALUE (HARDENED_INDEX_START-1)
#define INVALID_INDEX HARDENED_INDEX_START

#define HARDENED_MARKER_LETTER 'h'
#define HARDENED_MARKER_APOSTROPHE '\''

typedef enum {
    STATE_PARSE_INVALID,
    STATE_PARSE_SUCCESS,
    STATE_PARSE_ERROR,

    STATE_PARSE_NEXT_SECTION,
    STATE_PARSE_SECTION_START,
    STATE_PARSE_RANGE_WITHIN_SECTION,
    STATE_PARSE_SECTION_END,
    STATE_PARSE_VALUE
} parse_state_t;

typedef enum {
    RANGE_CORRECTNESS_FLAG_RANGE_NEXT,
    RANGE_CORRECTNESS_FLAG_RANGE_LAST
} range_correctness_flag_t;

static int is_parse_finished(parse_state_t state) {
    assert( state != STATE_PARSE_INVALID );

    if( state == STATE_PARSE_SUCCESS || state == STATE_PARSE_ERROR ) {
        return 1;
    }

    return 0;
}

static int is_digit(char c) {
    if( c >= '0' && c <= '9' ) {
        return 1;
    }
    return 0;
}

static bip32_template_error_t unexpected_char_error(char c) {
    if( c == 0 ) {
        return BIP32_TEMPLATE_ERROR_UNEXPECTED_FINISH;
    }
    if( c == ' ' || c == '\t' ) {
        return BIP32_TEMPLATE_ERROR_UNEXPECTED_SPACE;
    }
    if( c == '/' || c == '[' || c == ']' || c == '-' || c == ',' || c == '*'
        || c == 'h' || c == '\'' || is_digit(c) )
    {
        return BIP32_TEMPLATE_ERROR_UNEXPECTED_CHAR;
    }
    return BIP32_TEMPLATE_ERROR_INVALID_CHAR;
}


static int process_digit(char c, uint32_t* index_value_p,
                         parse_state_t* state_p, bip32_template_error_t* error_p)
{
    assert( is_digit(c) );

    uint32_t v = c - '0';
    uint32_t index_value = *index_value_p;
    uint32_t new_value;

    if( index_value == 0 ) {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_INDEX_HAS_LEADING_ZERO;
        return 0;
    }

    if( index_value != INVALID_INDEX
        && ( index_value > (MAX_INDEX_VALUE / 10)
             || ( index_value == (MAX_INDEX_VALUE / 10)
                  && v > (MAX_INDEX_VALUE % 10) ) ) )
    {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_INDEX_TOO_BIG;
        return 0;
    }

    if( index_value == INVALID_INDEX ) {
        new_value = v;
    }
    else {
        new_value = index_value * 10 + v;
    }
    assert( new_value <= MAX_INDEX_VALUE );

    *index_value_p = new_value;

    return 1;
}

static bip32_template_section_t* get_last_section(bip32_template_t* template_p)
{
    assert( template_p->num_sections < BIP32_TEMPLATE_MAX_SECTIONS );
    return &template_p->sections[template_p->num_sections];
}

static bip32_template_section_range_t* get_last_section_range(bip32_template_section_t* section_p)
{
    assert( section_p->num_ranges < BIP32_TEMPLATE_MAX_RANGES_PER_SECTION );

    return &section_p->ranges[section_p->num_ranges];
}

static void open_path_section_range(bip32_template_t* template_p, uint32_t index_value)
{
    bip32_template_section_range_t* range_p = get_last_section_range(get_last_section(template_p));
    range_p->range_start = index_value;
    assert( range_p->range_end == INVALID_INDEX );
}

static int is_range_open(bip32_template_section_range_t* range_p)
{
    return range_p->range_start != INVALID_INDEX && range_p->range_end == INVALID_INDEX;
}

static int finalize_last_section(bip32_template_t* template_p, uint32_t index_value)
{
    assert( index_value != INVALID_INDEX );
    bip32_template_section_range_t* range_p = get_last_section_range(get_last_section(template_p));
    if( is_range_open(range_p) ) {
        assert( range_p->range_end == INVALID_INDEX );
        range_p->range_end = index_value;
        return 1;
    }
    assert( range_p->range_start == INVALID_INDEX || range_p->range_end != INVALID_INDEX );
    if( range_p->range_start == INVALID_INDEX ) {
        range_p->range_start = index_value;
    }
    assert( range_p->range_end == INVALID_INDEX || range_p->range_end == index_value );
    range_p->range_end = index_value;
    return 0;
}

static void normalize_last_section_and_advance_ranges(bip32_template_t* template_p)
{
    bip32_template_section_t* section_p = get_last_section(template_p);

    assert( section_p->num_ranges < BIP32_TEMPLATE_MAX_RANGES_PER_SECTION );

    bip32_template_section_range_t* last_range_p = &section_p->ranges[section_p->num_ranges];

    if( section_p->num_ranges == 0 ) {
        section_p->num_ranges++;
        return;
    }

    bip32_template_section_range_t* prev_range_p = &section_p->ranges[section_p->num_ranges-1];

    assert( last_range_p->range_start <= MAX_INDEX_VALUE );
    assert( last_range_p->range_end <= MAX_INDEX_VALUE );
    assert( prev_range_p->range_start <= MAX_INDEX_VALUE );
    assert( prev_range_p->range_end <= MAX_INDEX_VALUE );

    if( prev_range_p->range_end + 1 == last_range_p->range_start ) {
        prev_range_p->range_end = last_range_p->range_end;
        last_range_p->range_start = INVALID_INDEX;
        last_range_p->range_end = INVALID_INDEX;
    }
    else {
        section_p->num_ranges++;
    }
}

static void harden_last_section(bip32_template_t* template_p)
{
    int i;

    bip32_template_section_t* section_p = get_last_section(template_p);

    for( i = 0; i < section_p->num_ranges; i++ ) {
        assert( section_p->ranges[i].range_start <= MAX_INDEX_VALUE );
        assert( section_p->ranges[i].range_end <= MAX_INDEX_VALUE );

        section_p->ranges[i].range_start += HARDENED_INDEX_START;
        section_p->ranges[i].range_end += HARDENED_INDEX_START;
    }
}

static int is_prev_section_hardened(bip32_template_t* template_p)
{
    assert( template_p->num_sections > 0 );

    int i;
    int is_hardened = -1;
    bip32_template_section_t* section_p = &template_p->sections[template_p->num_sections-1];

    for( i = 0; i < section_p->num_ranges; i++ ) {
        if( section_p->ranges[i].range_start >= HARDENED_INDEX_START ) {
            assert( is_hardened != 0 );
            is_hardened = 1;
        }
        else {
            assert( is_hardened != 1 );
            is_hardened = 0;
        }

        if( section_p->ranges[i].range_end >= HARDENED_INDEX_START ) {
            assert( is_hardened != 0 );
            is_hardened = 1;
        }
        else {
            assert( is_hardened != 1 );
            is_hardened = 0;
        }
    }

    return is_hardened;
}

static int check_range_correctness(bip32_template_t* template_p,
                                   parse_state_t* state_p, bip32_template_error_t* error_p,
                                   int range_was_open, int is_format_unambiguous,
                                   range_correctness_flag_t flag)
{
    bip32_template_section_t* section_p = get_last_section(template_p);
    bip32_template_section_range_t* range_p = get_last_section_range(section_p);
    bip32_template_section_range_t* prev_range_p;

    assert( range_p->range_start <= MAX_INDEX_VALUE );
    assert( range_p->range_end <= MAX_INDEX_VALUE );

    int is_start_equals_end = range_p->range_start == range_p->range_end;
    int is_range_equals_wildcard = range_p->range_start == 0 && range_p->range_end == MAX_INDEX_VALUE;
    int is_start_larger_than_end = range_p->range_start > range_p->range_end;
    int is_single_index = ( flag == RANGE_CORRECTNESS_FLAG_RANGE_LAST
                            ? (section_p->num_ranges == 0 && is_start_equals_end)
                            : 0 );

    int is_start_before_previous = 0;
    int is_start_in_previous = 0;
    int is_start_next_to_previous = 0;
    if( section_p->num_ranges > 0 ) {
        prev_range_p = &section_p->ranges[section_p->num_ranges-1];
        assert( prev_range_p->range_start <= MAX_INDEX_VALUE );
        assert( prev_range_p->range_end <= MAX_INDEX_VALUE );
        is_start_before_previous = prev_range_p->range_start > range_p->range_start;
        is_start_in_previous = ( prev_range_p->range_start <= range_p->range_start
                                 && prev_range_p->range_end >= range_p->range_start );
        is_start_next_to_previous = prev_range_p->range_end + 1 == range_p->range_start;
    }

    if( is_single_index ) {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_SINGLE_INDEX_AS_RANGE;
        return 0;
    }

    if( range_was_open && is_start_equals_end ) {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_RANGE_START_EQUALS_END;
        return 0;
    }

    if( is_format_unambiguous && is_start_next_to_previous ) {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_RANGE_START_NEXT_TO_PREVIOUS;
        return 0;
    }

    if( is_range_equals_wildcard ) {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_RANGE_EQUALS_WILDCARD;
        return 0;
    }

    if( (range_was_open && is_start_larger_than_end) || is_start_before_previous ) {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_RANGE_ORDER_BAD;
        return 0;
    }

    if( is_start_in_previous ) {
        *state_p = STATE_PARSE_ERROR;
        *error_p = BIP32_TEMPLATE_ERROR_RANGES_INTERSECT;
        return 0;
    }

    return 1;
}

void bip32_template_context_set_string(const char* template_string, bip32_template_getchar_context_t* ctx)
{
    ctx->pos = 0;
    ctx->stop = 0;
    ctx->data.str = template_string;
}

int bip32_template_getchar(bip32_template_getchar_context_t* ctx, char* out_p)
{
    if( ctx->pos == UINT_MAX ) {
        ctx->stop = 1;
    }

    if( ctx->stop ) {
        return 0;
    }

    ctx->pos++;

    *out_p = ctx->data.str[ctx->pos-1];

    if( *out_p == 0 ) {
        ctx->stop = 1;
    }

    return 1;
}

int bip32_template_parse(bip32_template_getchar_func_t get_char, bip32_template_getchar_context_t* ctx,
                         bip32_template_format_mode_t mode,
                         bip32_template_t* template_p, bip32_template_error_t* error_p)
{
    parse_state_t state = STATE_PARSE_SECTION_START;
    bip32_template_error_t error = BIP32_TEMPLATE_ERROR_UNDEFINED;
    parse_state_t return_state = STATE_PARSE_INVALID;
    uint32_t index_value = INVALID_INDEX;
    int is_format_unambiguous = mode == BIP32_TEMPLATE_FORMAT_UNAMBIGOUS;
    int is_format_onlypath = mode == BIP32_TEMPLATE_FORMAT_ONLYPATH;
    char accepted_hardened_markers[2] = { HARDENED_MARKER_LETTER,
                                          HARDENED_MARKER_APOSTROPHE };
    char c;
    int i, ii;

    template_p->num_sections = 0;
    for( i = 0; i < BIP32_TEMPLATE_MAX_SECTIONS; i++ ) {
        template_p->sections[i].num_ranges = 0;
        for( ii = 0; ii < BIP32_TEMPLATE_MAX_RANGES_PER_SECTION; ii++ ) {
            template_p->sections[i].ranges[ii].range_start = INVALID_INDEX;
            template_p->sections[i].ranges[ii].range_end = INVALID_INDEX;
        }
    }

    while( !is_parse_finished(state) ) {
        if( !get_char(ctx, &c) ) {
            state = STATE_PARSE_ERROR;
            error = BIP32_TEMPLATE_ERROR_GETCHAR_FAILED;
            break;
        }

        if( state == STATE_PARSE_VALUE && !is_digit(c) ) {
            assert( return_state != STATE_PARSE_INVALID );
            assert( return_state != STATE_PARSE_VALUE );
            state = return_state;
            return_state = STATE_PARSE_INVALID;
        }
        switch( state ) {
            case STATE_PARSE_SECTION_START:
                {
                    if( (c == '[' || c == '*') && !is_format_onlypath
                        && template_p->num_sections == BIP32_TEMPLATE_MAX_SECTIONS )
                    {
                        state = STATE_PARSE_ERROR;
                        error = BIP32_TEMPLATE_ERROR_PATH_TOO_LONG;
                    }
                    else if( c == '/' && !is_format_onlypath ) {
                        state = STATE_PARSE_ERROR;
                        error = BIP32_TEMPLATE_ERROR_UNEXPECTED_SLASH;
                    }
                    else if( c == '[' && !is_format_onlypath ) {
                        index_value = INVALID_INDEX;
                        state = STATE_PARSE_VALUE;
                        return_state = STATE_PARSE_RANGE_WITHIN_SECTION;
                    }
                    else if( c == '*' && !is_format_onlypath ) {
                        open_path_section_range(template_p, 0);
                        index_value = MAX_INDEX_VALUE;
                        state = STATE_PARSE_SECTION_END;
                    }
                    else if( is_digit(c)
                             && template_p->num_sections == BIP32_TEMPLATE_MAX_SECTIONS )
                    {
                        if( process_digit(c, &index_value, &state, &error) ) {
                            state = STATE_PARSE_ERROR;
                            error = BIP32_TEMPLATE_ERROR_PATH_TOO_LONG;
                        }
                    }
                    else if( is_digit(c) ) {
                        if( process_digit(c, &index_value, &state, &error) ) {
                            state = STATE_PARSE_VALUE;
                            return_state = STATE_PARSE_SECTION_END;
                        }
                    }
                    else if( c == 0 ) {
                        if( template_p->num_sections == 0 ) {
                            state = STATE_PARSE_ERROR;
                            error = BIP32_TEMPLATE_ERROR_PATH_EMPTY;
                        }
                        else {
                            state = STATE_PARSE_ERROR;
                            error = BIP32_TEMPLATE_ERROR_UNEXPECTED_SLASH;
                        }
                    }
                    else {
                        state = STATE_PARSE_ERROR;
                        error = unexpected_char_error(c);
                    }
                } break;

            case STATE_PARSE_NEXT_SECTION:
                {
                    if( c == '/' ) {
                        state = STATE_PARSE_SECTION_START;
                    }
                    else if( c == 0 && template_p->num_sections == BIP32_TEMPLATE_MAX_SECTIONS ) {
                        state = STATE_PARSE_ERROR;
                        error = BIP32_TEMPLATE_ERROR_PATH_TOO_LONG;
                    }
                    else if( c == 0 ) {
                        state = STATE_PARSE_SUCCESS;
                    }
                    else {
                        state = STATE_PARSE_ERROR;
                        error = unexpected_char_error(c);
                    }
                } break;

            case STATE_PARSE_RANGE_WITHIN_SECTION:
                {
                    assert( !is_format_onlypath );

                    if( c == 0 ) {
                        state = STATE_PARSE_ERROR;
                        error = BIP32_TEMPLATE_ERROR_UNEXPECTED_FINISH;
                    }
                    else if( index_value == INVALID_INDEX ) {
                        if( c == ' ' ) {
                            state = STATE_PARSE_ERROR;
                            error = BIP32_TEMPLATE_ERROR_UNEXPECTED_SPACE;
                        }
                        else {
                            state = STATE_PARSE_ERROR;
                            error = BIP32_TEMPLATE_ERROR_DIGIT_EXPECTED;
                        }
                    }
                    else if( c == '-' ) {
                        if( !is_range_open(
                                    get_last_section_range(
                                        get_last_section(template_p))) )
                        {
                            open_path_section_range(template_p, index_value);
                            index_value = INVALID_INDEX;
                            state = STATE_PARSE_VALUE;
                            return_state = STATE_PARSE_RANGE_WITHIN_SECTION;
                        }
                        else {
                            state = STATE_PARSE_ERROR;
                            error = unexpected_char_error(c);
                        }
                    }
                    else if( c == ',' ) {
                        if( template_p->sections[template_p->num_sections].num_ranges
                                == BIP32_TEMPLATE_MAX_RANGES_PER_SECTION - 1 )
                        {
                            state = STATE_PARSE_ERROR;
                            error = BIP32_TEMPLATE_ERROR_PATH_SECTION_TOO_LONG;
                        }
                        else {
                            int was_open = finalize_last_section(template_p, index_value);
                            if( check_range_correctness(template_p, &state, &error,
                                                        was_open, is_format_unambiguous,
                                                        RANGE_CORRECTNESS_FLAG_RANGE_NEXT) )
                            {
                                normalize_last_section_and_advance_ranges(template_p);
                                index_value = INVALID_INDEX;
                                state = STATE_PARSE_VALUE;
                                return_state = STATE_PARSE_RANGE_WITHIN_SECTION;
                            }
                        }
                    }
                    else if( c == ']' ) {
                        int was_open = finalize_last_section(template_p, index_value);
                        if( check_range_correctness(template_p, &state, &error,
                                                    was_open, is_format_unambiguous,
                                                    RANGE_CORRECTNESS_FLAG_RANGE_LAST) )
                        {
                            state = STATE_PARSE_SECTION_END;
                        }
                    }
                    else {
                        state = STATE_PARSE_ERROR;
                        error = unexpected_char_error(c);
                    }
                } break;

            case STATE_PARSE_SECTION_END:
                {
                    assert( index_value != INVALID_INDEX );
                    if( c == 0 && template_p->num_sections == BIP32_TEMPLATE_MAX_SECTIONS ) {
                        state = STATE_PARSE_ERROR;
                        error = BIP32_TEMPLATE_ERROR_PATH_TOO_LONG;
                    }
                    else if( c == '/' ) {
                        finalize_last_section(template_p, index_value);
                        normalize_last_section_and_advance_ranges(template_p);
                        assert( template_p->num_sections < BIP32_TEMPLATE_MAX_SECTIONS );
                        template_p->num_sections++;
                        index_value = INVALID_INDEX;
                        state = STATE_PARSE_SECTION_START;
                    }
                    else if( c == 0 ) {
                        finalize_last_section(template_p, index_value);
                        normalize_last_section_and_advance_ranges(template_p);
                        assert( template_p->num_sections < BIP32_TEMPLATE_MAX_SECTIONS );
                        template_p->num_sections++;
                        index_value = INVALID_INDEX;
                        state = STATE_PARSE_SUCCESS;
                    }
                    else if( c == accepted_hardened_markers[0]
                                || c == accepted_hardened_markers[1] )
                    {
                        if( template_p->num_sections > 0
                            && !is_prev_section_hardened(template_p) )
                        {
                            state = STATE_PARSE_ERROR;
                            error = BIP32_TEMPLATE_ERROR_GOT_HARDENED_AFTER_UNHARDENED;
                        }
                        else {
                            accepted_hardened_markers[0] = c;
                            accepted_hardened_markers[1] = c;
                            finalize_last_section(template_p, index_value);
                            normalize_last_section_and_advance_ranges(template_p);
                            harden_last_section(template_p);
                            assert( template_p->num_sections < BIP32_TEMPLATE_MAX_SECTIONS );
                            template_p->num_sections++;
                            index_value = INVALID_INDEX;
                            state = STATE_PARSE_NEXT_SECTION;
                        }
                    }
                    else if( c == HARDENED_MARKER_LETTER
                                || c == HARDENED_MARKER_APOSTROPHE )
                    {
                        state = STATE_PARSE_ERROR;
                        error = BIP32_TEMPLATE_ERROR_UNEXPECTED_HARDENED_MARKER;
                    }
                    else {
                        state = STATE_PARSE_ERROR;
                        error = unexpected_char_error(c);
                    }
                } break;

            case STATE_PARSE_VALUE:
                {
                    process_digit(c, &index_value, &state, &error);
                } break;

            default:
                /* should not happen, all cases must be hanlded */
                assert(0); /* UNREACHABLE */
        }

        if( c == 0 ) {
            assert( is_parse_finished(state) );
            break;
        }
    }

    assert( error == BIP32_TEMPLATE_ERROR_UNDEFINED || state == STATE_PARSE_ERROR );
    assert( error != BIP32_TEMPLATE_ERROR_UNDEFINED || state == STATE_PARSE_SUCCESS );

    if( error_p ) {
        *error_p = error;
    }
    return state == STATE_PARSE_SUCCESS;
}

int bip32_template_parse_string(const char* template_string, bip32_template_format_mode_t mode,
                                bip32_template_t* template_p, bip32_template_error_t* error_p,
                                unsigned int* last_pos_p)
{
    bip32_template_getchar_context_t ctx;
    bip32_template_context_set_string(template_string, &ctx);
    int result = bip32_template_parse(bip32_template_getchar, &ctx, mode, template_p, error_p);
    if( last_pos_p ) {
        *last_pos_p = ctx.pos;
    }
    return result;
}

int bip32_template_match(const bip32_template_t* template_p, const uint32_t* path_p, unsigned int path_len)
{
    int i, ii;
    int range_match;

    if( template_p->num_sections != path_len ) {
        return 0;
    }
    for( i = 0; i < template_p->num_sections; i++ ) {
        range_match = 0;
        for( ii = 0; ii < template_p->sections[i].num_ranges; ii++ ) {
            if( path_p[i] < template_p->sections[i].ranges[ii].range_start
                || path_p[i] > template_p->sections[i].ranges[ii].range_end )
            {
                /* Do nothing.
                 * This way the condition check here matches
                 * the condition check in the formal spec */
            }
            else {
                range_match = 1;
                break;
            }
        }
        if( ! range_match ) {
            return 0;
        }
    }

    return 1;
}

/* Convert template to a simple path.
 * Returns 0 if any section contains more than one range
 * or any range has range_start != range_end,
 * Returns 1 otherwise, and puts the path into path_p and path len into path_len_p
 * Caller must set *path_len_p to the available number of elements in path_p */
int bip32_template_to_path(const bip32_template_t* template_p, uint32_t* path_p, unsigned int* path_len_p)
{
    int i;

    if( template_p->num_sections > *path_len_p ) {
        return 0;
    }

    for( i = 0; i < template_p->num_sections; i++ ) {
        if( template_p->sections[i].num_ranges != 1 ) {
            return 0;
        }
        if( template_p->sections[i].ranges[0].range_start
            != template_p->sections[i].ranges[0].range_end )
        {
            return 0;
        }
        path_p[i] = template_p->sections[i].ranges[0].range_start;
    }

    *path_len_p = template_p->num_sections;

    return 1;
}

const char* bip32_template_error_to_string(bip32_template_error_t error)
{
    switch( error ) {
        case BIP32_TEMPLATE_ERROR_GETCHAR_FAILED:
            return "failed to retrieve next character";
        case BIP32_TEMPLATE_ERROR_UNEXPECTED_HARDENED_MARKER:
            return "unexpected hardened marker";
        case BIP32_TEMPLATE_ERROR_UNEXPECTED_SPACE:
            return "unexpected space";
        case BIP32_TEMPLATE_ERROR_UNEXPECTED_CHAR:
            return "unexpected character";
        case BIP32_TEMPLATE_ERROR_UNEXPECTED_FINISH:
            return "unexpected finish";
        case BIP32_TEMPLATE_ERROR_UNEXPECTED_SLASH:
            return "unexpected slash";
        case BIP32_TEMPLATE_ERROR_INVALID_CHAR:
            return "invalid character";
        case BIP32_TEMPLATE_ERROR_INDEX_TOO_BIG:
            return "index too big";
        case BIP32_TEMPLATE_ERROR_INDEX_HAS_LEADING_ZERO:
            return "index has leading zero";
        case BIP32_TEMPLATE_ERROR_PATH_EMPTY:
            return "path is empty";
        case BIP32_TEMPLATE_ERROR_PATH_TOO_LONG:
            return "path too long";
        case BIP32_TEMPLATE_ERROR_PATH_SECTION_TOO_LONG:
            return "path section too long";
        case BIP32_TEMPLATE_ERROR_RANGES_INTERSECT:
            return "intersecting range encountered";
        case BIP32_TEMPLATE_ERROR_RANGE_ORDER_BAD:
            return "indexes are ordered incorrectly within the section";
        case BIP32_TEMPLATE_ERROR_RANGE_EQUALS_WILDCARD:
            return "range equals wildcard, should be specified as \"*\" instead";
        case BIP32_TEMPLATE_ERROR_SINGLE_INDEX_AS_RANGE:
            return "single index is specified within range";
        case BIP32_TEMPLATE_ERROR_RANGE_START_EQUALS_END:
            return "range start equals range end";
        case BIP32_TEMPLATE_ERROR_RANGE_START_NEXT_TO_PREVIOUS:
            return "adjacent ranges not allowed, should be specified as single range";
        case BIP32_TEMPLATE_ERROR_GOT_HARDENED_AFTER_UNHARDENED:
            return "hardened derivation specified after unhardened";
        case BIP32_TEMPLATE_ERROR_DIGIT_EXPECTED:
            return "digit expected";
        case BIP32_TEMPLATE_ERROR_UNDEFINED:
            return "<undefined error>";
        default:
            /* should not happen, all cases must be hanlded */
            assert( 0 ); /* UNREACHABLE */
            return "<unexpected error code>";
    }
}
