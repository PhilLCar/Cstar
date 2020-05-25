#include <ast.h>

ASTNode *astsubnode(ASTNode*, int);

ASTNode *newASTNode(ASTNode *ast, BNFNode *bnf)
{
  ASTNode *new = malloc(sizeof(ASTNode));
  if (new) {
    new->name     = newString(bnf ? bnf->name : "");
    new->ref      = bnf;
    new->subnodes = newArray(sizeof(ASTNode*));
    new->value    = newString("");
    new->symbol   = NULL;
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
    new->symbol   = ast->symbol ? newSymbol(ast->symbol) : NULL;
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

void freeastnode(ASTNode *node)
{
  deleteString(&node->name);
  deleteString(&node->value);
  deleteArray(&node->subnodes);
  deleteSymbol(&node->symbol);
}

void revertAST(ASTNode **node, Stream *s) {
  if (*node) {
    while((*node)->subnodes->size) revertAST(pop((*node)->subnodes), s);
    if ((*node)->symbol) push(s->stack, &(*node)->symbol);
    (*node)->symbol = NULL;
    freeastnode(*node);
    free(*node);
    *node = NULL;
  }
}

ASTNode *astsubnode(ASTNode *ast, int index)
{
  ASTNode **ret = (ASTNode**)at(ast->subnodes, index);
  if (ret) return *ret;
  return NULL;
}

int astempty(ASTNode *ast) {
  return !ast->subnodes->size && !ast->value->length;
}

BNFNode *bnfsubnode(BNFNode *bnf, int index)
{
  return *(BNFNode**)at(bnf->content, index);
}

void astconcatnode(ASTNode *c)
{
  while(c->subnodes->size) {
    ASTNode *n = *(ASTNode**)rem(c->subnodes, 0);
    concat(c->value, newString(n->value->content));
    deleteAST(&n);
  }
}

void astupnode(ASTNode *super, ASTNode *sub)
{
  concat(super->value, newString(sub->value->content));
  if (!super->name->content[0]) {
    super->ref = sub->ref;
    concat(super->name, newString(sub->name->content));
  } else if (sub->name->length) {
    String *tmp = super->name;
    super->name = sub->name;
    sub->name = tmp;
  }
  Array *tmp = sub->subnodes;
  sub->subnodes = newArray(sizeof(ASTNode*));
  while (super->subnodes->size) deleteAST(pop(super->subnodes));
  deleteArray(&super->subnodes);
  super->subnodes = tmp;
}

void asterror(Array *errors, ASTErrorType errno, BNFNode *bnf)
{
  ASTError error;
  error.errno  = errno;
  error.bnfref = bnf;
  push(errors, &error);
}

void astnewchar(ASTNode *ast, BNFNode *bnf, ASTFlags flags, char c)
{
  ASTNode  *superast;
  ASTNode  *subast;
  ASTNode  *save    = NULL;
  BNFNode  *subbnf;
  int       size    = 0;
  int       f       = 0;
  char     *content = bnf->content;
  if (bnf->type != NODE_LEAF && bnf->type != NODE_RAW) size = ((Array*)bnf->content)->size;
  push(bnf->refs, &ast);

  switch (bnf->type) {
    case NODE_ROOT:
    case NODE_RAW:
    case NODE_MANY_OR_NONE:
    case NODE_MANY_OR_ONE:
    case NODE_ONE_OR_NONE:
    case NODE_NOT:
      /// UNIMPLEMENTED
      break;
    case NODE_LEAF:
      if (c == AST_LOCK) {
        if (!content || ast->status == STATUS_CONFIRMED) { ast->status = STATUS_CONFIRMED; break; }
        else if (!ast->pos && (flags & ASTFLAGS_STARTED
                            || flags & ASTFLAGS_END))    { ast->status = STATUS_FAILED;    break; }
        else if (!ast->pos)                              { ast->status = STATUS_ONGOING;   break; }
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
        if (c != AST_LOCK) { 
          while (superast->subnodes->size) deleteAST(pop(superast->subnodes));
          superast->status = STATUS_FAILED;
        }
        break;
      }
      if (!superast->subnodes->size) {
        ast = newASTNode(superast, bnf);
        ast->status = STATUS_ONGOING;
      }
      if (bnf->type == NODE_CONCAT) {
        if (c == AST_LOCK)       { if (superast->pos) superast->pos++; }
        else if (!superast->pos) {                    superast->pos++; }
      }
      for (int i = 0; i < superast->subnodes->size; i++) {
        ast = astsubnode(superast, i);
        ///////////////////////////// LIST BODY ////////////////////////////////////////////
        for (char nc = c;;) {
          if (ast->pos >= size) {
            ast->status = STATUS_CONFIRMED;
            save = ast;
            break;
          } else if (ast->pos >= size - 1 && flags & ASTFLAGS_REC) {
            ast->status = STATUS_REC;
            save = ast;
            break;
          } else if (ast->status == STATUS_NOSTATUS) {
            ast->status = STATUS_ONGOING;
            nc = AST_LOCK;
          }
          subast = astsubnode(ast, ast->pos + ast->rec);
          subbnf = bnfsubnode(bnf, ast->pos);
          if (!subast) subast = newASTNode(ast, NULL);
          f  = 0;
          f |= (flags & ~ASTFLAGS_REC & ~ASTFLAGS_FRONT) | (bnf->type == NODE_CONCAT ? ASTFLAGS_CONCAT : 0);
          f |= superast->pos > 1 ? ASTFLAGS_STARTED : 0;
          f |= !ast->pos ? ASTFLAGS_FRONT : 0;
          astnewchar(subast, subbnf, f, nc);
          if (subast->status == STATUS_FAILED) {
            deleteAST(rem(superast->subnodes, i--));
          } else if (subast->status == STATUS_PARTIAL) {
            ASTNode *nast = newASTNode(superast, bnf);
            subast->status = STATUS_ONGOING; // STEAL PARTIAL AWAY
            nast->status   = STATUS_NOSTATUS;
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
                if (!subast->rec) rem(subast->subnodes, j);
                else partial = duplicateAST(partial);
                if (status == STATUS_CONFIRMED && astempty(partial)) {
                  deleteAST(&partial);
                  nast->rec--;
                } else { push(nast->subnodes, &partial); }
                if (status == STATUS_REC) nast->rec++;
                else                      nast->pos++;
                break;
              }
            }
          } else if (subast->status == STATUS_CONFIRMED) {
            if (astempty(subast)) {
              ast->rec--;
              deleteAST(pop(ast->subnodes));
            }
            ++ast->pos;
            nc = AST_LOCK;
            continue;
          } else if (subast->status == STATUS_REC) {
            ++ast->rec;
            nc = AST_LOCK;
            continue;
          } else if (subast->status == STATUS_SKIP)  {
            ASTNode *nast = newASTNode(superast, bnf);
            push(nast->subnodes, pop(subast->subnodes));
            subast->status = STATUS_ONGOING;
            nc = AST_LOCK;
            continue;
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
    case NODE_ANON:
    /////////////////////////////// ONE OF HEADER ///////////////////////////////////////
      superast = ast;
      if (bnf->refs->size > 1 && flags & ASTFLAGS_FRONT) {
        // SPECIAL CASE OF LEFTMOST DERIVATION (when the recursive element is the first position)
        ASTNode *recroot = *(ASTNode**)at(bnf->refs, bnf->refs->size - 2);
        recroot->rec = 1;
        if (flags & ASTFLAGS_END) { superast->status = STATUS_FAILED; break; }
        if (recroot->status == STATUS_PARTIAL && c == AST_LOCK) {
          recroot->status  = STATUS_ONGOING;
          recroot->pos     = bnf->refs->size;
          ASTNode *partial = *(ASTNode**)pop(recroot->subnodes);
          push(superast->subnodes, &partial);
          superast->status = STATUS_SKIP;
        }
        break;
      }
      if (superast->status == STATUS_FAILED) break;
      if (superast->status == STATUS_NOSTATUS) superast->status = STATUS_ONGOING;
      if (superast->status == STATUS_CONFIRMED) {
        if (c != AST_LOCK) { 
          while (superast->subnodes->size) deleteAST(pop(superast->subnodes));
          superast->status = STATUS_FAILED;
        }
        break;
      }
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
          astnewchar(subast, subbnf, f, c);
          if (subast->status == STATUS_CONFIRMED)    { ast->status = STATUS_CONFIRMED; save = subast; }
          else if (subast->status == STATUS_PARTIAL) { ast->status = STATUS_PARTIAL;   save = subast; }
          else if (subast->status == STATUS_REC)     { ast->status = STATUS_REC;       save = subast; }
          else if (subast->status == STATUS_FAILED)  { ast->pos++;                                    }
        }
      }
      if (c != AST_LOCK && superast->subnodes->size > 1) deleteAST(pop(superast->subnodes));
      if ((ast->status == STATUS_CONFIRMED && ast->pos == size - 1) || ast->status == STATUS_REC) {
        superast->status = ast->status;
        if (bnf->type == NODE_ANON) {
          astupnode(superast, save);
        } else {
          astupnode(ast,     save); // for the name
          astupnode(superast, ast);
        }
        break;
      } else if (ast->status == STATUS_CONFIRMED) {
        ASTNode *nast    = newASTNode(NULL, NULL);
        superast->status = STATUS_PARTIAL;
        ast->status      = STATUS_ONGOING;
        nast->status     = STATUS_FAILED;
        set(ast->subnodes, indexof(ast->subnodes, &save), &nast);
        if (!save->name->length && bnf->type != NODE_ANON) concat(save->name, newString(ast->name->content));
        push(superast->subnodes, &save);
        ast->pos++;
      } else if (ast->status == STATUS_PARTIAL) {
        superast->status = STATUS_PARTIAL;
        ast->status      = STATUS_ONGOING; // STEAL PARTIAL AWAY
        save->status     = STATUS_ONGOING;
        for (int i = 0; i < save->subnodes->size; i++) {
          ASTNode *partial = astsubnode(save, i);
          if (partial->status == STATUS_CONFIRMED || partial->status == STATUS_REC) {
            if (!save->rec) rem(save->subnodes, i);
            else partial = duplicateAST(partial);
            if (!partial->name->length && bnf->type != NODE_ANON) {
              partial->ref = ast->ref;
              concat(partial->name, newString(ast->name->content));
            }
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

void astnewsymbol(ASTNode *ast, BNFNode *bnf, ASTFlags flags, Symbol *s)
{
  ASTNode  *superast;
  ASTNode  *subast;
  ASTNode  *save    = NULL;
  BNFNode  *subbnf;
  int       size    = 0;
  int       f       = 0;
  char     *content = bnf->content;
  if (bnf->type != NODE_LEAF && bnf->type != NODE_RAW) size = ((Array*)bnf->content)->size;


  if      (ast->status == STATUS_FAILED) return;
  if      (ast->status == STATUS_NOSTATUS) ast->status = STATUS_ONGOING;
  else if (ast->status == STATUS_CONFIRMED) {
    if (s){
      while (ast->subnodes->size) deleteAST(pop(ast->subnodes));
      ast->status = STATUS_FAILED;
    }
    return;
  }

  push(bnf->refs, &ast);
  switch (bnf->type) {
    case NODE_ROOT:
    case NODE_MANY_OR_NONE:
    case NODE_MANY_OR_ONE:
    case NODE_ONE_OR_NONE:
    case NODE_CONCAT:
      /// UNIMPLEMENTED
      break;
    case NODE_NOT:
      subast = astsubnode(ast, 0);
      subbnf = bnfsubnode(bnf, 0);
      if (!subast) subast = newASTNode(ast, NULL);
      astnewsymbol(subast, subbnf, flags, s);
      if (subast->status == STATUS_CONFIRMED || subast->status == STATUS_REC) {
        if (subast->ref == bnfsubnode(bnf, 1)) {
          ast->status = STATUS_FAILED;
          deleteAST(pop(ast->subnodes));
        } else {
          ast->status = STATUS_CONFIRMED;
          astupnode(ast, subast);
        }
      } else if (subast->status == STATUS_PARTIAL) {
        subast->status = STATUS_ONGOING;
        for (int i = 0; i < subast->subnodes->size; i++) {
          ASTNode *partial = astsubnode(subast, i);
          if (partial->status == STATUS_CONFIRMED || partial->status == STATUS_REC) {
            if (partial->ref == bnfsubnode(bnf, 1)) deleteAST(rem(subast->subnodes, i));
            else { ast->status = STATUS_PARTIAL; push(ast->subnodes, rem(subast->subnodes, i)); }
            break;
          }
        }
      } else if (subast->status == STATUS_FAILED) {
        ast->status = STATUS_FAILED;
        deleteAST(pop(ast->subnodes));
      } else {
        ast->status = subast->status;
      }
      break;
    case NODE_RAW:
      if (s) {
        if (s->type == SYMBOL_NEWLINE && s->type != (SymbolType)bnf->content) {
          ast->status = STATUS_ONGOING;
        } else if (s->type == (SymbolType)bnf->content) {
          concat(ast->name,  newString(bnf->name));
          concat(ast->value, newString(s->text));
          ast->status = STATUS_CONFIRMED;
        } else ast->status = STATUS_FAILED;
      } else {
        if (flags & ASTFLAGS_END) ast->status = STATUS_FAILED;
        else                      ast->status = STATUS_ONGOING;
      }
      break;
    case NODE_LEAF:
      if (s) {
        if (s->type == SYMBOL_NEWLINE) {                 ast->status = STATUS_ONGOING;   }
        else if (content) {
          if (strcmp(s->text, content)) {                ast->status = STATUS_FAILED;    } 
          else { concat(ast->value, newString(content)); ast->status = STATUS_CONFIRMED; }
        }
      } else {
        if (content && ast->status != STATUS_CONFIRMED) {
          if (flags & ASTFLAGS_END) ast->status = STATUS_FAILED;
          else                      ast->status = STATUS_ONGOING;
        } else                      ast->status = STATUS_CONFIRMED;
      }
      break;
    case NODE_LIST:
    //////////////////////////////// LIST HEADER /////////////////////////////////////////
      superast = ast;
      if (!superast->subnodes->size) {
        ast = newASTNode(superast, bnf);
        ast->status = STATUS_ONGOING;
      }
      for (int i = 0; i < superast->subnodes->size; i++) {
        ast = astsubnode(superast, i);
        ///////////////////////////// LIST BODY ////////////////////////////////////////////
        for (Symbol *ns = s;;) {
          if (ast->pos >= size) {
            ast->status = STATUS_CONFIRMED;
            save = ast;
            break;
          } else if (ast->pos >= size - 1 && flags & ASTFLAGS_REC) {
            ast->status = STATUS_REC;
            save = ast;
            break;
          } else if (ast->status == STATUS_NOSTATUS) {
            ast->status = STATUS_ONGOING;
            ns = NULL;
          }
          subast = astsubnode(ast, ast->pos + ast->rec);
          subbnf = bnfsubnode(bnf, ast->pos);
          if (!subast) subast = newASTNode(ast, NULL);
          f = 0;
          f |= flags & ~ASTFLAGS_REC & ~ASTFLAGS_FRONT;
          if (flags & ASTFLAGS_REC) f |= !ast->pos ? ASTFLAGS_FRONT & flags : 0;
          else                      f |= !ast->pos ? ASTFLAGS_FRONT : 0;
          astnewsymbol(subast, subbnf, f, ns);
          if (subast->status == STATUS_FAILED) {
            deleteAST(rem(superast->subnodes, i--)); break;
          } else if (subast->status == STATUS_PARTIAL) {
            ASTNode *nast = newASTNode(superast, bnf);
            subast->status = STATUS_ONGOING; // STEAL PARTIAL AWAY
            nast->status   = STATUS_NOSTATUS;
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
                if (!subast->rec) rem(subast->subnodes, j);
                else partial = duplicateAST(partial);
                if (status == STATUS_CONFIRMED && astempty(partial)) {
                  deleteAST(&partial);
                  nast->rec--;
                } else { push(nast->subnodes, &partial); }
                if (status == STATUS_REC) nast->rec++;
                else                      nast->pos++;
                break;
              }
            }
          } else if (subast->status == STATUS_CONFIRMED) {
            if (astempty(subast)) {
              ast->rec--;
              deleteAST(pop(ast->subnodes));
            }
            ++ast->pos;
          } else if (subast->status == STATUS_REC) {
            ++ast->rec;
          } else if (subast->status == STATUS_SKIP)  {
            ASTNode *nast = newASTNode(superast, bnf);
            push(nast->subnodes, pop(subast->subnodes));
            subast->status = STATUS_ONGOING;
          } else if (!ns) break;
          if (ns) ns = NULL;
        }
      }
      /////////////////////////// LIST FOOTER ///////////////////////////////////////////
      if (save) {
        if (save->subnodes->size == 1) astupnode(save, astsubnode(save, 0));
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
    case NODE_ANON:
    /////////////////////////////// ONE OF HEADER ///////////////////////////////////////
      superast = ast;
      if (bnf->refs->size > 1 && flags & ASTFLAGS_FRONT) {
        // SPECIAL CASE OF LEFTMOST DERIVATION (when the recursive element is the first position)
        ASTNode *recroot = *(ASTNode**)at(bnf->refs, bnf->refs->size - 2);
        recroot->rec = 1;
        if (flags & ASTFLAGS_END) { superast->status = STATUS_FAILED; break; }
        if (recroot->subnodes->size > 1 && !s) {
          recroot->status  = STATUS_ONGOING;
          recroot->pos     = bnf->refs->size;
          ASTNode *partial = *(ASTNode**)pop(recroot->subnodes);
          push(superast->subnodes, &partial);
          superast->status = STATUS_SKIP;
        }
        break;
      }
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
          astnewsymbol(subast, subbnf, f, s);
          if (subast->status == STATUS_CONFIRMED)    { ast->status = STATUS_CONFIRMED; save = subast; }
          else if (subast->status == STATUS_PARTIAL) { ast->status = STATUS_PARTIAL;   save = subast; }
          else if (subast->status == STATUS_REC)     { ast->status = STATUS_REC;       save = subast; }
          else if (subast->status == STATUS_FAILED)  { ast->pos++;                                    }
        }
      }
      if (s && superast->subnodes->size > 1) deleteAST(pop(superast->subnodes));
      if ((ast->status == STATUS_CONFIRMED && ast->pos == size - 1) || ast->status == STATUS_REC) {
        superast->status = ast->status;
        if (bnf->type == NODE_ANON) {
          astupnode(superast, save);
        } else {
          astupnode(ast, save);
          astupnode(superast, ast);
        }
        break;
      } else if (ast->status == STATUS_CONFIRMED) {
        ASTNode *nast    = newASTNode(NULL, NULL);
        superast->status = STATUS_PARTIAL;
        ast->status      = STATUS_ONGOING;
        nast->status     = STATUS_FAILED;
        set(ast->subnodes, indexof(ast->subnodes, &save), &nast);
        if (!save->name->length && bnf->type != NODE_ANON) concat(save->name, newString(ast->name->content));
        push(superast->subnodes, &save);
        ast->pos++;
      } else if (ast->status == STATUS_PARTIAL) {
        superast->status = STATUS_PARTIAL;
        ast->status      = STATUS_ONGOING; // STEAL PARTIAL AWAY
        save->status     = STATUS_ONGOING;
        for (int i = 0; i < save->subnodes->size; i++) {
          ASTNode *partial = astsubnode(save, i);
          if (partial->status == STATUS_CONFIRMED || partial->status == STATUS_REC) {
            if (!save->rec) rem(save->subnodes, i);
            else partial = duplicateAST(partial);
            if (!partial->name->length && bnf->type != NODE_ANON) {
              partial->ref = ast->ref;
              concat(partial->name, newString(ast->name->content));
            }
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

void astparsestream(ASTNode *ast, BNFNode *bnf, Array *rejected, Stream *s)
{
  ASTNode  *subast;
  BNFNode  *subbnf;
  Symbol   *symbol  = s->symbol;
  int       size    = 0;
  char     *content = bnf->content;
  if (bnf->type != NODE_LEAF && bnf->type != NODE_RAW) size = ((Array*)bnf->content)->size;
  if (!symbol->text) { 
    while (s->gets(s->stream) && (symbol->type == SYMBOL_COMMENT || symbol->type == SYMBOL_NEWLINE));
  }

  switch (bnf->type) {
    case NODE_ROOT:
    case NODE_REC:
    case NODE_CONCAT:
      /// UNIMPLEMENTED
      break;
    case NODE_NOT:
      /// UNIMPLEMENTED
      break;
    case NODE_RAW:
      if (symbol->type == (SymbolType)bnf->content) {
        deleteString(&ast->name);
        deleteString(&ast->value);
        ast->name   = newString(bnf->name);
        ast->value  = newString(symbol->text);
        ast->symbol = newSymbol(symbol);
        ast->status = STATUS_CONFIRMED;
      } else ast->status = STATUS_FAILED;
      break;
    case NODE_LEAF:
      if (content) {
        if (strcmp(symbol->text, content)) { ast->status = STATUS_FAILED; } 
        else {
          deleteString(&ast->value);
          ast->value = newString(content);
          ast->symbol = newSymbol(symbol);
          ast->status = STATUS_CONFIRMED; 
        }
      } else { ast->status = STATUS_NULL; }
      break;
    case NODE_LIST:
      for (int i = 0; i < size; i++) {
        subbnf = *(BNFNode**)at(bnf->content, i);
        subast = newASTNode(ast, subbnf);
        astparsestream(subast, subbnf, rejected, s);
        if (subast->status == STATUS_CONFIRMED) {
          if (i == size - 1) break;
          while (s->gets(s->stream) && (symbol->type == SYMBOL_COMMENT || symbol->type == SYMBOL_NEWLINE));
          continue;
        } else if (subast->status == STATUS_NULL) {
          continue;
        } else if (subast->status == STATUS_FAILED) {
          ast->status = STATUS_FAILED;
          while (ast->subnodes->size) revertAST(pop(ast->subnodes), s);
          break;
        }
      }
      if (ast->status != STATUS_FAILED) ast->status = subast->status;
      break;
    case NODE_ONE_OF:
    case NODE_ANON:
    case NODE_ONE_OR_NONE:
      subast = newASTNode(ast, NULL);
      for (int i = 0; i < size; i++) {
        subbnf = *(BNFNode**)at(bnf->content, i);
        astparsestream(subast, subbnf, rejected, s);
        if (subast->status == STATUS_CONFIRMED) {
          ast->status = STATUS_CONFIRMED;
          break;
        } else if (subast->status == STATUS_NULL) {
          ast->status = STATUS_NULL;
        }
      }
      if (ast->status != STATUS_NULL && ast->status != STATUS_CONFIRMED) {
        deleteAST(pop(ast->subnodes));
        if (bnf->type == NODE_ONE_OR_NONE) ast->status = STATUS_NULL;
        else                               ast->status = STATUS_FAILED;
      }
      break;
    case NODE_MANY_OR_NONE:
    case NODE_MANY_OR_ONE:
      do {
        ast->status = STATUS_NOSTATUS;
        subast = newASTNode(ast, NULL);
        for (int i = 0; i < size; i++) {
          subbnf = *(BNFNode**)at(bnf->content, i);
          astparsestream(subast, subbnf, rejected, s);
          if (subast->status == STATUS_CONFIRMED) {
            ast->status = STATUS_CONFIRMED;
            break;
          } else if (subast->status == STATUS_NULL) {
            ast->status = STATUS_NULL;
          }
        }
        if (ast->status != STATUS_NULL && ast->status != STATUS_CONFIRMED) ast->status = STATUS_FAILED;
        if (ast->status == STATUS_CONFIRMED) {
          while (s->gets(s->stream) && (symbol->type == SYMBOL_COMMENT || symbol->type == SYMBOL_NEWLINE));
        }
      } while (ast->status == STATUS_CONFIRMED);
      deleteAST(pop(ast->subnodes));
      if (!ast->subnodes->size && bnf->type == NODE_MANY_OR_ONE) ast->status = STATUS_FAILED;
      else                                                       ast->status = STATUS_NULL;
      break;
  }
}

int isopening(Symbol *symbol, Parser *parser)
{
  for (int i = 0; parser->delimiters[i]; i++) {
    if (!strcmp(symbol->text, parser->delimiters[i])) {
      return (i + 1) % 2;
    }
  }
  return 0;
}

ASTNode *parseast(char *filename)
{
  BNFNode      *bnftree  = parsebnf("parsing/bnf/test.bnf");
  BNFNode      *rootent  = bnfsubnode(bnftree, 0);
  Parser       *parser   = newParser("parsing/prs/csr.prs");
  Stream       *s        = getStreamSS(ssopen(filename, parser));
  ASTNode      *ast      = newASTNode(NULL, NULL);
  Array        *trace    = newArray(sizeof(char*));
  push(trace, &filename);

  astparsestream(ast, rootent, NULL, s);

  deleteArray(&trace);
  closeStream(s);
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