#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define OPTLIST_INIT(LL, OPT)                                                                                          \
  do {                                                                                                                 \
    LL.start = &(OPT);                                                                                                 \
    LL.end = &(OPT);                                                                                                   \
  } while (0)

#define OPTLIST_ADD(LL, OPT)                                                                                           \
  do {                                                                                                                 \
    LL.end->_next = &(OPT);                                                                                            \
    LL.end = &(OPT);                                                                                                   \
  } while (0)

#define OPTLIST_FOREACH(LL, ITER) for (Option* ITER = (LL)->start; ITER != NULL; ITER = ITER->_next)

typedef enum {
  OPTION_POSITIONAL,
  OPTION_FLAG,
  OPTION_STR,
  OPTION_INCREMENT,
} OptionType;

typedef struct Option {
  char const* lname;
  char const* sname;
  char const* metavar;
  char const* help;
  OptionType type;
  bool required;
  void* dest;
  struct Option* _next;
  bool _activated;
} Option;

typedef struct {
  Option* start;
  Option* end;
} OptionList;

typedef enum {
  OPTERROR_NOERR = 0,
  OPTERROR_UNKNOWN,
  OPTERROR_UNEXPECTED_POSITIONAL,
  OPTERROR_EXPECTED_POSITIONAL,
  OPTERROR_ARGUMENT_REQUIRED,
  OPTERROR_REQUIRED_OPTION,
} OptParserErrorType;

typedef struct {
  OptParserErrorType type;
  char const* lname;
  char const* sname;
  char const* opt;
} OptParserError;

int parse_opts(OptionList* opts, int argc, char** argv, OptParserError* err);
void print_usage(OptionList* opts, FILE* fout, char const* progname);
void print_help(OptionList* opts, FILE* fout);
char const* opterror_type_to_str(OptParserErrorType err_type);
void print_error(OptParserError* err, FILE* fout);

