#include "parser.h"

#include "llvm/Support/Path.h"
#include "llvm/Support/SMLoc.h"
#include "llvm/Support/SourceMgr.h"

namespace grp {
ParserOption
ParserOption::createDefaultOption(const std::string mainInputFile) {
  ParserOption result;
  result.mainInputFile = mainInputFile;
  llvm::StringRef parentPath = llvm::sys::path::parent_path(mainInputFile);
  result.includePaths.emplace_back(parentPath.data(), parentPath.size());
  return result;
}

ParserContext::ParserContext(const ParserOption &option)
    : option(option), fs(llvm::vfs::createPhysicalFileSystem()) {}

CSTParser::CSTParser(ParserContext &context) : context(context) {
  srcMgr.setIncludeDirs(context.getOption().includePaths);
  auto result = context.getFS().getBufferForFile(
      context.getOption().mainInputFile, -1, false);
  if (!result) {
    // FIXME: diag
  }
  lexerStack.emplace_back(**result, context.getIdentifierInterner(), 1);
  srcMgr.AddNewSourceBuffer(std::move(*result), llvm::SMLoc());
  ID_include = context.getIdentifierInterner().get("include");
}

IdentifierCST *CSTParser::parseIdentifierCST() {
  Token id = topLexer().lex();
  assert(id.isIdentifier());
  auto ptr = context.getAllocator().Allocate<IdentifierCST>();
  new (ptr) IdentifierCST(id.getLoc(), id.getID());
  return ptr;
}

StringCST *CSTParser::parseStringCST() {
  Token str = topLexer().lex();
  assert(str.isPlainString());
  auto ptr = context.getAllocator().Allocate<StringCST>();
  new (ptr) StringCST(str.getLoc(), str.getString());
  return ptr;
}

CodeStringCST *CSTParser::parseCodeStringCST() {
  Token str = topLexer().lex();
  assert(str.isCodeString());
  auto ptr = context.getAllocator().Allocate<CodeStringCST>();
  new (ptr) CodeStringCST(str.getLoc(), str.getString());
  return ptr;
}

IntCST *CSTParser::parseIntCST() {
  Token num = topLexer().lex();
  assert(num.isNumber());
  auto ptr = context.getAllocator().Allocate<IntCST>();
  new (ptr) IntCST(num.getLoc(), num.takeNum());
  return ptr;
}

VectorCST *CSTParser::parseVectorCST() {
  Token openBracket = expect(TokenKind::OpenBracket);
  std::vector<CST *> subforms;
  while (true) {
    if (topLexer().peek().getKind() == TokenKind::CloseBracket) {
      topLexer().lex();
      auto ptr = context.getAllocator().Allocate<VectorCST>();
      new (ptr) VectorCST(openBracket.getLoc(), std::move(subforms));
      return ptr;
    }
    auto ptr = parseSubCST();
    if (!ptr) {
      // TODO: diag
    }
    subforms.push_back(ptr);
  }
}

ExpressionCST *CSTParser::parseRawExpressionCST() {
  Token openParen = expect(TokenKind::OpenParen);
  Token machineMode;
  bool first = true;
  std::vector<CST *> subforms;
  while (true) {
    Token peek = topLexer().peek();
    if (peek.getKind() == TokenKind::CloseParen) {
      topLexer().lex();
      auto ptr = context.getAllocator().Allocate<ExpressionCST>();
      new (ptr) ExpressionCST(openParen.getLoc(),
                              machineMode.isValid() ? machineMode.getID() : 0,
                              std::move(subforms));
      return ptr;
    }
    auto ptr = parseSubCST();
    if (!ptr) {
      // TODO: diag
    }
    subforms.push_back(ptr);
    if (first) {
      if (topLexer().peek().getKind() == TokenKind::Colon) {
        topLexer().lex();
        machineMode = expect(TokenKind::Identifier);
      }
      first = false;
    }
  }
}

CST *CSTParser::parseSubCST() {
  Token peek = topLexer().peek();
  switch (peek.getKind()) {
  default:
    return nullptr;
  case TokenKind::Identifier:
    return parseIdentifierCST();
  case TokenKind::String:
    return parseStringCST();
  case TokenKind::CodeString:
    return parseCodeStringCST();
  case TokenKind::Number:
    return parseIntCST();
  case TokenKind::OpenParen:
    return parseRawExpressionCST();
  case TokenKind::OpenBracket:
    return parseVectorCST();
  }
}

void CSTParser::skipEmptyLexers() {
  while (!lexerStack.empty() && lexerStack.back().peek().isEOS()) {
    lexerStack.pop_back();
  }
}

void CSTParser::includeFile(llvm::StringRef path) {
  std::string pathStr(path.data(), path.size());
  std::string realPath;
  // TODO: get SMLoc from a token
  auto loc = llvm::SMLoc::getFromPointer(topLexer().getCurPos());
  unsigned fileID = srcMgr.AddIncludeFile(pathStr, loc, realPath);
  if (fileID) {
    // TODO: diag
  }
  // temporarily, make some noise
  assert(fileID);
  lexerStack.emplace_back(*srcMgr.getMemoryBuffer(fileID),
                          context.getIdentifierInterner(), fileID);
}

ExpressionCST *CSTParser::parseTopCST() {
again:
  skipEmptyLexers();
  if (lexerStack.empty()) {
    return nullptr;
  }
  ExpressionCST *result = parseRawExpressionCST();
  if (result->getLeadID() == ID_include) {
    auto sub = result->getSubforms();
    if (sub.size() != 2 || sub[1]->getKind() != CST_Kind::String) {
      // TODO: diag
    }
    llvm::StringRef includePath = static_cast<StringCST *>(sub[1])->getStr();
    includeFile(includePath);
    goto again;
  }
  return result;
}

} // namespace grp
