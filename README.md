# Pthandlers: affine effect handling via POSIX threads
This C library provides a proof-of-concept encoding of affine effect
handlers using pthreads.

## References

* Sanjeev Kumar, Carl Bruggeman, and R. Kent Dybvig. 1998. Threads Yield Continuations. Higher-Order and Symbolic Computation 10, 223–236 (1998).  
  [(DOI)](https://doi.org/10.1023/A:1007782300874)
* Carl Bruggeman, Oscar Waddell, and R. Kent Dybvig. 1996. Representing control in the presence of one-shot continuations. SIGPLAN Not. 31, 5 (May 1996), 99–107.  
  [(DOI)](https://doi.org/10.1145/249069.231395)
* Stephen Dolan, Leo White, KC Sivaramakrishnan, Jeremy Yallop and Anil Madhavapeddy. 2015. Effective Concurrency with Algebraic Effects. OCaml Workshop.  
  [(PDF)](https://kcsrk.info/papers/effects_ocaml15.pdf)
