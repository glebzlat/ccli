#ifndef OPTPARSE_H
#define OPTPARSE_H

#include <stdbool.h>
#include <stdio.h>

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
  char sname;
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
  OPTERROR_ONE_ARG_OPT_PER_GROUP,
} OptParserErrorType;

typedef struct {
  OptParserErrorType type;
  char const* lname;
  char sname;
  char const* opt;
} OptParserError;

/** Parse command line options
 *
 * Sets an `err` output variable on error.
 *
 * @return 0 on success, -1 on error
 */
int parse_opts(OptionList* opts, int argc, char** argv, OptParserError* err);

/** Print program usage */
void print_usage(OptionList* opts, FILE* fout, char const* progname);

/** Print more elaborate program usage with help strings */
void print_help(OptionList* opts, FILE* fout);

/** Get a string representation of an error */
char const* opterror_type_to_str(OptParserErrorType err_type);

/** Pretty-print an error */
void print_error(OptParserError* err, FILE* fout);

#endif
