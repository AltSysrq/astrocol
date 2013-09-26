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
#include <stdarg.h>
#include <errno.h>

#include <yaml.h>

#include "data.h"
#include "reader.h"

#define EXPECT(event, extype)                           \
  do {                                                  \
    if ((event).type != extype)                         \
      format_error("Expected " #extype, &(event));      \
  } while (0)

#define FORYMAP(parser,evt)                     \
  for (xyp_parse(&(evt), parser);               \
       not_end_of_mapping_or_delete(&(evt)) &&  \
       expect_is_scalar(&(evt));                \
       yaml_event_delete(&(evt)),               \
       xyp_parse(&(evt), parser))

static void format_error(const char*, const yaml_event_t*);

static inline int expect_is_scalar(yaml_event_t* evt) {
  EXPECT(*evt, YAML_SCALAR_EVENT);
  return 1;
}

static inline int not_end_of_mapping_or_delete(yaml_event_t* evt) {
  if (evt->type == YAML_MAPPING_END_EVENT) {
    yaml_event_delete(evt);
    return 0;
  } else {
    return 1;
  }
}

static void format_error(const char* message, const yaml_event_t* evt) {
  fprintf(stderr, "%s:%d:%d: %s\n",
          input_filename,
          /* Conventionally, line numbers are 1-based, but libyaml
           * gives them to us 0-based.
           */
          (int)evt->start_mark.line + 1,
          (int)evt->start_mark.column,
          message);
  exit(EX_DATAERR);
}

/*
  Like yaml_parser_parse(), but prints message and exits program if an error
  occurs. It also orders the parameters correctly (ie, destination first).
 */
static void xyp_parse(yaml_event_t* evt, yaml_parser_t* parser) {
  if (!yaml_parser_parse(parser, evt)) {
    /* The error members of the parser are supposedly private, but there
     * doesn't appear to be any API to access them otherwise.
     */
    fprintf(stderr,
            "Error reading %s, in context %s: %s\n",
            input_filename,
            parser->context,
            parser->problem);
    exit(EX_DATAERR);
  }
}

static void start_document(yaml_parser_t* parser) {
  yaml_event_t evt;
  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_STREAM_START_EVENT);
  yaml_event_delete(&evt);
  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_DOCUMENT_START_EVENT);
  yaml_event_delete(&evt);
  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_MAPPING_START_EVENT);
  yaml_event_delete(&evt);
}

static void end_document(yaml_parser_t* parser, yaml_event_t* prev_event) {
  EXPECT(*prev_event, YAML_MAPPING_END_EVENT);
  yaml_event_delete(prev_event);
  xyp_parse(prev_event, parser);
  EXPECT(*prev_event, YAML_DOCUMENT_END_EVENT);
  yaml_event_delete(prev_event);
  xyp_parse(prev_event, parser);
  EXPECT(*prev_event, YAML_STREAM_END_EVENT);
  yaml_event_delete(prev_event);
}

static void read_one_configuration_value(yaml_parser_t*, yaml_event_t*);
static void read_configuration(yaml_parser_t* parser, yaml_event_t* key) {
  yaml_event_t evt;

  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_MAPPING_START_EVENT);
  yaml_event_delete(&evt);

  FORYMAP(parser, evt) {
    read_one_configuration_value(parser, &evt);
  }
}

static void read_config_protocol_name(yaml_parser_t*);
static void read_config_header(yaml_parser_t*);
static void read_config_output(yaml_parser_t*);

static const struct {
  const char* name;
  void (*fun)(yaml_parser_t*);
} config_keys[] = {
  { "protocol_name", read_config_protocol_name },
  { "header", read_config_header },
  { "output", read_config_output },
  { NULL, NULL },
};

static void read_one_configuration_value(yaml_parser_t* parser,
                                         yaml_event_t* key) {
  unsigned i;
  char message[96];
  const char* value = (const char*)key->data.scalar.value;

  for (i = 0; config_keys[i].name; ++i) {
    if (0 == strcmp(value, config_keys[i].name)) {
      (*config_keys[i].fun)(parser);
      return;
    }
  }

  snprintf(message, sizeof(message), "Unknown config option: %s", value);
  format_error(message, key);
}

