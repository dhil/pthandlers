# Pthandlers: affine effect handling via POSIX threads

This C library provides a proof-of-concept encoding of affine effect
handlers using POSIX threads (pthreads).

## Encoding affine effect handlers with pthreads

The encoding strategy utilises the insights of Kumar *et al.* (1998),
who implement one-shot delimited continuations using threads, as the
basis for simulating the implementation of effect handlers in
Multicore OCaml (Dolan *et al.* (2015)), which uses a variation of
segmented stacks á la Bruggeman *et al.* (1996).

### Simulating delimited control with threads

The key insight from Kumar *et al.* (1998) is that each thread has its
own stack and by establishing a parent-child relationship between the
thread-stacks, we can simulate delimited control. For all intents and
purposes we can treat thread and stack as synonyms as the simulation
do not take advantage of the concurrency or parallel aspects of
(p)threads. In fact the control flow of any encoded program is
intended to be sequential and entirely deterministic. As depicted in
Figure 1 below, to run some computation `f` under some delimiter `del`
the idea is to install the delimiter on top of the current
thread/stack, `s1`, and subsequently spawn a new thread/stack, `s2`,
to execute the computation. Conceptually, in terms of single-threaded
programs, the stack pointer (`sp`) moves to the top of the new stack
`s2`.

```
  s1         s2
+-----+    +-----+
|     |    |     |
|     |    | f() |
| del |<---|     |<- stack pointer (sp)
+-----+    +-----+
---------------------------------------
      Fig 1. Execution stacks
```

After spawning the new thread the parent thread `s1` blocks and awaits
either the completion of `s2` or a continuation capture within
`s2`. Capturing the continuation within `f` amounts to synchronising
the threads `s2` and `s1`, as the thread `s2` has to unblock `s1` and
subsequently block itself to await being resumed. As depicted in Figure 2
below, a continuation capture conceptually replaces the delimiter
`del` with a reference, `k`, to the child stack, `s2`, and sets the
stack pointer to point to the top of the parent stack,
`s1`. Similarly, to resume `s2`, the thread `s1` simply has to unblock
`s2` and subsequently block itself --- whether `del` gets reinstalled
depends on the exact nature of the control operator of
choice. Following the resumption, the stack pointer is, conceptually,
set to point to the top of the child stack `s2`.

```
After stack suspension
  s1             s2
+-----+        +-----+
|     |        |     |
|     |        | f() |
|  k  |------->|     |
|     |<- sp   +-----+
+-----+

After stack resumption
  s1         s2
+-----+    +-----+
|     |    |     |
|     |    | f() |
| del |<---|     |<- stack pointer (sp)
+-----+    +-----+
---------------------------------------------
   Fig 2. Suspending and resuming stacks
```

Stack synchronisation can readily be realised by using mutexes and
condition variables.

### Simulating effect handlers

The novelty of this encoding does not lie in how delimited control is
simulated, but rather, how the technique outlined in the previous
section is adapted to encode the structured interface of effect
handlers.

TODO...

## Building

The `Makefile` provides rules for building static and shared
variations of this library, simply type

```shell
$ make lib
```

This rule assembles the static library `libpthandlers.a` and the
shared library `libpthandlers.so` and outputs both in the `lib/`
directory. To build either library variant alone invoke `make static`
or `make shared` respectively.

### Examples

This repository contains some example programs. To build them all
simply type `make examples`. Alternatively, they can be built individually

```shell
$ make examples/{coop,divzero,dobedobe,state}
```

The resulting binaries are placed in the root of this repository.

## Caveats

This encoding relies heavily on tail-call optimisation (i.e. gcc's
`-foptimize-sibling-calls`), which seems to be rather fragile, or
unreliable, in C compilers. In addition this implementation also
depends on one or more flags from the `-O1` optimisation bundle that I
have yet to discover. Currently, without passing `-O1` it appears that
`gcc` (version 7.5.0) fails to tail-call optimise some functions.

## Future work

The current implementation only supports unary deep handlers. A
natural extension would be to support unary shallow handlers. Going
even further, it would be interesting to encode both deep and shallow
variations of multi-handlers.

## Acknowledgements

This work was done whilst I was being supported by
[EPSRC](https://www.epsrc.ac.uk/) grant
[EP/L01503X/1](http://pervasiveparallelism.inf.ed.ac.uk) (EPSRC Centre
for Doctoral Training in Pervasive Parallelism) and by ERC
Consolidator Grant
[Skye](https://homepages.inf.ed.ac.uk/jcheney/group/skye.html) (grant
number 682315).

## References

* Sanjeev Kumar, Carl Bruggeman, and R. Kent Dybvig. 1998. Threads Yield Continuations. Higher-Order and Symbolic Computation 10, 223–236 (1998).  
  [(DOI)](https://doi.org/10.1023/A:1007782300874)
* Carl Bruggeman, Oscar Waddell, and R. Kent Dybvig. 1996. Representing control in the presence of one-shot continuations. SIGPLAN Not. 31, 5 (May 1996), 99–107.  
  [(DOI)](https://doi.org/10.1145/249069.231395)
* Stephen Dolan, Leo White, KC Sivaramakrishnan, Jeremy Yallop and Anil Madhavapeddy. 2015. Effective Concurrency with Algebraic Effects. OCaml Workshop.  
  [(PDF)](https://kcsrk.info/papers/effects_ocaml15.pdf)
