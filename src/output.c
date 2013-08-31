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
#include <stdarg.h>

#include "data.h"
#include "output.h"

static void xprintf(FILE* out, const char* format, ...)
#ifdef __GNUC__
__attribute__((format(printf,2,3)))
#endif
;

static void xprintf(FILE* out, const char* format, ...) {
  va_list args;

  va_start(args, format);

  if (-1 == vfprintf(out, format, args)) {
    perror("fprintf");
    exit(EX_IOERR);
  }

  va_end(args);
}

static void declare_globals(FILE*);
static void declare_predefinitions(FILE*);
static void declare_protocol_struct(FILE*);
static void declare_protocol_vtable(FILE*);
static void declare_protocol_methods(FILE*);
static void declare_element_ctors(FILE*);
void write_header(FILE* output) {
  xprintf(output,
          "/*\n"
          "  Auto-generated from %s by astrocol.\n"
          "  Do not edit this file!\n"
          " */\n"
          "#ifndef ASTROCOL_%s_H_\n"
          "#define ASTROCOL_%s_H_\n",
          input_filename, protocol_name, protocol_name);

  declare_predefinitions(output);
  fputs(definitions, output);
  declare_globals(output);
  declare_protocol_vtable(output);
  declare_protocol_struct(output);
  declare_protocol_methods(output);
  declare_element_ctors(output);

  xprintf(output, "#endif\n");
}

static void declare_predefinitions(FILE* out) {
  xprintf(out,
          "struct %s_s;\n"
          "typedef struct {\n"
          "  struct %s_s* first;\n"
          "  void (*oom)(void);\n"
          "} %s_context_t;\n",
          protocol_name, protocol_name, protocol_name);
}

static void declare_globals(FILE* out) {
  xprintf(out,
          "#ifndef %s_CONTEXT_T\n"
          "#define %s_CONTEXT_T %s_context_t\n"
          "#endif\n",
          protocol_name, protocol_name, protocol_name);
  xprintf(out,
          "extern %s_CONTEXT_T* %s_context;\n",
          protocol_name, protocol_name);
  xprintf(out,
          "%s_CONTEXT_T* %s_create_context(void);\n"
          "void %s_destroy_context(%s_CONTEXT_T*);\n",
          protocol_name, protocol_name,
          protocol_name, protocol_name);
}

static void declare_protocol_struct(FILE* out) {
  xprintf(out,
          "typedef struct %s_s {\n"
          "  /**\n"
          "   * The table of implementations for this instance.\n"
          "   * Don't use it except to test for undefined.\n"
          "   */\n"
          "  struct %s_vtable* astrocol_vtable;\n"
          "  /**\n"
          "   * The location within the input file of this instance.\n"
          "   * It is up to the implementation to track filenames if it needs\n"
          "   * to do so.\n"
          "   */\n"
          "  YYLTYPE where;\n"
          "  /** Used internally by astrocol. */\n"
          "  struct %s_s* astrocol_gc_next;\n"
          "} %s;\n",
          protocol_name,
          protocol_name,
          protocol_name,
          protocol_name);
}

static void declare_protocol_vtable(FILE* out) {
  method* meth;
  field* arg;

  xprintf(out, "typedef struct {\n");
  for (meth = methods; meth; meth = meth->next) {
    xprintf(out, "%s (*%s)(",
            meth->return_type, meth->name);
    if (meth->fields) {
      for (arg = meth->fields; arg; arg = arg->next)
        xprintf(out, "%s %s%s", arg->type, arg->name,
                arg->next? ", " : "");
    } else {
      xprintf(out, "void");
    }

    xprintf(out, ");\n");
  }

  xprintf(out, "} %s_vtable;\n", protocol_name);
}

static void declare_protocol_methods(FILE* out) {
  method* meth;
  field* arg;

  for (meth = methods; meth; meth = meth->next) {
    xprintf(out, "%s %s(%s*", meth->return_type, meth->name, protocol_name);

    for (arg = meth->fields; arg; arg = arg->next)
      xprintf(out, ", %s", arg->type);

    xprintf(out, ");\n");
  }
}

static void write_ctor_args(FILE* out, field* arg) {
  /* Skip alignment-only and implicit members */
  while (arg && (arg->name[0] == ':' || arg->name[0] == '_'))
    arg = arg->next;

  /* Base case: No members remain */
  if (!arg) return;

  /* Recursive case: Write earlier arguments, then this one */
  write_ctor_args(out, arg->next);
  xprintf(out, ", %s", arg->type);
}

static void declare_element_ctors(FILE* out) {
  element* elt;

  for (elt = elements; elt; elt = elt->next) {
    xprintf(out, "%s* %s(YYLTYPE", protocol_name, elt->name);

    write_ctor_args(out, elt->members);
    xprintf(out, ");\n");
  }
}