static void read_string_value(const char** dst,
                              yaml_parser_t* parser) {
  yaml_event_t evt;

  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_SCALAR_EVENT);

  *dst = xstrdup((const char*)evt.data.scalar.value);

  yaml_event_delete(&evt);
}

static void read_config_protocol_name(yaml_parser_t* parser) {
  read_string_value(&protocol_name, parser);
}

static void read_config_header(yaml_parser_t* parser) {
  read_string_value(&protocol_header_filename, parser);
}

static void read_config_output(yaml_parser_t* parser) {
  read_string_value(&protocol_impl_filename, parser);
}

static void read_definitions(yaml_parser_t* parser, yaml_event_t* key) {
  read_string_value(&definitions, parser);
}

static void read_prologue(yaml_parser_t* parser, yaml_event_t* key) {
  read_string_value(&prologue, parser);
}

static void read_epilogue(yaml_parser_t* parser, yaml_event_t* key) {
  read_string_value(&epilogue, parser);
}

static void read_protocol_method(yaml_parser_t*, yaml_event_t*);
static void read_protocol(yaml_parser_t* parser, yaml_event_t* ignored) {
  yaml_event_t evt;

  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_MAPPING_START_EVENT);
  yaml_event_delete(&evt);

  FORYMAP(parser, evt) {
    read_protocol_method(parser, &evt);
  }
}
static void read_protocol_method_decl(yaml_parser_t* parser,
                                      method* meth,
                                      yaml_event_t* key);
static void read_protocol_method(yaml_parser_t* parser,
                                 yaml_event_t* key) {
  char message[64];
  const char* name = (const char*)key->data.scalar.value;
  method* meth;
  yaml_event_t evt;

  /* Ensure no method with this name already exists */
  for (meth = methods; meth; meth = meth->next) {
    if (0 == strcmp(name, meth->name)) {
      snprintf(message, sizeof(message), "Method %s already defined", name);
      format_error(message, key);
    }
  }

  /* OK, create */
  meth = xmalloc(sizeof(method));
  meth->name = xstrdup(name);
  meth->return_type = "void";
  meth->default_impl.type = mit_undefined;
  meth->fields = NULL;
  meth->next = methods;
  meth->is_implicit = 0;
  methods = meth;

  /* Read method information */
  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_MAPPING_START_EVENT);
  yaml_event_delete(&evt);

  FORYMAP(parser, evt) {
    read_protocol_method_decl(parser, meth, &evt);
  }
}

static const struct {
  const char* key;
  method_impl_type value;
} method_impl_names[] = {
  { "recursive", mit_recursive },
  { "visit parent", mit_visit_parent },
  { "visits parent", mit_visit_parent },
  { "returns 0", mit_returns_0 },
  { "return 0", mit_returns_0 },
  { "returns 1", mit_returns_1 },
  { "return 1", mit_returns_1 },
  { "returns this", mit_returns_this },
  { "return this", mit_returns_this },
  { "does nothing", mit_does_nothing },
  { "do nothing", mit_does_nothing },
  { "undefined", mit_undefined },
  { "custom", mit_custom },
  { "graphviz", mit_graphviz },
  { NULL }
};

static void read_method_impl(method_impl* impl,
                             yaml_parser_t* parser,
                             const char* caller_name) {
  const char* impl_name;
  unsigned i;
  yaml_event_t evt;
  char message[64];

  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_SCALAR_EVENT);
  impl_name = (const char*)evt.data.scalar.value;

  for (i = 0; method_impl_names[i].key; ++i) {
    if (0 == strcmp(impl_name, method_impl_names[i].key)) {
      impl->type = method_impl_names[i].value;
      impl->implemented_by = caller_name;
      yaml_event_delete(&evt);

      return;
    }
  }

  snprintf(message, sizeof(message),
           "Unknown implementation type: %s", impl_name);
  format_error(message, &evt);
}

static void read_method_arg(yaml_parser_t*, method*, yaml_event_t*);
static void read_protocol_method_decl(yaml_parser_t* parser,
                                      method* meth,
                                      yaml_event_t* key) {
  const char* key_name = (const char*)key->data.scalar.value;

  if (0 == strcmp(key_name, "return"))
    read_string_value(&meth->return_type, parser);
  else if (0 == strcmp(key_name, "default"))
    read_method_impl(&meth->default_impl, parser, protocol_name);
  else
    read_method_arg(parser, meth, key);
}

