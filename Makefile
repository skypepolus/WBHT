# BSD 3-Clause License
#
# Copyright (c) 2024, Young Ho Song
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
CC=gcc
CXX=g++
CFLAGS=-Iinc -fPIC -Ofast
LDFLAGS=-Wl,--defsym=malloc=.wbht.private.wbht_malloc,--defsym=calloc=.wbht.private.wbht_calloc,--defsym=free=.wbht.private.wbht_free,--defsym=realloc=.wbht.private.wbht_realloc,--defsym=aligned_alloc=.wbht.private.wbht_aligned_alloc,--defsym=memalign=.wbht.private.wbht_memalign,--defsym=posix_memalign=.wbht.private.wbht_posix_memalign,--defsym=pvalloc=.wbht.private.wbht_pvalloc,--defsym=reallocarray=.wbht.private.wbht_reallocarray,--defsym=reallocf=.wbht.private.wbht_reallocf,--defsym=valloc=.wbht.private.wbht_valloc
AR=ar

.PHONY: lib
lib: lib/libwbht.a lib/libwbht.so lib/libavlht.a lib/libavlht.so

lib/libwbht.so: lib/libwbht.a
	$(CC) -shared -Wl,--whole-archive $? -Wl,--no-whole-archive $(LDFLAGS) -o $@

#lib/libbtff.so: lib/libbtff.a
#	$(CC) -shared -Wl,--whole-archive $? -Wl,--no-whole-archive $(BTFF_LDFLAGS) -o $@

#lib/libbtff-sse.so: lib/libbtff-sse.a
#	$(CC) -shared -Wl,--whole-archive $? -Wl,--no-whole-archive $(BTFF_LDFLAGS) -o $@

lib/libwbht.a: src/wbht.o
	$(AR) rs $@ $?

lib/libavlht.so: src/avlht.o
	$(CC) $(CFLAGS) -DAVLHT -shared $? $(LDFLAGS) -o $@

lib/libavlht.a: src/avlht.o
	$(AR) rs $@ $?

src/avlht.o: src/wbht.c
	$(CC) $(CFLAGS) -DAVLHT -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	find src -name "*.[oa]" -exec rm -f {} \;
	find lib -name "*.so" -exec rm -f {} \;

.PHONY: dep
dep:
	$(CC) $(CFLAGS) -M src/*.c > .dep

ifeq (1, $(shell if [ -e .dep ]; then echo 1; fi))
include .dep
endif
