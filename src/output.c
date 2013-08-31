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
#include <string.h>

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

static inline void on_each_elt(FILE* out, void (*f)(FILE*, element*)) {
  element* elt;
  for (elt = elements; elt; elt = elt->next)
    (*f)(out, elt);
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
  xprintf(out, "typedef struct %s_s %s;\n",
          protocol_name, protocol_name);
  xprintf(out,
          "typedef struct {\n"
          "  %s* first;\n"
          "  void (*oom)(void);\n"
          "} %s_context_t;\n",
          protocol_name, protocol_name);
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
          "struct %s_s {\n"
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
          "};\n",
          protocol_name,
          protocol_name,
          protocol_name);
}

static void write_args(FILE* out, field* arg, char implicit) {
  /* Skip alignment-only and implicit members */
  while (arg && (arg->name[0] == ':' || arg->name[0] == implicit))
    arg = arg->next;

  /* Base case: No members remain */
  if (!arg) return;

  /* Recursive case: Write earlier arguments, then this one */
  write_args(out, arg->next, implicit);
  xprintf(out, ", %s %s", arg->type, arg->name);
}

static void declare_protocol_vtable(FILE* out) {
  method* meth;

  xprintf(out, "typedef struct {\n");
  for (meth = methods; meth; meth = meth->next) {
    xprintf(out, "%s (*%s)(%s*",
            meth->return_type, meth->name, protocol_name);

    write_args(out, meth->fields, 0);

    xprintf(out, ");\n");
  }

  xprintf(out, "} %s_vtable;\n", protocol_name);
}

static void declare_protocol_methods(FILE* out) {
  method* meth;

  for (meth = methods; meth; meth = meth->next) {
    xprintf(out, "%s %s(%s*", meth->return_type, meth->name, protocol_name);

    write_args(out, meth->fields, 0);

    xprintf(out, ");\n");
  }
}

static void declare_element_ctors(FILE* out) {
  element* elt;

  for (elt = elements; elt; elt = elt->next) {
    xprintf(out, "%s* %s(YYLTYPE", protocol_name, elt->name);

    write_args(out, elt->members, '_');
    xprintf(out, ");\n");
  }
}

static void declare_element_types(FILE*);
static void declare_method_impls(FILE*);
static void define_element_vtables(FILE*);
static void define_element_types(FILE*);
void write_impl(FILE* out) {
  xprintf(out,
          "/*\n"
          "  Auto-generated from %s by astrocol.\n"
          "  Do not edit this file!\n"
          " */\n"
          "#ifdef HAVE_CONFIG_H\n"
          "#include <config.h>\n"
          "#endif\n"
          "#include \"%s\"\n"
          "%s\n",
          input_filename,
          protocol_header_filename,
          prologue);
  declare_element_types(out);
  declare_method_impls(out);
  define_element_vtables(out);
  define_element_types(out);
  fputs(epilogue, out);
}

static const char* get_implementor_name(method* meth,
                                        unsigned ix,
                                        element* elt) {
  /* If not "implemented by" the protocol, that field is always correct. */
  if (strcmp(protocol_name, elt->implementations[ix].implemented_by))
    return elt->implementations[ix].implemented_by;

  /* Each element actually implements methods provided by the protocol, unless
   * the protocol provides a custom default.
   */
  if (mit_custom == elt->implementations[ix].type)
    return protocol_name;
  else
    return elt->name;
}

static const char* get_implementation_linkage(unsigned ix, element* elt) {
  /* All implementations are static, other than custom, since that must be
   * linked against code in another file.
   */
  return mit_custom == elt->implementations[ix].type?
    "extern" : "static";
}

static void declare_element_method_impls(FILE* out, element* elt) {
  method* meth;
  unsigned i = 0;

  for (meth = methods; meth; meth = meth->next, ++i) {
    /* Only need to declare something if a version specific to this element
     * must be generated.
     */
    if (mit_undefined != elt->implementations[i].type &&
        !strcmp(elt->name, get_implementor_name(meth, i, elt))) {
      xprintf(out,
              "%s %s %s_%s(%s_t*",
              get_implementation_linkage(i, elt),
              meth->return_type,
              elt->name,
              meth->name,
              elt->name);
      write_args(out, meth->fields, 0);
      xprintf(out, ");\n");
    }
  }
}

static void declare_method_impls(FILE* out) {
  on_each_elt(out, declare_element_method_impls);
}

static void declare_element_types(FILE* out) {
  element* elt;

  for (elt = elements; elt; elt = elt->next)
    xprintf(out,
            "typedef struct %s_s %s_t;\n",
            elt->name, elt->name);
}

static void define_element_vtable(FILE* out, element* elt) {
  method* meth;
  unsigned ix = 0;
  method_impl impl;

  xprintf(out,
          "static const %s_vtable %s_vtable = {\n",
          protocol_name, elt->name);
  for (meth = methods; meth; meth = meth->next, ++ix) {
    impl = elt->implementations[ix];
    if (mit_undefined == impl.type) {
      xprintf(out, "  NULL,\n");
    } else {
      /* Explicitly cast to suppress warnings */
      xprintf(out,
              "  (%s (*)(%s*",
              meth->return_type,
              protocol_name);
      write_args(out, meth->fields, 0);
      xprintf(out, ")) %s_%s,\n",
              get_implementor_name(meth, ix, elt),
              meth->name);
    }
  }

  xprintf(out, "};\n");
}

static void define_element_vtables(FILE* out) {
  on_each_elt(out, define_element_vtable);
}

static void define_element_members(FILE* out, field* head) {
  if (!head) return;

  define_element_members(out, head->next);

  xprintf(out,
          "  %s %s;\n",
          head->type, head->name);
}

static void define_element_type(FILE* out, element* elt) {
  xprintf(out,
          "struct %s_s {\n"
          "  %s astrocol_protocol;\n",
          elt->name, protocol_name);
  define_element_members(out, elt->members);
  xprintf(out, "};\n");
}

static void define_element_types(FILE* out) {
  on_each_elt(out, define_element_type);
}