int main(int argc, char** argv) {
  bool foo = false;
  bool bar = false;
  bool help = false;
  int verbose = 0;
  char const* str = NULL;
  char const* path = NULL;

  Option foo_opt = {
      .lname = "foo",
      .sname = "f",
      .type = OPTION_FLAG,
      .dest = &foo,
      .help = "foo option",
  };
  Option bar_opt = {
      .lname = "bar",
      .sname = "b",
      .type = OPTION_FLAG,
      .dest = &bar,
      .help = "bar option",
  };
  Option help_opt = {
      .lname = "help",
      .sname = "h",
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
      .sname = "s",
      .metavar = "STR",
      .type = OPTION_STR,
      .required = true,
      .dest = &str,
      .help = "string option",
  };
  Option verbose_opt = {
    .lname = "verbose",
    .sname = "v",
    .type = OPTION_INCREMENT,
    .dest = &verbose,
    .help = "verbosity level"
  };

  OptionList opts;
  OPTLIST_INIT(opts, foo_opt);
  OPTLIST_ADD(opts, bar_opt);
  OPTLIST_ADD(opts, help_opt);
  OPTLIST_ADD(opts, path_opt);
  OPTLIST_ADD(opts , str_opt);
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

/* Collect positional options into an option list
 *
 * Internally it does not "collect" options into a separate list, instead it
 * repartitions the existing list, connecting positionals to positionals
 * and non-positionals to non-positionals, and then places positionals at the
 * end of the list.
 */
int collect_positionals(OptionList* pos, OptionList* opts);

/* Find an option by its long name */
Option* find_option_lname(OptionList* opts, char const* str);

/* Find an option by its short name */
Option* find_option_sname(OptionList* opts, char const* str);

/* Assign a value to an option */
int execute_option(Option* opt, int idx, int argc, char** argv, OptParserError* err);

/* Check if an option requires an argument */
bool opt_has_argument(Option* opt);

int print_option(Option* opt, FILE* fout);
int print_option_bare(Option* opt, FILE* fout);
int print_option_names(Option* opt, FILE* fout);

int parse_opts(OptionList* opts, int argc, char** argv, OptParserError* err) {
  assert(opts);
  assert(argv);
  assert(err);

  OptionList positionals;
  int const n_pos = collect_positionals(&positionals, opts);
  int pos_count = 0;

  int omit_idx = -1;

  for (int i = 1; i < argc; ++i) {
    if (i == omit_idx)
      continue;

    if (argv[i][0] == '-' && argv[i][1] == '-') {
      Option* opt = find_option_lname(opts, argv[i] + 2);
      if (!opt) {
        *err = (OptParserError){OPTERROR_UNKNOWN, .opt = argv[i]};
        return -1;
      }
      if (execute_option(opt, i, argc, argv, err) == -1)
        return -1;
      if (opt_has_argument(opt))
        omit_idx = i + 1;

    } else if (argv[i][0] == '-') {
      Option* opt = find_option_sname(opts, argv[i] + 1);
      if (!opt) {
        *err = (OptParserError){OPTERROR_UNKNOWN, .opt = argv[i]};
        return -1;
      }
      if (execute_option(opt, i, argc, argv, err) == -1)
        return -1;
      if (opt_has_argument(opt))
        omit_idx = i + 1;

    } else {
      if (pos_count == n_pos) {
        *err = (OptParserError){OPTERROR_UNEXPECTED_POSITIONAL, .opt = argv[i]};
        return -1;
      }
      assert(positionals.start);
      *(char const**)positionals.start->dest = argv[i];
      positionals.start = positionals.start->_next;
      pos_count += 1;
    }
  }

  if (pos_count < n_pos) {
    Option* pos = positionals.start->_next;
    *err = (OptParserError){OPTERROR_EXPECTED_POSITIONAL, .opt = pos->lname};
    return -1;
  }

  OPTLIST_FOREACH(opts, opt) {
    if (opt->type == OPTION_POSITIONAL)
      break;
    if (opt->required && !opt->_activated) {
      err->type = OPTERROR_REQUIRED_OPTION;
      if (opt->lname)
        err->lname = opt->lname;
      if (opt->sname)
        err->sname = opt->sname;
      return -1;
    }
  }

  return 0;
}

void print_usage(OptionList* opts, FILE* fout, char const* progname) {
  assert(opts);
  assert(fout);

  /* collect_positionals is called in parse_opts and that should be enough,
   * but this is for the case if the user calls print_usage without parsing
   * options */
  OptionList positionals;
  collect_positionals(&positionals, opts);

  char const* last_slash = strrchr(progname, '/');
  if (last_slash) {
    fprintf(fout, "%s ", last_slash + 1);
  } else {
    fprintf(fout, "%s ", progname);
  }

  OPTLIST_FOREACH(opts, opt) {
    if (opt->type == OPTION_POSITIONAL)
      break;
    print_option(opt, fout);
  }

  OPTLIST_FOREACH(&positionals, opt) { print_option(opt, fout); }

  fprintf(fout, "\n");
}

void print_help(OptionList* opts, FILE* fout) {
  assert(opts);
  assert(fout);

#define OPT_COLUMN_WIDTH 30

  /* collect_positionals is called in parse_opts and that should be enough,
   * but this is for the case if the user calls print_help without parsing
   * options or calling print_usage */
  OptionList positionals;
  collect_positionals(&positionals, opts);

  OPTLIST_FOREACH(opts, opt) {
    int opt_column_len = 0;
    opt_column_len += fprintf(fout, "  ");
    opt_column_len += print_option_bare(opt, fout);
    if (opt->help) {
      if (opt_column_len >= OPT_COLUMN_WIDTH) {
        fprintf(fout, "\n%*s", OPT_COLUMN_WIDTH, " ");
      } else {
        fprintf(fout, "%*s", OPT_COLUMN_WIDTH - opt_column_len, " ");
      }
      fprintf(fout, "%s\n", opt->help);
    }
  }
}

char const* opterror_type_to_str(OptParserErrorType err_type) {
  switch (err_type) {
  case OPTERROR_NOERR:
    return "no error";
  case OPTERROR_UNKNOWN:
    return "unknown option";
  case OPTERROR_UNEXPECTED_POSITIONAL:
    return "unexpected positional argument";
  case OPTERROR_EXPECTED_POSITIONAL:
    return "expected a positional argument";
  case OPTERROR_ARGUMENT_REQUIRED:
    return "option requires an argument";
  case OPTERROR_REQUIRED_OPTION:
    return "option required";
  default:
    __builtin_unreachable();
  }
}

void print_error(OptParserError* err, FILE* fout) {
  assert(err);
  assert(fout);

  fprintf(fout, "%s: ", opterror_type_to_str(err->type));

  if (err->opt) {
    fprintf(fout, "%s\n", err->opt);
    return;
  }

  if (err->sname) {
    fprintf(fout, "-%s", err->sname);
    if (err->lname)
      fprintf(fout, "|");
  }

  if (err->lname)
    fprintf(fout, "--%s", err->lname);

  fprintf(fout, "\n");
}

int collect_positionals(OptionList* pos, OptionList* opts) {
  pos->start = pos->end = NULL;
  Option* last_n = NULL;
  int count = 0;
  OPTLIST_FOREACH(opts, i) {
    if (i->type == OPTION_POSITIONAL) {
      if (!pos->end) {
        pos->start = pos->end = i;
      } else {
        pos->end = pos->end->_next = i;
      }
      count += 1;
    } else {
      if (!last_n) {
        last_n = i;
      } else {
        last_n = last_n->_next = i;
      }
    }
  }
  last_n->_next = pos->start;
  pos->end->_next = NULL;
  return count;
}

Option* find_option_lname(OptionList* opts, char const* str) {
  OPTLIST_FOREACH(opts, opt) {
    if (!opt->lname)
      continue;
    if (strcmp(opt->lname, str) == 0)
      return opt;
  }
  return NULL;
}

Option* find_option_sname(OptionList* opts, char const* str) {
  OPTLIST_FOREACH(opts, opt) {
    if (!opt->sname)
      continue;
    if (strcmp(opt->sname, str) == 0)
      return opt;
  }
  return NULL;
}

int execute_option(Option* opt, int idx, int argc, char** argv, OptParserError* err) {
  assert(opt->type != OPTION_POSITIONAL);
  switch (opt->type) {
  case OPTION_STR:
    if (idx + 1 == argc) {
      *err = (OptParserError){OPTERROR_ARGUMENT_REQUIRED, .opt = argv[idx]};
      return -1;
    }
    *(char const**)opt->dest = argv[idx + 1];
    break;
  case OPTION_FLAG:
    *(bool*)opt->dest = true;
    break;
  case OPTION_INCREMENT:
    *(int*)opt->dest += 1;
    break;
  case OPTION_POSITIONAL:
    __builtin_unreachable();
  }
  opt->_activated = true;
  return 0;
}

bool opt_has_argument(Option* opt) { return opt->type == OPTION_STR; }

int print_option_names(Option* opt, FILE* fout) {
  assert(opt->sname || opt->lname);
  if (!opt->sname)
    return fprintf(fout, "--%s", opt->lname);
  else if (!opt->lname)
    return fprintf(fout, "-%s", opt->sname);
  else
    return fprintf(fout, "-%s|--%s", opt->sname, opt->lname);
}

int print_option_bare(Option* opt, FILE* fout) {
  int total_len = 0;

  switch (opt->type) {

  case OPTION_POSITIONAL:
    assert(opt->lname);
    total_len += fprintf(fout, "%s", opt->lname);
    break;

  case OPTION_FLAG:
    /* fallthrough */

  case OPTION_INCREMENT:
    total_len += print_option_names(opt, fout);
    break;

  case OPTION_STR:
    total_len += print_option_names(opt, fout);
    if (opt->metavar)
      total_len += fprintf(fout, " %s", opt->metavar);
    else
      total_len += fprintf(fout, " %s", opt->lname ? opt->lname : opt->sname);
    break;
  }

  return total_len;
}

int print_option(Option* opt, FILE* fout) {
  int total_len = 0;

  if (!opt->required && opt->type != OPTION_POSITIONAL)
    total_len += fprintf(fout, "[");
  total_len += print_option_bare(opt, fout);
  if (!opt->required && opt->type != OPTION_POSITIONAL)
    total_len += fprintf(fout, "]");

  total_len += fprintf(fout, " ");

  return total_len;
}
