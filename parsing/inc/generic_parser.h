#ifndef GENERIC_PARSING
#define GENERIC_PARSING

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tracked_file.h"

typedef struct parser {
  char   *whitespaces;
  char   *escapes;
  char  **breaksymbols;
  int     max_depth;
} Parser;

char   *characters(FILE*);
char  **word(FILE*);
void    emptyline(FILE*);
int     strcmps(char*, char*);
int     extend(char**, int*, int*, char);

Parser *newParser(char*);
void    deleteParser(Parser**);

#endif
