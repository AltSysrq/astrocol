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

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_SYSEXITS_H
#include <sysexits.h>
#else
#define EX_USAGE 64
#define EX_DATAERR 65
#define EX_NOINPUT 66
#define EX_OSERR 71
#define EX_IOERR 74
#endif

#include <getopt.h>

#include <yaml.h>

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

const char* input_filename;
const char* protocol_header_filename;
const char* protocol_impl_filename;
const char* protocol_name;
const char* prologue = "";
const char* definitions = "";
const char* epilogue = "";

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
} method;

method* methods;

typedef struct element_s {
  const char* name;
  field* members;
  method_impl* implementations;
  struct element_s* next;
} element;

element* elements;

static inline void* xmalloc(size_t sz) {
  void* ret = malloc(sz);
  if (!ret) {
    fprintf(stderr, "Memory exhausted");
    abort();
  }

  return ret;
}

static void format_error(const char* message, const yaml_event_t* evt) {
  fprintf(stderr, "%s:%d:%d: %s\n",
          input_filename,
          (int)evt->start_mark.line,
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

  *dst = strdup((const char*)evt.data.scalar.value);

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
  meth->name = strdup(name);
  meth->return_type = "void";
  meth->default_impl.type = mit_undefined;
  meth->fields = NULL;
  meth->next = methods;
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
  { "returns 0", mit_returns_0 },
  { "returns 1", mit_returns_1 },
  { "returns this", mit_returns_this },
  { "does nothing", mit_does_nothing },
  { "undefined", mit_undefined },
  { "custom", mit_custom },
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

  for (i = 0; ; ++i) {
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
  arg->name = strdup(key_name);
  read_string_value(&arg->type, parser);
  arg->next = meth->fields;
  meth->fields = arg;
}

static const struct {
  const char* name;
  void (*parse)(yaml_parser_t*, yaml_event_t*);
} parsing_stages[] = {
  { "configuration", read_configuration },
  { "definitions", read_definitions },
  { "prologue", read_prologue },
  { "protocol", read_protocol },
  { "epilogue", read_epilogue },
  { NULL, NULL }
};

static void read_input_file(yaml_parser_t* parser) {
  yaml_event_t evt;
  unsigned stage = 0;
  char message[64];
  const char* section_name;
  int ok;

  start_document(parser);

  /* TODO: Elements */
  for (xyp_parse(&evt, parser);
       evt.type == YAML_SCALAR_EVENT;
       yaml_event_delete(&evt), xyp_parse(&evt, parser)) {
    section_name = (const char*)evt.data.scalar.value;
    ok = 0;
    while (!ok && parsing_stages[stage].name) {
      if (0 == strcmp(section_name, parsing_stages[stage].name)) {
        ok = 1;
        (*parsing_stages[stage].parse)(parser, &evt);
      }

      ++stage;
    }

    if (!ok) {
      snprintf(message, sizeof(message),
               "Unknown section type: %s", section_name);
      format_error(message, &evt);
    }
  }

  end_document(parser, &evt);
}

int main(void) {
  yaml_parser_t parser;

  /* TODO: Command-line arguments, reasonable default config */
  protocol_name = "foo";

  yaml_parser_initialize(&parser);
  yaml_parser_set_input_file(&parser, stdin);
  read_input_file(&parser);
  yaml_parser_delete(&parser);

  return 0;
}
