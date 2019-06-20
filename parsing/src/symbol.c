#include <symbol.h>

// Returns the next symbol in the tracked file tf
int nextsymbol(TrackedFile *tf, Parser *parser, Symbol *symbol) {
  char  c;
  int   buf_size = 0, buf_cap = 2;
  char *buf = malloc(buf_cap * sizeof(char));
  int   new = 0;
  int   pos = 0;

  if (buf != NULL) {
    memset(buf, 0, buf_cap * sizeof(char));
    while ((c = tfgetc(tf))) {
      if (c == EOF) break;
      int cmp, ws = 0;
      //////////////////////////////////////// NEW-LINE ////////////////////////////////////////
      if (c == '\n') {
	if (buf_size) {
	  tfungetc(tf, c);
	}
	else {
	  pos = tf->position;
	  if(!extend(&buf, &buf_size, &buf_cap, c)) goto next_fail;
	}
	break;
      }
      //////////////////////////////////////// ESCAPE ////////////////////////////////////////
      for (int i = 0; parser->escapes[i]; i++) {
	if (c == parser->escapes[i]) {
	  ws = 1;
	  break;
	}
      }
      if (ws) {
	c = tfgetc(tf);
	if (!extend(&buf, &buf_size, &buf_cap, c)) goto next_fail;
	continue;
      }
      //////////////////////////////////////// WHITESPACE ////////////////////////////////////////
      for (int i = 0; parser->whitespaces[i]; i++) {
	if (c == parser->whitespaces[i]) {
	  ws = 1;
	  break;
	}
      }
      if (ws) {
	if(buf_size) {
	  break;
	}
	else continue;
      }
      //////////////////////////////////////// BREAKSYMBOLS ////////////////////////////////////////
      for (int i = 0; parser->breaksymbols[i]; i++) {
	if ((cmp = strcmps(tf->buffer, parser->breaksymbols[i]))) {
	  if (cmp > ws) {
	    ws = cmp;
	  }
	}
      }
      if (ws) {
        if (buf_size) {
	  tfungetc(tf, c);
	}
	else {
	  pos = tf->position;
	  for (int i = 1;; i++) {
	    if(!extend(&buf, &buf_size, &buf_cap, c)) goto next_fail;
	    if (i == ws) break;
	    c = tfgetc(tf);
	  }
	}
	break;
      }
      //////////////////////////////////////// SYMBOL ////////////////////////////////////////
      if (!buf_size) pos = tf->position;
      if(!extend(&buf, &buf_size, &buf_cap, c)) {
      next_fail:
	free(buf);
	return 0;
      }
    }
    symbol->text = buf;
    symbol->line = tf->line;
    symbol->position = pos;
    new = 1;
  }
  return new;
}

// Parses a file using the rules provided in parser
Symbol *parse(char *filename, Parser *parser) {
  TrackedFile *tf = tfopen(filename, parser->max_depth);
  int symbols_size = 0, symbols_cap = 1024;
  Symbol *symbols = NULL;
  if (tf != NULL) {
    symbols = malloc(symbols_cap * sizeof(Symbol));
    if (symbols != NULL) {
      memset(symbols, 0, symbols_cap * sizeof(Symbol));
      while (nextsymbol(tf, parser, &symbols[symbols_size])) {
	if (!symbols[symbols_size].text[0]) {
	  // END OF FILE
	  break;
	}
	if (++symbols_size == symbols_cap) {
	  Symbol *t = realloc(symbols, (symbols_cap *= 2) * sizeof(Symbol));
	  if (t != NULL) {
	    symbols = t;
	    memset(symbols + symbols_size, 0, (symbols_cap - symbols_size) * sizeof(Symbol));
	  } else break;
	}
      }
    }
    tfclose(tf);
  }
  return symbols;
}

SymbolStream *sopen(char *filename, Parser *parser) {
  SymbolStream *ss = malloc(sizeof(SymbolStream));
  TrackedFile  *tf = tfopen(filename, parser->max_depth);
  
  if (ss != NULL && parser != NULL) {
    ss->filename = filename;
    ss->tfptr    = tf;
    ss->parser   = parser;
    ss->symbol   = malloc(sizeof(Symbol));
  } else {
    if (ss) free(ss);
    if (parser) deleteParser(&parser);
  }
  return ss;
}

void sclose(SymbolStream *ss) {
  if (ss->tfptr) tfclose(ss->tfptr);
  if (ss->symbol) free(ss->symbol);
  if (ss) free(ss);
}

Symbol *getsymbol(SymbolStream *ss) {
  Symbol *s = NULL;
  if(nextsymbol(ss->tfptr, ss->parser, ss->symbol)) {
    s = ss->symbol;
  }
  return s;
}
