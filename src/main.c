#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "optparse.h"

int main(int argc, char** argv) {
  bool foo = false;
  bool bar = false;
  bool help = false;
  int verbose = 0;
  char const* str = NULL;
  char const* path = NULL;

  Option foo_opt = {
      .lname = "foo",
      .sname = 'f',
      .type = OPTION_FLAG,
      .dest = &foo,
      .help = "foo option",
  };
  Option bar_opt = {
      .lname = "bar",
      .sname = 'b',
      .type = OPTION_FLAG,
      .dest = &bar,
      .help = "bar option",
  };
  Option help_opt = {
      .lname = "help",
      .sname = 'h',
      .type = OPTION_FLAG,
      .dest = &help,
      .help = "show help message",
  };
  Option path_opt = {
      .lname = "path",
      .type = OPTION_POSITIONAL,
      .dest = &path,
      .help = "a path",
  };
  Option str_opt = {
      .lname = "str",
      .sname = 's',
      .metavar = "STR",
      .type = OPTION_STR,
      .required = true,
      .dest = &str,
      .help = "string option",
  };
  Option verbose_opt = {
      .lname = "verbose",
      .sname = 'v',
      .type = OPTION_INCREMENT,
      .dest = &verbose,
      .help = "verbosity level",
  };

  OptionList opts;
  OPTLIST_INIT(opts, foo_opt);
  OPTLIST_ADD(opts, bar_opt);
  OPTLIST_ADD(opts, help_opt);
  OPTLIST_ADD(opts, path_opt);
  OPTLIST_ADD(opts, str_opt);
  OPTLIST_ADD(opts, verbose_opt);

  OptParserError err = {0};
  if (parse_opts(&opts, argc, argv, &err) == -1 && !help) {
    print_error(&err, stderr);
    print_usage(&opts, stderr, argv[0]);
    return 64;
  }

  if (help) {
    print_usage(&opts, stdout, argv[0]);
    print_help(&opts, stdout);
    return 0;
  }

  printf("foo=%i bar=%i verbose=%i path=%s str=%s\n", foo, bar, verbose, path, str);

  return 0;
}
