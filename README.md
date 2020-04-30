# C* (C-STAR)
## The language to rule them all (and in darkness bind them)
C* is a language creation project: hybrid language that merges C, C++ and C# in the spirit of functionnal programming.

Project
=======
The idea is merge the ease of use of C# with the expressability of C, C++. Mainly this 
means a language very close to C#, but where pointers and memory are still accessible.

Although the language will keep its imperative-style syntax, it will be implemented as a functionnal language; lambdas will be natively supported by the language.

Implementation
==============
## The project includes (or will include):
- The grammar files (written in EBNF).
- An EBNF parser (written in C).
- An AST generator that takes the BNF tree generated by the EBNF parser and a .CSR (C*) file to create the AST
- The CISOR compiler, which includes:
    - An intermediate compiler (written in C) that will convert the AST into a .CSIR (C* intermediary respresentation) file
    - A compiler that will convert the .CSIR files in .S files
    - An assembler (eventually)
    - A linker (eventually)

*The code will be converted to GAS syntax, the assembly will then be compiled by gcc (for now, with the intention of writing the assembler eventually).