static void read_method_arg(yaml_parser_t* parser,
                            method* meth,
                            yaml_event_t* key) {
  const char* key_name = (const char*)key->data.scalar.value;
  field* arg;
  char message[96];

  /* Ensure no such argument already exists */
  for (arg = meth->fields; arg; arg = arg->next) {
    if (0 == strcmp(key_name, arg->name)) {
      snprintf(message, sizeof(message),
               "Method %s already has an argument named %s",
               meth->name, key_name);
      format_error(message, key);
    }
  }

  /* OK, create */
  arg = xmalloc(sizeof(field));
  arg->name = xstrdup(key_name);
  read_string_value(&arg->type, parser);
  arg->next = meth->fields;
  meth->fields = arg;
}

static method_impl* get_default_implementations(void) {
  unsigned num_methods = count_methods(), i = 0;
  method_impl* impls = xmalloc(num_methods * sizeof(method_impl));
  method* meth;

  for (meth = methods; meth; meth = meth->next)
    memcpy(impls + i++, &meth->default_impl, sizeof(method_impl));

  return impls;
}

static void read_element_decl(yaml_parser_t* parser, element* elt,
                              yaml_event_t* key);
static void read_element(yaml_parser_t* parser, yaml_event_t* key) {
  const char* name = (const char*)key->data.scalar.value;
  element* elt;
  char message[64];
  yaml_event_t evt;
  field* padding;

  /* Ensure not already defined */
  for (elt = elements; elt; elt = elt->next) {
    if (0 == strcmp(name, elt->name)) {
      snprintf(message, sizeof(message),
               "Element %s already defined", name);
      format_error(message, key);
    }
  }

  if (0 == strcmp(protocol_name, name)) {
    format_error("Element name may not equal protocol name", key);
  }

  /* OK, create */
  elt = xmalloc(sizeof(element));
  memset(elt, 0, sizeof(element));
  elt->name = name = xstrdup(name);
  elt->next = elements;
  elt->implementations = get_default_implementations();
  elements = elt;

  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_MAPPING_START_EVENT);
  yaml_event_delete(&evt);

  FORYMAP(parser, evt) {
    read_element_decl(parser, elt, &evt);
  }

  /* Add padding to long alignment to ensure binary compatibility */
  padding = xmalloc(sizeof(field));
  padding->type = "long";
  padding->name = ":0";
  padding->next = elt->members;
  elt->members = padding;
}

static void extend_element(element*, yaml_event_t*);

static void read_element_extends(yaml_parser_t* parser, element* elt,
                                 yaml_event_t* key) {
  yaml_event_t evt;

  if (elt->members) {
    format_error("`extends` subsection must precede `fields`", key);
  }

  xyp_parse(&evt, parser);
  EXPECT(evt, YAML_SEQUENCE_START_EVENT);
  yaml_event_delete(&evt);

  for (xyp_parse(&evt, parser);
       evt.type != YAML_SEQUENCE_END_EVENT;
       yaml_event_delete(&evt), xyp_parse(&evt, parser)) {
    EXPECT(evt, YAML_SCALAR_EVENT);
    extend_element(elt, &evt);
  }

  yaml_event_delete(&evt);
}

static field* concatenate_fields(field* head, field* tail) {
  field* this;
  if (!head) return tail;

  this = xmalloc(sizeof(field));
  memcpy(this, head, sizeof(field));
  this->next = concatenate_fields(head->next, tail);
  return this;
}

static void extend_element(element* this, yaml_event_t* key) {
  element* that;
  const char* extname = (const char*)key->data.scalar.value;
  char message[64];
  unsigned i, num_methods = count_methods();

  if (0 == strcmp(extname, this->name)) {
    format_error("Element may not extend itself", key);
  }

  for (that = elements; that; that = that->next) {
    if (0 == strcmp(extname, that->name)) {
      this->members = concatenate_fields(that->members, this->members);

      /* Replace methods in this with non-default implementations from that. */
      for (i = 0; i < num_methods; ++i)
        if (protocol_name != that->implementations[i].implemented_by)
          memcpy(this->implementations + i,
                 that->implementations + i,
                 sizeof(method_impl));

      return;
    }
  }

  snprintf(message, sizeof(message),
           "No such element: %s", extname);
  format_error(message, key);
}

