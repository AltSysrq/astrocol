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
#include <ctype.h>

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
static void declare_element_types(FILE*);
static void declare_element_ctors(FILE*);
static void declare_protocol_custom_defaults(FILE*);
static void declare_method_impls(FILE*);
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
  declare_element_types(output);
  declare_element_ctors(output);
  declare_protocol_custom_defaults(output);
  declare_method_impls(output);

  xprintf(output, "#endif\n");
}

static void declare_predefinitions(FILE* out) {
  xprintf(out, "typedef struct %s_s %s;\n",
          protocol_name, protocol_name);
  xprintf(out,
          "typedef struct {\n"
          "  %s* first, * last;\n"
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
          "  %s_vtable* vtable;\n"
          "  /**\n"
          "   * The location within the input file of this instance.\n"
          "   * It is up to the implementation to track filenames if it needs\n"
          "   * to do so.\n"
          "   */\n"
          "  YYLTYPE where;\n"
          "  /** Used internally by astrocol. */\n"
          "  struct %s_s* gc_next;\n"
          "  void (*dtor)(void*);\n"
          "  /**\n"
          "   * The unique parent of this instance, or NULL if this\n"
          "   * is a root. */\n"
          "  struct %s_s* parent;\n"
          "};\n",
          protocol_name,
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
    if (meth->is_implicit) continue;
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

static void define_protocol_vcalls(FILE*);
static void define_element_vtables(FILE*);
static void define_element_types(FILE*);
static void define_implementations(FILE*);
static void define_element_ctors(FILE*);
static void define_protocol_context(FILE*);
void write_impl(FILE* out) {
  xprintf(out,
          "/*\n"
          "  Auto-generated from %s by astrocol.\n"
          "  Do not edit this file!\n"
          " */\n"
          "#ifdef HAVE_CONFIG_H\n"
          "#include <config.h>\n"
          "#endif\n"
          "#include <string.h>\n"
          "#include <stdlib.h>\n"
          "#include <stdio.h>\n"
          "#include <assert.h>\n"
          "#include \"%s\"\n"
          "%s\n",
          input_filename,
          protocol_header_filename,
          prologue);
  xprintf(out,
          "static void* astrocol_malloc(size_t sz) {\n"
          "  void* ret = malloc(sz);\n"
          "  if (ret) return ret;\n"
          "  (*%s_context->oom)();\n"
          "  abort();\n"
          "}\n",
          protocol_name);
  define_protocol_vcalls(out);
  define_element_vtables(out);
  define_element_types(out);
  define_implementations(out);
  define_element_ctors(out);
  define_protocol_context(out);
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
              "extern %s %s_%s(%s_t*",
              meth->return_type,
              elt->name,
              meth->name,
              elt->name);
      write_args(out, meth->fields, 0);
      xprintf(out, ");\n");
    }
  }
}

