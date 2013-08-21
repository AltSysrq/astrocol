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
are described in the Element Construction section. Furthermore, the method name
"free" is forbidden.

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
fields.

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

See the Memory Management subsection of the C Interface section for further
details.

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

- `recursive_fold` --- Defined with one parameter, which is a base name for a
  pair of functions. The implementation allocates a local instance ("the
  accumulator") of the return value, then calls a function named `BASE_init`
  (where `BASE` is the base name parameter) with a pointer to that value. It
  then recurses to all children as per `recursive`, except that the return
  values are passed to the second argument of `BASE_fold`, with a pointer to
  the accumulator as the first argument. The final value of the accumulator is
  returned.

- `visit_parent` --- The method being invoked is called on the parent instance
  with the same arguments. If the method is non-void, the parent's return value
  is also the generated implementation's return value.

- `return_0` --- The generated implementation is simply `return 0;`.

- `return_1` --- The generated implementation is simply `return 1;`.

- `return_this` --- The generated implementation is simply `return this;`.

- `does_nothing` --- The generated implementation is empty. Only appropriate
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
