Astrocol
========

Astrocol (AST pROtoCOL) automates the tedium of generating structures,
redundant-but-different recursive functions, and trivial constructors needed to
build an Abstract Syntax Tree with Bison and C. This allows you to focus solely
on the real logic of your parser, while also ensuring that your AST has a
simple, uniform structure and that no memory is leaked.

Core Concepts
-------------

### Protocols

A *protocol* defines a single C type; it is typically the only type you expose
directly to your Bison code. Each instance of a protocol directly exposes
exactly three things:

- The location (as reported by Bison) where it was constructed.

- An *element table* which identifies the method implementations used for that
  particular instance.

- A "parent" instance, which reflects the one protocol instance that contains
  this instance, or NULL if this is a root instance.

### Methods

A protocol declares one or more *methods*, which are C function types
representing general operations that can be performed on each instance. Thus, a
protocol is rather similar to a Java interface, except that most methods will
be implemented by default for you.

Each method defines a default implicit implementation, which may be any
described in the Automatic Implementations section.

Astrocol reserves two special method names, "ctor" and "dtor", whose semantics
are described in the Element Construction section. One typically should avoid
defining methods whose names conflict with other global symbols, such as
"free".

### Elements

One typically has two or more *elements* belonging to a protocol. Every
element, represented by a C struct beginning with the protocol struct, defines
zero or more *fields*, which are added to the struct. Additionally, a
constructor function (with the same name as the element) is implicitly
generated, which takes as arguments and assigns all fields which are not
annotated as "internal". Non-internal fields are generally pointers to protocol
instances, which are managed automatically by Astrocol.

Each element also defines its method implementations; these can either be an
automatic implementation (see below) or a custom implementation, which must be
defined in another source file (ie, the code is not embedded into the input
file). Methods not explicitly defined by an element instead use the default
implicit implementation given by the protocol.

An element may *extend* another element. When this is done, the new element
starts off with a copy of the extended element's fields and methods instead of
empty lists. While this is akin to subclassing in object systems, it is
primarily useful for sharing method implementations rather than
substitution. It is legal for an element to extend more than one other element,
but the effects are undefined if more than one of the extended elements defines
fields and custom method implementations. A pointer to an element is always a
valid pointer to the first element it extends.

### Element Construction

Element construction is performed as follows:

- Memory for the element structure is allocated and initialised to zeroes.

- The embedded protocol instance, as well as all non-internal fields, are
  initialised according to the element type and the arguments to the
  constructor function.

- The "ctor" method on the element is called, if it exists. "ctor" is a method
  which takes no extra arguments and returns void. It defaults to "undefined".

When an element is to be destroyed, the following steps are taken:

- The "dtor" method on the element is called, if it exists. "dtor" is a method
  which takes no extra arguments and returns void. It defaults to
  "undefined". The destructor must not do anything with pointers to other
  protocol instances, since they are not guaranteed to be valid at this time.

- The memory associated with the element is released.

### Automatic Implementations

In general, few method implementations in an AST protocol do anything
interesting --- doing nothing, or recursing to child elements, are by far the
most common implementations. Therefore, method implementations are usually
selected from a the below list of automatic implementations, so that only the
truly interesting methods must be implemented manually.

- `recursive` --- The method being invoked is called with the same arguments on
  each field whose type is a non-NULL protocol instance which defines the
  method, in the order the fields are defined. Any return value is
  discarded. The generated implementation does not explicitly return, so this
  is only meaningful on void methods.

- `visit parent` --- The method being invoked is called on the parent instance
  with the same arguments. If the method is non-void, the parent's return value
  is also the generated implementation's return value. If there is no parent,
  the implementation either does nothing or returns 0, depending on whether the
  return type is void.

- `return 0` --- The generated implementation is simply `return 0;`.

- `return 1` --- The generated implementation is simply `return 1;`.

- `return this` --- The generated implementation is simply `return this;`.

- `does nothing` --- The generated implementation is empty. Only appropriate
  for void methods.

- `undefined` --- There is no generated implementation; it is set to NULL in
  the protocol table. Client code must be prepared for this condition and
  explicitly check for it before trying to call methods that may be undefined,
  unless it is intended to be logically impossible for an undefined method to
  be invoked.

