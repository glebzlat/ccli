# Quick and dirty C command-line argument parser

Everything is placed in a single `main.c` file, there is a usage example.

## Features

- Library does not require memory allocation at all. Options form a linked list,
  output variables are modified via pointers.
- Option long and short names.
- Short option grouping. Short options `-a -b -c` can be grouped into `-abc`.
- Positional arguments.

## License

Licensed under [MIT license](./LICENSE).
