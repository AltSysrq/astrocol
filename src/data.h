/*
Copyright (c) 2013 Jason Lingle
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the author nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
 */

#ifndef DATA_H_
#define DATA_H_

extern const char* input_filename;
extern const char* protocol_header_filename;
extern const char* protocol_impl_filename;
extern const char* protocol_name;
extern const char* prologue;
extern const char* definitions;
extern const char* epilogue;

typedef struct field_s {
  const char* type;
  const char* name;
  struct field_s* next;
} field;

typedef enum {
  mit_recursive = 0,
  mit_visit_parent,
  mit_returns_0,
  mit_returns_1,
  mit_returns_this,
  mit_does_nothing,
  mit_undefined,
  mit_custom
} method_impl_type;

typedef struct {
  method_impl_type type;
  const char* implemented_by;
} method_impl;

typedef struct method_s {
  const char* name;
  const char* return_type;
  method_impl default_impl;
  field* fields;
  struct method_s* next;
  int is_implicit;
} method;

extern method* methods;

static inline unsigned count_methods(void) {
  unsigned cnt = 0;
  method* meth = methods;

  while (meth) {
    ++cnt;
    meth = meth->next;
  }

  return cnt;
}

typedef struct element_s {
  const char* name;
  field* members;
  method_impl* implementations;
  struct element_s* next;
} element;

extern element* elements;

void* xmalloc(size_t);

#endif /* DATA_H_ */