- `custom` --- There is no implementation; instead, an external implementation
  is declared and used in the protocol table, and the protocol header shall
  contain a macro to define the actual implementation in another source
  file. This may be used as a default implementation, which may be defined in
  source by using the protocol name in place of an element name, though the
  usefulness of such a default implementation is questionable.

File Format
-----------
Astrocol takes a YAML document as input. The top-level element is a mapping
whose elements must come in a specific order. Each element defines a section;
the sections, in order, are:

- configuration (optional)
- definitions (optional)
- prologue (optional)
- protocol
- element
- epilogue (optional)

Note that there may be any number of "element" sections; each element section
is identified by the name of the element it defines.

### Configuration section
The configuration section is a mapping underneath a key named
"configuration". Astrocol tries to have reasonable defaults for all
configuration options, but they may be overridden here.

- `protocol_name` --- The name of the protocol struct (ie, a C type). By
  default, this is the base name of the input file.

- `header` --- The name of the file to which to write a C header for use by
  both clients and custom implementations. By default, it is the name of the
  input file with the extension replaced with "h".

- `output` --- The name of the file to which to write the auto-generated C
  source for the protocol. By default, it is the name of the input file with
  the extension replaced with "c".

### Definitions section
The contents of the definitions section, identified by the key "definitions",
must be a string value. This string is inserted at the top of the generated
header file (which is implicitly included by the implementation output), and so
allows to make definitions common to both external and internal code.

### Prologue section
The contents of the prologue section, identified by the key "prologue", must be
a string value. It is inserted at the top of the implementation output file,
immediately after the implicit prologue of that file, but before any actual
auto-generated code. The prologue will effectively follow the definitions
section.

### Protocol section
The protocol section, identified by the key "protocol", is a mapping which
defines the methods the protocol specifies.

Each method is keyed on its name, and contains another mapping. Each element of
the contained mapping is one of the following:

- Key `return` --- Indicates the return type of the method. A method with no
  specified return type returns void. The return type must be a "simple" C
  type; that is, given a type `X`, the statement `X x;` must declare `x` to be
  a variable of type `X`. An example of a type that fails this requirement is a
  literal function pointer (such as `void (*)(int)`); use typedefs in the
  definitions section to work around this limitation.

- Key `default` --- Specifies the default automatic implementation type. If not
  given, "undefined" is implied.

- Anything else --- Defines an argument whose name is the key of the element
  and whose type is the value. The same "simple type" restriction as for the
  `return` key also applies to this type. Arguments are added to the protocol
  in the order they are specified.

### Element sections
An element section is identified simply by a key whose value is the name of the
element (thus one cannot name an element "epilogue"). An element may contain
any number of the subsections defined below; each is identified by a key named
after the subsection type.

#### Subsection `extends`
Contains a list. Lists names of zero or more other elements defined earlier in
the file. This element will start off as the concatenation of those elements,
rather than empty. The `extends` subsection *must* precede the `fields` and
`methods` subsections.

#### Subsection `fields`
Contains a mapping. Each element is treated much the same as an argument
declaration from the protocol section, except that it is appended to the fields
of the element. Elements are layed out in the structure in the precise order in
which they were defined. Fields whose names begin with an underscore are
internal --- they do not get passed to the constructor function, instead being
merely initialised to zeroes. A field is of "protocol type" if it is a pointer
to the protocol type (ie, matching `^\s*PROT\s*\*\s*$`, where `PROT` is the
configuation value `protocol_name`).

#### Subsection `methods`
Contains a mapping. Each key names a method of the protocol; each value
indicates the implementation type to generate. Later pairs override the effects
of earlier ones, which is mainly useful for inheritance.

### Epilogue section
The contents of the epilogue section, identified by the key "epilogue", must be
a string value. This string is inserted at the bottom of the output
implementation file, after all auto-generated code.

C Interface
-----------

### Types

#### YYLTYPE
The data type to be used for storing location information. This *must* be
defined by the application, typically in the definitions section or by the
Bison-generated header.

#### PROTOCOL_CONTEXT_T
A structure type encapsulating the data pertaining to a protocol's "context";
"PROTOCOL" is the exact string value of the `protocol_name` configuration, so
the real name is something like `example_CONTEXT_T`. This identifier is assumed
o be a preprocessor definition, and will not behave correctly if it is not.

