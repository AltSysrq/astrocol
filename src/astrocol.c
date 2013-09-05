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

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <yaml.h>

#include "reader.h"
#include "data.h"
#include "output.h"

static void load_defaults(void);
static void read_file(FILE*);
static void do_to_file(void (*)(FILE*), const char*, const char*);

int main(int argc, char** argv) {
  /* Just check for usage the simple way, since we can only be given one
   * argument and take no command-line options.
   */
  if (2 != argc ||
      !strcmp(argv[1], "-h") ||
      !strcmp(argv[1], "-help") ||
      !strcmp(argv[1], "--help") ||
      !strcmp(argv[1], "-?")) {
    printf("Usage: %s <infile>\n", argv[0]);
    return 1 == argc? 0 : EX_USAGE;
  }

  input_filename = argv[1];
  load_defaults();

  do_to_file(read_file, input_filename, "r");

  do_to_file(write_header, protocol_header_filename, "w");
  do_to_file(write_impl, protocol_impl_filename, "w");

  return 0;
}

static void load_defaults(void) {
  unsigned last_period = strlen(input_filename), i;
  char* str;
  method* meth;
  for (i = last_period; i; --i) {
    if (input_filename[i] == '.') {
      last_period = i;
      break;
    }
  }

  protocol_name = str = xmalloc(last_period + 1);
  memcpy(str, input_filename, last_period);
  str[last_period] = 0;

  protocol_header_filename = str = xmalloc(last_period + 3);
  memcpy(str, input_filename, last_period);
  str[last_period+0] = '.';
  str[last_period+1] = 'h';
  str[last_period+2] = 0;

  protocol_impl_filename = str = xmalloc(last_period + 3);
  memcpy(str, input_filename, last_period);
  str[last_period+0] = '.';
  str[last_period+1] = 'c';
  str[last_period+2] = 0;

  meth = xmalloc(sizeof(method));
  meth->name = "ctor";
  meth->return_type = "void";
  meth->default_impl.type = mit_undefined;
  meth->fields = NULL;
  meth->next = methods;
  meth->is_implicit = 1;
  methods = meth;

  meth = xmalloc(sizeof(method));
  meth->name = "dtor";
  meth->return_type = "void";
  meth->default_impl.type = mit_undefined;
  meth->fields = NULL;
  meth->next = methods;
  meth->is_implicit = 1;
  methods = meth;
}

static void read_file(FILE* in) {
  yaml_parser_t parser;

  yaml_parser_initialize(&parser);
  yaml_parser_set_input_file(&parser, in);
  read_input_file(&parser);
  yaml_parser_delete(&parser);
}

static void do_to_file(void (*proc)(FILE*), const char* fn, const char* mode) {
  FILE* f = fopen(fn, mode);
  if (!f) {
    fprintf(stderr, "Unable to open %s: %s\n", fn, strerror(errno));
    exit(EX_OSERR);
  }

  (*proc)(f);

  fclose(f);
}
