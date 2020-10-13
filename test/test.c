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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../bip32template.h"

typedef struct {
    const char* tmpl_str;
    bip32_template_type tmpl;
} testcase_success_type;

#include "test_data.h"

static int templates_equal(bip32_template_type* a, bip32_template_type* b)
{
    int i;
    int ii;

    if( a->num_sections != b->num_sections ) {
        return 0;
    }

    for( i = 0; i < a->num_sections; i++ ) {
        if( a->sections[i].num_ranges != b->sections[i].num_ranges ) {
            return 0;
        }
        for( ii = 0; ii < a->sections[i].num_ranges; ii++ ) {
            if( a->sections[i].ranges[ii].range_start != b->sections[i].ranges[ii].range_start ) {
                return 0;
            }
            if( a->sections[i].ranges[ii].range_end != b->sections[i].ranges[ii].range_end ) {
                return 0;
            }
        }
    }

    return 1;
}

static int extract_path(bip32_template_type* template_p, uint32_t* path_p, unsigned int* path_len_p, int want_nomatch)
{
    *path_len_p = template_p->num_sections;

    int i;
    int ii;
    int have_put;
    int have_nomatch = 0;
    uint32_t val;

    for( i = 0; i < template_p->num_sections; i++ ) {
        bip32_template_section_type* section_p = &template_p->sections[i];
        if( want_nomatch && !have_nomatch ) {
            if( section_p->ranges[0].range_start < 0x80000000
                && section_p->ranges[0].range_start > 0 )
            {
                path_p[i] = 0;
                have_nomatch = 1;
            }
            else if( section_p->ranges[0].range_start <= 0xFFFFFFFF
                && section_p->ranges[0].range_start > 0x80000000 )
            {
                path_p[i] = 0x80000000;
                have_nomatch = 1;
            }
            else if( section_p->ranges[section_p->num_ranges-1].range_end < 0x7FFFFFFF ) {
                path_p[i] = 0x7FFFFFFF;
                have_nomatch = 1;
            }
            else if( section_p->ranges[section_p->num_ranges-1].range_end < 0xFFFFFFFF ) {
                path_p[i] = 0xFFFFFFFF;
                have_nomatch = 1;
            }
        }
        else {
            have_put = 0;
            for( ii = 0; ii < section_p->num_ranges; ii++ ) {
                if( (rand() & 1) == 0 ) {
                    if( (rand() & 1) == 0 ) {
                        val = section_p->ranges[ii].range_start;
                    }
                    else {
                        val = section_p->ranges[ii].range_end;
                    }
                    path_p[i] = val;
                    have_put = 1;
                }
            }
            if( ! have_put ) {
                path_p[i] = section_p->ranges[0].range_start;
            }
        }
    }
    return have_nomatch;
}