This structure *must* begin with a member of type `PROTOCOL_context_t`. If user
code does not define its own `PROTOCOL_CONTEXT_T`, it is implicitly defined to
`PROTOCOL_context_t`.

#### PROTOCOL
Each protocol creates a typedefedd structure with the same name as the
protocol. The majority of interactions are done through pointers to this type,
and all elements begin with such value.

The application-usable fields within this structure are as follows. All are
read-only application code.
- `vtable` --- Struct with function pointers for each method. User code should
  only use this if it needs to test for the existence of a method
  implementation.  Do not call the values directly; instead, use the global
  functions named after those methods.
- `begin`, `end` --- The start and end locations where this instance was defined.
- `parent` --- The logical parente of this instance. It is set when an elmeent
  is instantiated with this instance as a value of one of its non-internal
  protocol-type fields.

#### ELEMENT_t
Each element creates a typedefed structure whose name is the element name with
the suffix "_t" attached. Application code providing custom method
implementations typically is all that interacts with this directly. It begins
with a field named "core", which is of type PROTOCOL. The rest of the fields
are those define by the element and the elements it extends.

### Context Management
Each protocol has a "curent context", represented by the global variable
`PROTOCOL_context`, of type `PROTOCOL_CONTEXT_T*`. Astrocol only uses context
to encapsulate memory management, though application code may wish to store
additional data here.

A new context can be created by calling the function
`PROTOCOL_create_context(void)`, which either returns a fresh context object or
NULL if memory is unavailable. This does not set the current context; control
of the current context is delegated entirely to the application.

Once the current context has been set, new elements may be created by calling
their constructor functions. Each element is considered a member of what was
the current context when it was constructed; hierarchical relationships (ie,
the `parent` field) do not play into memory management. This makes Astrocol
safe to use for, eg, Bison-generated GLR parsers, which may create multiple
versions of a parse tree while keeping only one visible in the end.

When you are done with a context, it may be destroyed by passing it to
`PROTOCOL_destroy_context()`. This call not only frees the context object, but
also destroys all elements belonging to the context, in reverse the order they
were allocated. Note that the application may need to manually clean up
additional memory to which any data it added to the context points before
calling this function.

### Protocol
There are no functions to directly manipulate protocol objects, per se. Each
non-implicit method has one global function of the same name and return type,
which takes as arguments a protocol object followed by any arguments declared
for the method. This function will automatically delegate to the real
implementation of the method.

Application code should only ever call methods via the global functinos, rather
than dereferencing the vtable directly. Accessing the vtable to test whether a
method is defined (but not calling it this way) is sometimes acceptable, though
in general a correctly-designed program will never need to do this.

Custom default implementations of protocol methods are expected to be named
`PROTOCOL_METHOD`, and have the same signature as the global method functions.

### Elements
New instances of a particular element type may be constructed by calling a
global function of the same name. This function takes as arguments two
`YYLTYPE`s indicating where the element is defined, followed by values for all
non-internal fields. The constructor functions return a pointer to a protocol
type rather than the element type (though they can be converted back if need
be).

Each method implementation provided by an element is defined in a fuction named
`ELEMENT_METHOD`, whose signature is identical to the global method function,
except that the first argument is a pointer to the element type rather than the
protocol type.

### Memory Management
As described earlier, all element instances allocated are bound to a specific
protocol context, and destroyed when the associated context is destroyed.

It is also possible to allocate arbitrary memory to be associated with the
context. The functions `PROTOCOL_malloc` and `PROTOCOL_strdup` function like
their standard C counterparts, but the memory is automatically freed when the
context is. `PROTOCOL_dalloc` takes an additional argument, of type
`void (*)(void*)`, which is invoked on the memory immediately before it is
freed (ie, it specifies a destructor function).

The memory allocation functions, other than `PROTOCOL_create_context`, do not
return `NULL` if allocation fails. Instead, they invoke the out-of-memory
handler set in the context (type `void (*)(void)`). The default OOM handler
simply prints an error message and aborts the process. It is safe for the OOM
handler to `longjmp()` out of the callback and into user code, where the
protocol context in question may be freed.