static void read_element_fields(yaml_parser_t* parser,
                                element* this,
                                yaml_event_t* key) {
  yaml_event_t nameevt;
  const char* name;
  field* f;
  char message[64];

  xyp_parse(&nameevt, parser);
  EXPECT(nameevt, YAML_MAPPING_START_EVENT);
  yaml_event_delete(&nameevt);

  FORYMAP(parser, nameevt) {
    name = (const char*)nameevt.data.scalar.value;
    /* Ensure not already used */
    for (f = this->members; f; f = f->next) {
      if (0 == strcmp(name, f->name)) {
        snprintf(message, sizeof(message),
                 "Field %s already defined in this element", name);
        format_error(message, &nameevt);
      }
    }

    /* OK, create and read type */
    f = xmalloc(sizeof(field));
    f->name = xstrdup(name);
    f->next = this->members;
    read_string_value(&f->type, parser);
    this->members = f;
  }
}

static void read_element_methods(yaml_parser_t* parser,
                                 element* this,
                                 yaml_event_t* key) {
  char message[64];
  const char* name;
  yaml_event_t nameevt;
  unsigned i;
  method* meth;

  xyp_parse(&nameevt, parser);
  EXPECT(nameevt, YAML_MAPPING_START_EVENT);
  yaml_event_delete(&nameevt);

  FORYMAP(parser, nameevt) {
    name = (const char*)nameevt.data.scalar.value;
    i = 0;
    for (meth = methods; meth && strcmp(name, meth->name); meth = meth->next)
      ++i;

    if (!meth) {
      snprintf(message, sizeof(message),
               "Method %s not defined for protocol", name);
      format_error(message, &nameevt);
    }

    read_method_impl(this->implementations + i, parser, this->name);
  }
}

static const struct {
  const char* name;
  void (*parse)(yaml_parser_t*, element*, yaml_event_t*);
} element_decls[] = {
  { "extends", read_element_extends },
  { "fields", read_element_fields },
  { "methods", read_element_methods },
  { NULL },
};

static void read_element_decl(yaml_parser_t* parser,
                              element* this,
                              yaml_event_t* key) {
  const char* name = (const char*)key->data.scalar.value;
  char message[64];
  unsigned i;

  for (i = 0; element_decls[i].name; ++i) {
    if (0 == strcmp(name, element_decls[i].name)) {
      (*element_decls[i].parse)(parser, this, key);
      return;
    }
  }

  snprintf(message, sizeof(message),
           "Unknown element subsection type: %s", name);
  format_error(message, key);
}

static const struct {
  const char* name;
  void (*parse)(yaml_parser_t*, yaml_event_t*);
} parsing_stages[] = {
  { "configuration", read_configuration },
  { "definitions", read_definitions },
  { "prologue", read_prologue },
  { "protocol", read_protocol },
  { "~epilogue", read_element },
  { "epilogue", read_epilogue },
  { NULL, NULL }
};

static int matches_section(unsigned* increment,
                           const char* target, const char* key) {
  if ('~' == target[0]) {
    *increment = 0;
    return !!strcmp(target + 1, key);
  } else {
    *increment = 1;
    return !strcmp(target, key);
  }
}

void read_input_file(yaml_parser_t* parser) {
  yaml_event_t evt;
  unsigned stage = 0, incr;
  char message[64];
  const char* section_name;
  int ok;

  start_document(parser);

  for (xyp_parse(&evt, parser);
       evt.type == YAML_SCALAR_EVENT;
       yaml_event_delete(&evt), xyp_parse(&evt, parser)) {
    section_name = (const char*)evt.data.scalar.value;
    ok = 0;
    while (!ok && parsing_stages[stage].name) {
      if (matches_section(&incr, parsing_stages[stage].name, section_name)) {
        (*parsing_stages[stage].parse)(parser, &evt);
        ok = 1;
        stage += incr;
      } else {
        ++stage;
      }
    }

    if (!ok) {
      snprintf(message, sizeof(message),
               "Unknown section type: %s", section_name);
      format_error(message, &evt);
    }
  }

  end_document(parser, &evt);
}
