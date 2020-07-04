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
#
all: test

CFLAGS=-Wall -Wextra -pedantic

test/test_data.h: test/test_data.json test/gentest.py
	test/gentest.py $< > $@

test/test: test/test.c bip32template.c test/test_data.h
	$(CC) $(CFLAGS) \
	    -DBIP32_TEMPLATE_MAX_SECTIONS=3 -DBIP32_TEMPLATE_MAX_RANGES_PER_SECTION=4 \
	    -o $@ test/test.c bip32template.c

test: test/test
	test/test

clean:
	$(RM) test/test bip32template.o test/test_data.h

.PHONY: all test clean