static void declare_protocol_custom_defaults(FILE* out) {
  method* meth;

  for (meth = methods; meth; meth = meth->next) {
    if (mit_custom == meth->default_impl.type) {
      xprintf(out,
              "extern %s %s_%s(%s*",
              meth->return_type,
              protocol_name,
              meth->name,
              protocol_name);
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
          "  %s core;\n",
          elt->name, protocol_name);
  define_element_members(out, elt->members);
  xprintf(out, "};\n");
}

static void define_element_types(FILE* out) {
  on_each_elt(out, define_element_type);
}

static int skip_whitespace(const char** str) {
  while (isspace(**str))
    ++*str;

  return 1;
}

static int scan_str(const char** str, const char* target) {
  while (*target)
    if (**str == *target)
      ++*str, ++target;
    else
      return 0;

  return 1;
}

static int is_protocol_instance(const char* type) {
  return
    skip_whitespace(&type) &&
    scan_str(&type, protocol_name) &&
    skip_whitespace(&type) &&
    scan_str(&type, "*") &&
    skip_whitespace(&type) &&
    !*type;
}

static int is_void(const char* type) {
  return
    skip_whitespace(&type) &&
    scan_str(&type, "void") &&
    skip_whitespace(&type) &&
    !*type;
}

static void write_callsite_args(FILE* out, field* field) {
  if (!field) return;

  write_callsite_args(out, field->next);
  xprintf(out, ", %s", field->name);
}

static void gen_impl_recursive_for_member(FILE* out, method* meth,
                                          field* member) {
  if (!member) return;

  gen_impl_recursive_for_member(out, meth, member->next);

  if (is_protocol_instance(member->type)) {
    xprintf(out, "%s(this->%s", meth->name, member->name);
    write_callsite_args(out, meth->fields);
    xprintf(out, ");\n");
  }
}

static void gen_impl_recursive(FILE* out, method* meth, element* elt) {
  gen_impl_recursive_for_member(out, meth, elt->members);
}

static void gen_impl_visit_parent(FILE* out, method* meth, element* elt) {
  xprintf(out, "if (this->core.parent) ");

  if (!is_void(meth->return_type))
    xprintf(out, "return ");

  xprintf(out, "%s(this->core.parent", meth->name);
  write_callsite_args(out, meth->fields);
  xprintf(out, ");\n");

  if (!is_void(meth->return_type))
    /* Need to return *something*, zero is probably best */
    xprintf(out, "else return (%s)0;\n", meth->return_type);
}

static void gen_impl_returns_0(FILE* out, method* meth, element* elt) {
  xprintf(out, "return (%s)0;\n", meth->return_type);
}

static void gen_impl_returns_1(FILE* out, method* meth, element* elt) {
  xprintf(out, "return (%s)1;\n", meth->return_type);
}

static void gen_impl_returns_this(FILE* out, method* meth, element* elt) {
  xprintf(out, "return (%s)this;\n", meth->return_type);
}

static void gen_impl_does_nothing(FILE* out, method* meth, element* elt) {
}

static void (*const gen_impl_funs[])(FILE*, method*, element*) = {
  gen_impl_recursive,
  gen_impl_visit_parent,
  gen_impl_returns_0,
  gen_impl_returns_1,
  gen_impl_returns_this,
  gen_impl_does_nothing,
  NULL,
  NULL,
};

static void define_implementations_for_element(FILE* out, element* elt) {
  method* meth;
  unsigned ix = 0;

  for (meth = methods; meth; meth = meth->next, ++ix) {
    if (gen_impl_funs[elt->implementations[ix].type] &&
        !strcmp(elt->name, get_implementor_name(meth, ix, elt))) {
      xprintf(out,
              "%s %s_%s(%s_t* this",
              meth->return_type, elt->name, meth->name, elt->name);
      write_args(out, meth->fields, 0);
      xprintf(out, ") {\n");
      (*gen_impl_funs[elt->implementations[ix].type])(out, meth, elt);
      xprintf(out, "}\n");
    }
  }
}

static void define_implementations(FILE* out) {
  on_each_elt(out, define_implementations_for_element);
}

static void write_element_member_initialisers(FILE* out, field* member) {
  for (; member; member = member->next) {
    if (':' != member->name[0] && '_' != member->name[0]) {
      xprintf(out, "  this->%s = %s;\n", member->name, member->name);
      /* If the member is a non-NULL protocol instance, this is now its
       * parent. */
      if (is_protocol_instance(member->type)) {
        xprintf(out,
                "  if (%s) {\n"
                "    assert(!%s->parent);\n"
                "    %s->parent = (%s*)this;\n"
                "  }\n",
                member->name, member->name, member->name, protocol_name);
      }
    }
  }
}

static void define_element_ctor(FILE* out, element* elt) {
  xprintf(out, "%s* %s(YYLTYPE where", protocol_name, elt->name);
  write_args(out, elt->members, '_');
  xprintf(out, ") {\n");

  xprintf(out, "  %s_t* this = astrocol_malloc(sizeof(%s_t));\n",
          elt->name, elt->name);
  xprintf(out,
          "  memset(this, 0, sizeof(*this));\n"
          "  this->core.vtable = &%s_vtable;\n"
          "  this->core.where = where;\n",
          elt->name);

  write_element_member_initialisers(out, elt->members);

  /* Add to allocation chain */
  xprintf(out,
          "  if (%s_context->last) {\n"
          "    %s_context->last->gc_next = (%s*)this;\n"
          "    %s_context->last = (%s*)this;\n"
          "  } else {\n"
          "    %s_context->first = %s_context->last = (%s*)this;\n"
          "  }\n",
          protocol_name,
          protocol_name, protocol_name,
          protocol_name, protocol_name,
          protocol_name, protocol_name, protocol_name);

  xprintf(out, "  return (%s*)this;\n}\n", protocol_name);
}

static void define_element_ctors(FILE* out) {
  on_each_elt(out, define_element_ctor);
}

static void define_protocol_vcalls(FILE* out) {
  method* meth;

  for (meth = methods; meth; meth = meth->next) {
    if (meth->is_implicit)
      xprintf(out, "static ");

    xprintf(out, "%s %s(%s* this",
            meth->return_type, meth->name, protocol_name);
    write_args(out, meth->fields, 0);
    xprintf(out, ") {\n  ");

    if (!is_void(meth->return_type))
      xprintf(out, "return ");

    xprintf(out, "(*this->vtable->%s)(this", meth->name);
    write_callsite_args(out, meth->fields);
    xprintf(out, ");\n}\n");
  }
}

static void define_protocol_context(FILE* out) {
  xprintf(out,
          "static void astrocol_default_oom(void) {\n"
          "  fprintf(stderr, \"Astrocol: Memory exhausted.\");\n"
          "}\n");

  xprintf(out,
          "%s_CONTEXT_T* %s_context;\n",
          protocol_name, protocol_name);
  xprintf(out,
          "%s_CONTEXT_T* %s_create_context(void) {\n"
          "  %s_context_t* context = astrocol_malloc(sizeof(%s_CONTEXT_T));\n"
          "  if (!context) return NULL;\n"
          "  memset(context, 0, sizeof(%s_CONTEXT_T));\n"
          "  context->oom = astrocol_default_oom;\n"
          "  return (%s_CONTEXT_T*)context;\n"
          "}\n",
          protocol_name, protocol_name,
          protocol_name, protocol_name,

          protocol_name,

          protocol_name);

  xprintf(out,
          "void %s_destroy_context(%s_CONTEXT_T* context_) {\n"
          "  %s_context_t* context = (%s_context_t*)context_;\n"
          "  %s* item, * next;\n"
          "  for (item = context->first; item; item = next) {\n"
          /* TODO: Call destructors etc */
          "    next = item->gc_next;\n"
          "    free(item);\n"
          "  }\n"
          "}\n",
          protocol_name, protocol_name,
          protocol_name, protocol_name,
          protocol_name);
}
