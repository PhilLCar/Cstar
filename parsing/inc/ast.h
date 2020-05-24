#ifndef AST_PARSING
#define AST_PARSING

#include <diagnostic.h>
#include <bnf.h>
#include <generic_parser.h>
#include <symbol.h>
#include <strings.h>
#include <array.h>

#define AST_LOCK    0

typedef enum aststatus {
  STATUS_NOSTATUS = 0,
  STATUS_ONGOING,
  STATUS_CONFIRMED,
  STATUS_FAILED,
  STATUS_PARTIAL,
  STATUS_REC,
  STATUS_SKIP
} ASTStatus;

typedef enum asterrortype {
  ERROR_CONCAT_0MATCH,
  ERROR_CONCAT_MANYMATCH,
  ERROR_UNIMPLEMENTED,
  WARNING_AMBIGUOUS
} ASTErrorType;

typedef enum astflags {
  ASTFLAGS_NONE     = 0,
  ASTFLAGS_CONCAT   = 1,
  ASTFLAGS_REC      = 2,
  ASTFLAGS_STARTED  = 4,
  ASTFLAGS_NOREC    = 8,
  ASTFLAGS_FRONT    = 16,
  ASTFLAGS_END      = 32
} ASTFlags;

typedef struct asterror {
  ASTErrorType  errno;
  BNFNode      *bnfref;
} ASTError;

typedef struct astnode {
  String         *name;
  BNFNode        *ref;
  Array          *subnodes;
  String         *value;
  ASTStatus       status : 16;
  short           pointed;
  short           pos;
  short           rec;
  int             sympos;
  int             symline;
} ASTNode;

ASTNode *newASTNode(ASTNode*, BNFNode*);
void     astnewsymbol(ASTNode*, BNFNode*, ASTFlags, Symbol*);
ASTNode *parseast(char*);
void     deleteAST(ASTNode**);

#endif
