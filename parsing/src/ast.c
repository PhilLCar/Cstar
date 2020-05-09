#include <ast.h>

void astnewchar(Array*, ASTNode*, BNFNode*, Array*, ASTFlags, char);
ASTNode *astsubnode(ASTNode*, int);

ASTNode *newASTNode(ASTNode *ast, BNFNode *bnf)
{
  ASTNode *new = malloc(sizeof(ASTNode));
  if (new) {
    new->name     = newString(bnf ? bnf->name : "");
    new->ref      = bnf;
    new->subnodes = newArray(sizeof(ASTNode*));
    new->value    = newString("");
    new->status   = STATUS_NOSTATUS;
    new->pos      = 0;
    new->rec      = 0;
    if (ast) push(ast->subnodes, &new);
  }
  return new;
}

ASTNode *duplicateAST(ASTNode *ast)
{
  ASTNode *new = malloc(sizeof(ASTNode));
  if (new) {
    new->name     = newString(ast->name->content);
    new->ref      = ast->ref;
    new->subnodes = newArray(sizeof(ASTNode*));
    new->value    = newString(ast->value->content);
    new->status   = ast->status;
    new->pos      = ast->pos;
    new->rec      = ast->rec;
    if (new->subnodes) {
      for (int i = 0; i < ast->subnodes->size; i++) {
        ASTNode *n = duplicateAST(astsubnode(ast, i));
        push(new->subnodes, &n);
      }
    }
  }
  return new;
}

ASTNode *astsubnode(ASTNode *ast, int index) {
  ASTNode **ret = (ASTNode**)at(ast->subnodes, index);
  if (ret) return *ret;
  return NULL;
}

int astempty(ASTNode *ast) {
  return !ast->subnodes->size && !ast->value->length;
}

BNFNode *bnfsubnode(BNFNode *bnf, int index) {
  return *(BNFNode**)at(bnf->content, index);
}

void freeastnode(ASTNode *node) {
  deleteString(&node->name);
  deleteString(&node->value);
  deleteArray(&node->subnodes);
}

void astconcatnode(ASTNode *c) {
  while(c->subnodes->size) {
    ASTNode *n = *(ASTNode**)rem(c->subnodes, 0);
    concat(c->value, newString(n->value->content));
    deleteAST(&n);
  }
}

void astupnode(ASTNode *super, ASTNode *sub) {
  concat(super->value, newString(sub->value->content));
  if (!super->name->content[0]) {
    super->ref = sub->ref;
    concat(super->name, newString(sub->name->content));
  }
  Array *tmp = sub->subnodes;
  sub->subnodes = newArray(sizeof(ASTNode*));
  while (super->subnodes->size) deleteAST(pop(super->subnodes));
  deleteArray(&super->subnodes);
  super->subnodes = tmp;
}

void asterror(Array *errors, ASTErrorType errno, BNFNode *bnf) {
  ASTError error;
  error.errno  = errno;
  error.bnfref = bnf;
  push(errors, &error);
}

