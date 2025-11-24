#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "optparse.h"

/* Collect positional options into an option list
 *
 * Internally it does not "collect" options into a separate list, instead it
 * repartitions the existing list, connecting positionals to positionals
 * and non-positionals to non-positionals, and then places positionals at the
 * end of the list.
 */
static int collect_positionals(OptionList* pos, OptionList const* opts);

/* Find an option by its long name */
static Option* find_option_lname(OptionList const* opts, char const* str);

/* Find an option by its short name */
static Option* find_option_sname(OptionList const* opts, char const* str);

/* Assign a value to an option */
static int execute_option(Option* opt, int idx, int argc, char** argv, OptParserError* err);

/* Check if an option requires an argument */
static bool opt_has_argument(Option const* opt);

/* Parse short options group */
static int parse_short_opts(OptionList* opts, int idx, int argc, char** argv, int* omit_idx, OptParserError* err);

static int print_option(Option const* opt, FILE* fout);
static int print_option_bare(Option const* opt, FILE* fout);
static int print_option_names(Option const* opt, FILE* fout);

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
      if (parse_short_opts(opts, i, argc, argv, &omit_idx, err) == -1)
        return -1;

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
    Option* pos = positionals.start;
    while (pos_count != n_pos && pos->_next != NULL) {
      pos_count += 1;
      pos = pos->_next;
    }
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
  case OPTERROR_ONE_ARG_OPT_PER_GROUP:
    return "one argument option allowed per short option group";
  case OPTERROR_INT_TYPE_ERROR:
    return "required argument of type int";
  default:
    __builtin_unreachable();
  }
}

void print_error(OptParserError* err, FILE* fout) {
  assert(err);
  assert(fout);

  fprintf(fout, "%s: ", opterror_type_to_str(err->type));

  if (err->opt) {
    fprintf(fout, "%s ", err->opt);
    if (!err->sname && !err->lname) {
      fprintf(fout, "\n");
      return;
    }
  }

  if (err->sname) {
    fprintf(fout, "-%c", err->sname);
    if (err->lname)
      fprintf(fout, "|");
  }

  if (err->lname)
    fprintf(fout, "--%s", err->lname);

  fprintf(fout, "\n");
}

static int collect_positionals(OptionList* pos, OptionList const* opts) {
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

  if (last_n)
    last_n->_next = pos->start;

  pos->end->_next = NULL;
  return count;
}

static Option* find_option_lname(OptionList const* opts, char const* str) {
  OPTLIST_FOREACH(opts, opt) {
    if (opt->type == OPTION_POSITIONAL)
      return NULL;
    if (!opt->lname)
      continue;
    if (strcmp(opt->lname, str) == 0)
      return opt;
  }
  return NULL;
}

static Option* find_option_sname(OptionList const* opts, char const* str) {
  OPTLIST_FOREACH(opts, opt) {
    if (opt->type == OPTION_POSITIONAL)
      return NULL;
    if (!opt->sname)
      continue;
    if (str[0] == opt->sname)
      return opt;
  }
  return NULL;
}

static int execute_option(Option* opt, int idx, int argc, char** argv, OptParserError* err) {
  assert(opt->type != OPTION_POSITIONAL);
  switch (opt->type) {
  case OPTION_STORE_STR:
    if (idx + 1 == argc) {
      *err = (OptParserError){OPTERROR_ARGUMENT_REQUIRED, .opt = argv[idx]};
      return -1;
    }
    *(char const**)opt->dest = argv[idx + 1];
    break;
  case OPTION_STORE_INT: {
    if (idx + 1 == argc) {
      *err = (OptParserError){OPTERROR_ARGUMENT_REQUIRED, .opt = argv[idx]};
      return -1;
    }

    char* end = NULL;
    long result = strtol(argv[idx + 1], &end, 10);
    if (*end != '\0') {
      *err = (OptParserError){OPTERROR_INT_TYPE_ERROR, .opt = argv[idx]};
      return -1;
    }

    *(long*)opt->dest = result;
    break;
  }
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

static bool opt_has_argument(Option const* opt) {
  switch (opt->type) {
  case OPTION_STORE_STR:
  case OPTION_STORE_INT:
    return true;
  case OPTION_POSITIONAL:
  case OPTION_FLAG:
  case OPTION_INCREMENT:
  default:
    return false;
  }
}

static int print_option_names(Option const* opt, FILE* fout) {
  assert(opt->sname || opt->lname);
  if (!opt->sname)
    return fprintf(fout, "--%s", opt->lname);
  else if (!opt->lname)
    return fprintf(fout, "-%c", opt->sname);
  else
    return fprintf(fout, "-%c|--%s", opt->sname, opt->lname);
}

static int print_option_bare(Option const* opt, FILE* fout) {
  int total_len = 0;

  switch (opt->type) {

  case OPTION_POSITIONAL:
    assert(opt->lname || opt->metavar);
    total_len += fprintf(fout, "%s", opt->metavar ? opt->metavar : opt->lname);
    break;

  case OPTION_FLAG:
    /* fallthrough */

  case OPTION_INCREMENT:
    total_len += print_option_names(opt, fout);
    break;

  case OPTION_STORE_STR:
    /* fallthrough */

  case OPTION_STORE_INT:
    total_len += print_option_names(opt, fout);
    if (opt->metavar)
      total_len += fprintf(fout, " %s", opt->metavar);
    else if (opt->lname)
      total_len += fprintf(fout, " %s", opt->lname);
    else
      total_len += fprintf(fout, " %c", opt->sname);
    break;
  }

  return total_len;
}

static int parse_short_opts(OptionList* opts, int idx, int argc, char** argv, int* omit_idx, OptParserError* err) {
  char const* str = argv[idx];
  size_t const len = strlen(str);

  for (size_t i = 1; i < len; ++i) {
    Option* opt = find_option_sname(opts, argv[idx] + i);
    if (!opt) {
      *err = (OptParserError){OPTERROR_UNKNOWN, .opt = argv[idx]};
      return -1;
    }
    if (execute_option(opt, idx, argc, argv, err) == -1)
      return -1;
    if (opt_has_argument(opt)) {
      if (*omit_idx != -1) {
        *err = (OptParserError){OPTERROR_ONE_ARG_OPT_PER_GROUP, .opt = argv[idx], .sname = argv[idx][i]};
      }
      *omit_idx = idx + 1;
    }
  }

  return 0;
}

static int print_option(Option const* opt, FILE* fout) {
  int total_len = 0;

  if (!opt->required && opt->type != OPTION_POSITIONAL)
    total_len += fprintf(fout, "[");
  total_len += print_option_bare(opt, fout);
  if (!opt->required && opt->type != OPTION_POSITIONAL)
    total_len += fprintf(fout, "]");

  total_len += fprintf(fout, " ");

  return total_len;
}
