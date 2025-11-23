# Quick and dirty C command-line argument parser

Everything is placed in a single `main.c` file, there is a usage example.

Main ideas of this implementation are:

    - Options themselves form a linked list;

    - Options store pointers to output variables;

    - Parser does not require memory allocation.

Licensed under [MIT license](./LICENSE).