void astnewchar(Array *errors, ASTNode *ast, BNFNode *bnf, Array *reserved, ASTFlags flags, char c) {
  ASTNode  *superast;
  ASTNode  *subast;
  ASTNode  *save    = NULL;
  BNFNode  *subbnf;
  int       size    = 0;
  int       f       = 0;
  char     *content = bnf->content;
  if (bnf->type != NODE_LEAF) size = ((Array*)bnf->content)->size;
  push(bnf->refs, &ast);

  switch (bnf->type) {
    case NODE_ROOT:
    case NODE_RAW:
    case NODE_MANY_OR_NONE:
    case NODE_MANY_OR_ONE:
    case NODE_ONE_OR_NONE:
      /// UNIMPLEMENTED
      asterror(errors, ERROR_UNIMPLEMENTED, bnf);
      break;
    case NODE_LEAF:
      if (c == AST_LOCK) {
        if (!content || ast->status == STATUS_CONFIRMED) { ast->status = STATUS_CONFIRMED; break; }
        else if (!ast->pos && flags & ASTFLAGS_STARTED)  { ast->status = STATUS_FAILED;    break; }
        else if (!ast->pos)                              { ast->status = STATUS_ONGOING;   break; }
      } else if (c == AST_CLOSE) {
        if (!content || ast->status == STATUS_CONFIRMED) { ast->status = STATUS_CONFIRMED; break; }
        else if (!ast->pos)                              { ast->status = STATUS_FAILED;    break; }
        c = AST_LOCK;
      }
      if (content && content[ast->pos] == c) {
        if (!c || ((flags & ASTFLAGS_CONCAT) && !content[ast->pos + 1])) {
          ast->status = STATUS_CONFIRMED;
          concat(ast->value, newString(bnf->content));
        } else { 
          ast->status = STATUS_ONGOING;
          ast->pos++;
        }
      } else { ast->status = STATUS_FAILED; }
      break;
    case NODE_CONCAT:
    case NODE_LIST:
    //////////////////////////////// LIST HEADER /////////////////////////////////////////
      superast = ast;
      if (superast->status == STATUS_FAILED) break;
      if (superast->status == STATUS_NOSTATUS) superast->status = STATUS_ONGOING;
      if (superast->status == STATUS_CONFIRMED) {
        if (c == AST_LOCK || c == AST_CLOSE) break;
        else { 
          while (superast->subnodes->size) deleteAST(pop(superast->subnodes));
          superast->status = STATUS_FAILED;
        }
      }
      if (!superast->subnodes->size) newASTNode(superast, bnf);
      if (bnf->type == NODE_CONCAT) {
        if (c == AST_LOCK)       { if (superast->pos) superast->pos++; }
        else if (!superast->pos) {                    superast->pos++; }
      }
      for (int i = 0, lim = superast->subnodes->size; i < lim; i++) {
        ast = astsubnode(superast, i);
        if (ast->pos >= size) {
          if (c == AST_LOCK || c == AST_CLOSE) continue;
          else { deleteAST(rem(superast->subnodes, i--)); lim--; }
        }
        ast->status = STATUS_ONGOING;
        ///////////////////////////// LIST BODY ////////////////////////////////////////////
        for (char nc = c; ast->pos < size;) {
          subast = astsubnode(ast, ast->pos + ast->rec);
          subbnf = bnfsubnode(bnf, ast->pos);
          if (!subast) subast = newASTNode(ast, NULL);
          f |= (flags & ~ASTFLAGS_REC) | (bnf->type == NODE_CONCAT ? ASTFLAGS_CONCAT : 0);
          f |= superast->pos > 1 ? ASTFLAGS_STARTED : 0;
          f |= !ast->pos ? ASTFLAGS_FRONT : 0;
          astnewchar(errors, subast, subbnf, reserved, f, nc);
          if (subast->status == STATUS_FAILED) {
            deleteAST(rem(superast->subnodes, i--)); lim--;
          } else if (subast->status == STATUS_PARTIAL) {
            ASTNode *nast = newASTNode(superast, bnf);
            subast->status = STATUS_ONGOING; // STEAL PARTIAL AWAY
            nast->status   = STATUS_ONGOING;
            nast->rec = ast->rec;
            nast->pos = ast->pos;
            for (int j = 0; j < ast->pos + ast->rec; j++) {
              ASTNode *confirmed = duplicateAST(astsubnode(ast, j));
              push(nast->subnodes, &confirmed);
            }
            for (int j = 0; j < subast->subnodes->size; j++) {
              ASTNode *partial = astsubnode(subast, j);
              int      status  = partial->status;
              if (status == STATUS_CONFIRMED || status == STATUS_REC) {
                rem(subast->subnodes, j);
                if (status == STATUS_CONFIRMED && astempty(partial)) {
                  deleteAST(&partial);
                  nast->rec--;
                } else { push(nast->subnodes, &partial); }
                if (status == STATUS_REC) nast->rec++;
                else                      nast->pos++;
                break;
              }
            }
            if (nast->pos == size)                       { nast->status = STATUS_CONFIRMED; save = nast; }
            else if (flags & ASTFLAGS_REC && 
                     nast->pos == size - 1)              { nast->status = STATUS_REC;       save = nast; }
            else if (c == AST_CLOSE || c == AST_LOCK)    {                                        lim++; }
            else                                         { nc = AST_LOCK; ast = nast;          continue; }
          } else if (subast->status == STATUS_CONFIRMED) {
            if (astempty(subast)) {
              ast->rec--;
              deleteAST(pop(ast->subnodes));
            }
            if (++ast->pos == size)                      { ast->status = STATUS_CONFIRMED;   save = ast; }
            else if (flags & ASTFLAGS_REC &&
                     ast->pos == size - 1)               { ast->status = STATUS_REC;         save = ast; }
            else if (c == AST_CLOSE || c == AST_LOCK)    {                                     continue; }
            else                                         { nc = AST_LOCK;                      continue; }
          } else if (subast->status == STATUS_REC) {
            ++ast->rec;
            if (c == AST_CLOSE || c == AST_LOCK)         {                                     continue; }
            else                                         { nc = AST_LOCK;                      continue; }
          } else if (subast->status == STATUS_SKIP)  {
            ASTNode *nast = newASTNode(superast, bnf);
            push(nast->subnodes, pop(subast->subnodes));
            lim++;
            subast->status = STATUS_NOSTATUS;
          }
          break;
        }
      }
      /////////////////////////// LIST FOOTER ///////////////////////////////////////////
      if (save) {
        if (save->subnodes->size == 1)     astupnode(save, astsubnode(save, 0));
        else if (bnf->type == NODE_CONCAT) astconcatnode(save);
        if (superast->subnodes->size == 1) {
          astupnode(superast, save);
          superast->status = ast->status;
          break;
        } else { superast->status = STATUS_PARTIAL; }
      }
      if (!superast->subnodes->size) superast->status = STATUS_FAILED;
      break;
      ///////////////////////////////////////////////////////////////////////////////////
    case NODE_REC:
    case NODE_ONE_OF:
    /////////////////////////////// ONE OF HEADER ///////////////////////////////////////
      superast = ast;
      if (bnf->refs->size > 1 && flags & ASTFLAGS_FRONT) {
        // SPECIAL CASE OF LEFTMOST DERIVATION (when the recursive element is the first position)
        ASTNode *recroot = *(ASTNode**)at(bnf->refs, 0);
        if (c == AST_CLOSE) { superast->status = STATUS_FAILED; break; }
        if (recroot->status == STATUS_PARTIAL && c == AST_LOCK) {
          recroot->status  = STATUS_ONGOING;
          recroot->pos     = bnf->refs->size;
          ASTNode *partial = *(ASTNode**)pop(recroot->subnodes);
          push(superast->subnodes, &partial);
          superast->status = STATUS_SKIP;
        }
        break;
      }
      if (superast->status == STATUS_FAILED || superast->status == STATUS_CONFIRMED) break;
      if (superast->status == STATUS_NOSTATUS) superast->status = STATUS_ONGOING;
      if (!superast->subnodes->size) ast = newASTNode(superast, bnf);
      else                           ast = astsubnode(superast, 0);
      ast->status = STATUS_ONGOING;
      ///////////////////////////// ONE OF BODY //////////////////////////////////////////
      for (int i = 0; i < size; i++) {
        subast = astsubnode(ast, i);
        subbnf = bnfsubnode(bnf, i);
        if (!subast) subast = newASTNode(ast, NULL);
        if (subast->status != STATUS_FAILED) {
          f |= (flags & ~ASTFLAGS_REC) | (bnf->type == NODE_REC ? ASTFLAGS_REC : 0);
          astnewchar(errors, subast, subbnf, reserved, f, c);
          if (subast->status == STATUS_CONFIRMED)    { ast->status = STATUS_CONFIRMED; save = subast; }
          else if (subast->status == STATUS_PARTIAL) { ast->status = STATUS_PARTIAL;   save = subast; }
          else if (subast->status == STATUS_REC)     { ast->status = STATUS_REC;       save = subast; }
          else if (subast->status == STATUS_FAILED)  { ast->pos++;                                    }
        }
      }
      if ((ast->status == STATUS_CONFIRMED && ast->pos == size - 1) || ast->status == STATUS_REC) {
        superast->status = ast->status;
        astupnode(ast,     save); // for the name
        astupnode(superast, ast);
        break;
      } else if (ast->status == STATUS_CONFIRMED) {
        ASTNode *nast    = newASTNode(NULL, NULL);
        superast->status = STATUS_PARTIAL;
        ast->status      = STATUS_ONGOING;
        nast->status     = STATUS_FAILED;
        set(ast->subnodes, indexof(ast->subnodes, &save), &nast);
        if (superast->subnodes->size > 1) deleteAST(pop(superast->subnodes));
        push(superast->subnodes, &save);
        ast->pos++;
      } else if (ast->status == STATUS_PARTIAL) {
        superast->status = STATUS_PARTIAL;
        ast->status      = STATUS_ONGOING; // STEAL PARTIAL AWAY
        save->status     = STATUS_ONGOING;
        for (int i = 0; i < save->subnodes->size; i++) {
          ASTNode *partial = astsubnode(save, i);
          if (partial->status == STATUS_CONFIRMED || partial->status == STATUS_REC) {
            rem(save->subnodes, i);
            if (!partial->name->length) {
              partial->ref = ast->ref;
              concat(partial->name, newString(ast->name->content));
            }
            if (superast->subnodes->size > 1) deleteAST(pop(superast->subnodes));
            push(superast->subnodes, &partial);
            break;
          }
        }
      } else if (ast->pos >= size) {
        deleteAST(rem(superast->subnodes, 0));
        if (!superast->subnodes->size) superast->status = STATUS_FAILED;
        // in case the AST was partial
        else {
          // Memory leak?
          astupnode(superast, astsubnode(superast, 0));
          superast->status = STATUS_CONFIRMED;
        }
      }
      break;
  }
  pop(bnf->refs);
}

