#!/usr/bin/env python3
#
# Copyright 2020 Dmitry Petukhov https://github.com/dgpv
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

import sys
import json

if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.stderr.write(f"usage: {sys.argv[0]} /path/to/test_data.json\n")
        sys.exit(-1)

    with open(sys.argv[1]) as f:
        test_data = json.load(f)

    for state in test_data.keys():
        if state == 'normal_finish':
            print("testcase_success_t testcase_success[] = {")
            for i, case_data in enumerate(test_data[state]):
                tmpl_str, tmpl_data_str = case_data
                print('{', f'"{tmpl_str}",', '{', end='')
                tmpl = json.loads(tmpl_data_str)
                print(f"{len(tmpl)},", "{", end='')
                sections = []
                for section in tmpl:
                    s = "{"
                    s += f"{len(section)}, "
                    s += "{"
                    s += ", ".join(f"{'{'} {r[0]}, {r[1]} {'}'}"
                                   for r in section)
                    s += "}"
                    s += "}"
                    sections.append(s)
                print(", ".join(sections), end='')
                print('}}}', end='')
                if i+1 < len(test_data[state]):
                    print(",")
            print('\n};')
        else:
            vals = ",\n".join(f'"{val}"' for val in test_data[state])
            print(f"const char* testcase_{state}[] = {'{'} {vals} {'}'};")

    print("struct { bip32_template_error_t error; int num_strings; "
          "const char** strings; } testcase_errors[] = {")

    error_states = list(test_data.keys())

    for i, state in enumerate(error_states):
        if state == 'normal_finish':
            continue

        print('{', f'BIP32_TEMPLATE_{state.upper()}',
              ",", len(test_data[state]), ",",
              f'testcase_{state}', '}', end='')
        if i+1 < len(error_states):
            print(",")
        else:
            print("")

    print("};")