static void show_template(bip32_template_type* tmpl)
{
    int i, ii;

    fprintf(stderr, "num_sections: %u\n", tmpl->num_sections);
    for( i = 0; i < tmpl->num_sections; i++ ) {
        fprintf(stderr, "  section %d: num_ranges: %u\n", i, tmpl->sections[i].num_ranges);
        for( ii = 0; ii < tmpl->sections[i].num_ranges; ii++ ) {
            fprintf(stderr, "    range %d: (%u, %u)\n", ii,
                    tmpl->sections[i].ranges[ii].range_start,
                    tmpl->sections[i].ranges[ii].range_end);
        }
    }
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    int i, ii;
    bip32_template_type tmpl, tmpl_onlypath;
    bip32_template_error_type error, error_onlypath, expected_error;
    testcase_success_type* tcs;
    unsigned int last_pos, last_pos_onlypath, expected_pos;
    const char* tmpl_str;
    bip32_template_format_mode_type mode;
    uint32_t test_path[BIP32_TEMPLATE_MAX_SECTIONS];
    unsigned int test_path_len;

    for( i = 0; i < (int)(sizeof(testcase_success)/sizeof(testcase_success[0])); i++ ) {
        tcs = &testcase_success[i];
        if( !bip32_template_parse_string(tcs->tmpl_str, BIP32_TEMPLATE_FORMAT_AMBIGOUS,
                                         &tmpl, &error, &last_pos) )
        {
            fprintf(stderr, "success-case %d (%s) failed at position %u: %s\n",
                    i, tcs->tmpl_str, last_pos, bip32_template_error_to_string(error));
            exit(-1);
        }
        if( !templates_equal(&tcs->tmpl, &tmpl) ) {
            fprintf(stderr, "success-case %d (%s) failed: resulting template is not equal to template from test data\n", i, tcs->tmpl_str);
            fprintf(stderr, "\n");
            fprintf(stderr, "template from test data:\n");
            show_template(&tcs->tmpl);
            fprintf(stderr, "\n");
            fprintf(stderr, "template from parsing:\n");
            show_template(&tmpl);
            fprintf(stderr, "\n");
            exit(-1);
        }
        extract_path(&tmpl, test_path, &test_path_len, 0);
        if( !bip32_template_match(&tmpl, test_path, test_path_len) ) {
            fprintf(stderr, "success-case %d (%s) match failed\n", i, tcs->tmpl_str);
            show_template(&tmpl);
            exit(-1);
        }
        if( extract_path(&tmpl, test_path, &test_path_len, 1) ) {
            if( bip32_template_match(&tmpl, test_path, test_path_len) ) {
                fprintf(stderr, "success-case %d (%s) non-match matched\n", i, tcs->tmpl_str);
                show_template(&tmpl);
                exit(-1);
            }
        }
        test_path_len = BIP32_TEMPLATE_MAX_SECTIONS;
        if( bip32_template_parse_string(tcs->tmpl_str, BIP32_TEMPLATE_FORMAT_ONLYPATH, &tmpl_onlypath, 0, 0) ) {
            if( !bip32_template_to_path(&tmpl_onlypath, test_path, &test_path_len) ) {
                fprintf(stderr, "success-case %d (%s) template_to_path failed unexpectedly\n",
                        i, tcs->tmpl_str);
                show_template(&tmpl);
                exit(-1);
            }
        }
        else {
            if( bip32_template_to_path(&tmpl, test_path, &test_path_len) ) {
                fprintf(stderr, "success-case %d (%s) template_to_path succeeded unexpectedly\n",
                        i, tcs->tmpl_str);
                show_template(&tmpl);
                exit(-1);
            }
        }
    }

    for( i = 0; i < (int)(sizeof(testcase_errors)/sizeof(testcase_errors[0])); i++ ) {
        expected_error = testcase_errors[i].error;
        if( expected_error == BIP32_TEMPLATE_ERROR_RANGE_START_NEXT_TO_PREVIOUS ) {
            mode = BIP32_TEMPLATE_FORMAT_UNAMBIGOUS;
        }
        else {
            mode = BIP32_TEMPLATE_FORMAT_AMBIGOUS;
        }
        for( ii = 0; ii < testcase_errors[i].num_strings; ii++ ) {
            tmpl_str = testcase_errors[i].strings[ii];
            if( bip32_template_parse_string(tmpl_str, mode, &tmpl, &error, &last_pos) ) {
                fprintf(stderr, "error-case \"%s\" sample %d (\"%s\") succeeded at position %u\n",
                        bip32_template_error_to_string(expected_error), ii+1, tmpl_str, last_pos);
                exit(-1);
            }
            if( !strchr(tmpl_str, '[') && !strchr(tmpl_str, '*') ) {
                if( bip32_template_parse_string(
                            tmpl_str, BIP32_TEMPLATE_FORMAT_ONLYPATH,
                            &tmpl_onlypath, &error_onlypath, &last_pos_onlypath) )
                {
                    fprintf(stderr, "error-case \"%s\" sample %d (\"%s\") succeeded at position %u with onlypath flag\n",
                            bip32_template_error_to_string(expected_error), ii+1, tmpl_str, last_pos_onlypath);
                    exit(-1);
                }
                if( error_onlypath != error ) {
                    fprintf(stderr, "error-case \"%s\" sample %d (\"%s\") has different error with onlypath: \"%s\"\n",
                            bip32_template_error_to_string(expected_error), ii+1, tmpl_str, bip32_template_error_to_string(error_onlypath));
                    exit(-1);
                }
                if( last_pos != last_pos_onlypath ) {
                    fprintf(stderr, "error-case \"%s\" sample %d (\"%s\") has different error position with (%u) and without (%u) onlypath\n",
                            bip32_template_error_to_string(expected_error), ii+1, tmpl_str, last_pos_onlypath, last_pos);
                    exit(-1);
                }
            }

            if( error != expected_error ) {
                fprintf(stderr, "error-case \"%s\" sample %d (\"%s\") failed with unexpected error \"%s\"\n",
                        bip32_template_error_to_string(expected_error), ii+1, tmpl_str, bip32_template_error_to_string(error));
                exit(-1);
            }

            expected_pos = strlen(tmpl_str);
            if( expected_error == BIP32_TEMPLATE_ERROR_UNEXPECTED_FINISH ) {
                expected_pos++;
            }
            else if( expected_error == BIP32_TEMPLATE_ERROR_PATH_EMPTY ) {
                expected_pos++;
            }
            else if( expected_error == BIP32_TEMPLATE_ERROR_UNEXPECTED_SLASH ) {
                if( expected_pos > 1 && tmpl_str[expected_pos-2] != '/' ) {
                    expected_pos++;
                }
            }
            else if( expected_error == BIP32_TEMPLATE_ERROR_PATH_TOO_LONG ) {
                if( tmpl_str[expected_pos-1] == '\'' || tmpl_str[expected_pos-1] == 'h' ) {
                    expected_pos++;
                }
            }
            if( last_pos != expected_pos ) {
                fprintf(stderr, "error-case \"%s\" sample %d (\"%s\") failed at position %u, "
                                "but it should have failed at position \"%u\"\n",
                        bip32_template_error_to_string(expected_error), ii+1, tmpl_str, last_pos, expected_pos);
                exit(-1);
            }
        }
    }
}