ASTNode *parseast(char *filename)
{
  BNFNode      *bnftree  = parsebnf("parsing/bnf/test.bnf");
  BNFNode      *rootent  = bnfsubnode(bnftree, 0);
  Parser       *parser   = newParser("parsing/prs/csr.prs");
  SymbolStream *ss       = ssopen(filename, parser);
  ASTNode      *ast      = newASTNode(NULL, NULL);
  Array        *reserved = newArray(sizeof(char*));
  Array        *errors   = newArray(sizeof(ASTError));
  Array        *trace    = newArray(sizeof(char*));
  push(trace, &filename);

  Symbol *s;
  while (!(s = ssgets(ss))->eof) {
    int i = 0;
    char c;

    if (s->text[0] == '\n') continue;

    astnewchar(errors, ast, rootent, reserved, ASTFLAGS_NONE, AST_LOCK);
    while ((c = s->text[i++])) {
      astnewchar(errors, ast, rootent, reserved, ASTFLAGS_NONE, c);
    }
    if (ast->status == STATUS_FAILED) {
      printsymbolmessage(ERRLVL_ERROR, trace, s, "Unexpected symbol!");
      break;
    }
  }
  astnewchar(errors, ast, rootent, reserved, ASTFLAGS_NONE, AST_CLOSE);

  deleteArray(&trace);
  deleteArray(&errors);
  deleteArray(&reserved);
  ssclose(ss);
  deleteParser(&parser);
  deleteBNFTree(&bnftree);
  return ast;
}

void deleteAST(ASTNode **node) {
  if (*node) {
    while((*node)->subnodes->size) deleteAST(pop((*node)->subnodes));
    freeastnode(*node);
    free(*node);
    *node = NULL;
  }
}