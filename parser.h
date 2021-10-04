#pragma once

#include "cst.h"
#include "lexer.h"

#include "llvm/Support/Allocator.h"
#include "llvm/Support/VirtualFileSystem.h"

#include <vector>

namespace grp {

struct ParserOption {
  std::string mainInputFile;
  std::vector<std::string> includePaths;
  static ParserOption createDefaultOption(const std::string mainInputFile);
};

class ParserContext {
  ParserOption option;
  std::unique_ptr<llvm::vfs::FileSystem> fs;
  IdentifierInterner ii;
  llvm::BumpPtrAllocator alloc;

public:
  ParserContext(const ParserOption &option);
  const ParserOption &getOption() const { return option; }
  llvm::vfs::FileSystem &getFS() const { return *fs.get(); }
  IdentifierInterner &getIdentifierInterner() { return ii; }
  llvm::BumpPtrAllocator &getAllocator() { return alloc; }
};

class CSTParser {
  ParserContext &context;
  llvm::SourceMgr srcMgr;
  // note: SourceMgr has a stack of included file, we keep the corresponding
  // Lexers
  std::vector<Lexer> lexerStack;
  IDTy ID_include;
  Lexer &topLexer() { return lexerStack.back(); }
  void skipEmptyLexers();
  Token expect(TokenKind kind) {
    Token result = topLexer().lex();
    if (result.getKind() != kind) {
      // TODO: diag
    }
    return result;
  }
  void includeFile(llvm::StringRef path);

public:
  CSTParser(ParserContext &context);
  CST *parseSubCST();
  // parse an expression without handling of include
  ExpressionCST *parseRawExpressionCST();
  IdentifierCST *parseIdentifierCST();
  StringCST *parseStringCST();
  CodeStringCST *parseCodeStringCST();
  IntCST *parseIntCST();
  VectorCST *parseVectorCST();
  // parse a top-level CST(which must be an expression), include's are handled
  ExpressionCST *parseTopCST();
};
} // namespace grp
