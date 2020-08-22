# Pthandlers: affine effect handling via POSIX threads

This C library provides a proof-of-concept encoding of affine effect
handlers using POSIX threads (pthreads).

The encoding strategy utilises the insights of Kumar et al. (1998),
who implement one-shot delimited continuations using threads, as the
basis for simulating the implementation of effect handlers in
Multicore OCaml (Dolan et al. (2015)), which uses a variation of
segmented stacks á la Bruggeman et al. (1996).

## Building

The `Makefile` provides rules for building static and shared
variations of this library, simply type

```shell
$ make lib
```

This rule assembles the static library `libpthandlers.a` and the
shared library `libpthandlers.so` and outputs both in the `lib/`
directory. To build either library variant alone use `make static` or
`make shared` respectively.

### Examples

This repository contains some example programs. To build them all
simply type `make all`. Alternatively, they can be built individually

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
`gcc` (version 7.5.0) fails to tail call optimise some functions.

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
