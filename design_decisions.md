# Architectural Decisions

## Core library is not Arc-specific

### Context

Arc Core was originally created as the foundational library used by the Arc compiler.

As the project evolved, the abstractions in Arc Core, such as allocators, iterators, ranges, sentinels, and containers, became increasingly general-purpose and independent from the Arc language itself.

These abstractions may also be useful in other projects, especially Angry Rocket Engine.

Because of this, Arc Core should not unnecessarily depend on compiler-specific concepts or assume that its only consumer is the Arc compiler.

### Decision

Public Core API names will not use an `arc_` prefix solely because the library is currently developed inside the Arc project.

For example:

```c
array_create(...)
iterator_clone(...)
allocator_alloc(...)
range_init(...)
```