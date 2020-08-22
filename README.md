# Pthandlers: affine effect handling via POSIX threads
This C library provides a proof-of-concept encoding of affine effect
handlers using pthreads.

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
