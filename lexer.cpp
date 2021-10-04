#include "lexer.h"

#include <utility>

namespace grp {
Token Token::createEOF(const SourceLocation &loc) {
  return Token(TokenKind::EndOfStream, loc);
}

Token Token::createInvalid(const SourceLocation &loc) {
  return Token(TokenKind::Invalid, loc);
}

Token Token::createIdentifier(IdentifierInterner::IDTy ID,
                              const SourceLocation &loc) {
  Token result(TokenKind::Identifier, loc);
  result.id = ID;
  return result;
}

Token Token::createString(llvm::StringRef str, const SourceLocation &loc) {
  Token result(TokenKind::String, loc);
  new (&result.str) llvm::StringRef(str);
  return result;
}

Token Token::createCodeString(llvm::StringRef str, const SourceLocation &loc) {
  Token result(TokenKind::CodeString, loc);
  new (&result.str) llvm::StringRef(str);
  return result;
}

Token Token::createNumber(llvm::APInt &&num, const SourceLocation &loc) {
  Token result(TokenKind::Number, loc);
  new (&result.num) llvm::APInt(std::move(num));
  return result;
}

Token Token::createDelimiter(TokenKind kind, const SourceLocation &loc) {
  return Token(kind, loc);
}

SourceLocation Lexer::getSourceLocation() const {
  return SourceLocation(line, curPos - lineStart + 1, fileID);
}

void Lexer::skipWhiteSpaces() {
  while (hasMoreChars()) {
    char c = *curPos;
    if (isWhileSpace(c)) {
      advancePos();
    } else if (c == ';') {
      advancePos();
      while (true) {
        char c1 = *curPos;
        if (c1 == '\n') {
          advancePos();
          break;
        }
        advancePos();
      }
    } else if (c == '/') {
      // note: no c-style '\\' '\n' escape here
      const char *bufferEnd = buffer.getBufferEnd();
      if (curPos + 1 < bufferEnd) {
        if (curPos[1] == '*') {
          bool foundEnd = false;
          for (curPos += 2; curPos < bufferEnd; ++curPos) {
            if (*curPos == '*' && curPos + 1 < bufferEnd && curPos[1] == '/') {
              foundEnd = true;
              curPos += 2;
              break;
            }
          }
          if (!foundEnd) {
            // FIXME: diag
          }
        } else if (curPos[1] == '/') {
          for (curPos += 2; curPos < bufferEnd; ++curPos) {
            if (*curPos == '\n') {
              ++line;
              ++curPos;
              break;
            }
          }
        }
      }
    } else {
      break;
    }
  }
}

Token Lexer::lexIdentifierImpl() {
  const char *savedPos = curPos;
  advancePos();
  while (hasMoreChars()) {
    char c = *curPos;
    if (canContIdentifier(c)) {
      advancePos();
      continue;
    } else {
      break;
    }
  }
  auto ID = ii.get(llvm::StringRef(savedPos, curPos - savedPos));
  return Token::createIdentifier(ID, getSourceLocation());
}

Token Lexer::lexStringImpl() {
  const char *savedPos = curPos;
  advancePos();
  while (hasMoreChars()) {
    char c = *curPos;
    // TODO: escape
    if (c == '\\') {
      advancePos();
      advancePos();
      continue;
    }
    if (c == '"') {
      break;
    }
    advancePos();
  }
  Token result =
      Token::createString(llvm::StringRef(savedPos + 1, curPos - savedPos - 1),
                          getSourceLocation());
  advancePos();
  return result;
}

Token Lexer::lexCodeStringImpl() {
  const char *savedPos = curPos;
  bool insideString = false;
  bool insideChar = false;
  bool insideLineComment = false;
  bool insideBlockComment = false;
  unsigned int blockNestingLevel = 0;
  while (advanceCodePos()) {
    char c = *curPos;
    if (c == '\\') {
      const char *endPos = buffer.getBufferEnd();
      if (curPos + 1 == endPos) {
        // FIXME: diag
      }
      c = curPos[1];
      ++curPos;
    }
    if (insideLineComment) {
      if (c == '\n') {
        insideLineComment = false;
      }
      continue;
    }
    if (insideBlockComment) {
      if (c == '*') {
        if (advanceCodePos()) {
          char c1 = *curPos;
          if (c1 == '/') {
            insideBlockComment = false;
          }
        }
      }
      continue;
    }
    if (insideString) {
      if (c == '\\') {
        // TODO: escape
        // just eat the '"' following '\\', so that it doesn't interfence with
        // code structure
        if (!advanceCodePos()) {
          // TODO: diag
        }
      } else if (c == '"') {
        insideString = false;
      }
      continue;
    }
    if (insideChar) {
      if (c == '\\') {
        // TODO: escape
        // just eat the '\'' following '\\', so that it doesn't interfence with
        // code structure
        if (!advanceCodePos()) {
          // TODO: diag
        }
      } else if (c == '\'') {
        insideChar = false;
      }
      continue;
    }
    if (c == '/') {
      if (!advanceCodePos()) {
        // TODO: diag
      }
      char c1 = *curPos;
      if (c1 == '/') {
        insideLineComment = true;
      } else if (c1 == '*') {
        insideBlockComment = true;
      } else {
        // possibly ill-formed code, we will ignore the error
      }
      continue;
    }
    if (c == '{') {
      ++blockNestingLevel;
      continue;
    }
    if (c == '}') {
      if (blockNestingLevel) {
        --blockNestingLevel;
      } else {
        break;
      }
    }
    if (c == '"') {
      insideString = true;
    }
    if (c == '\'') {
      insideChar = true;
    }
  }
  Token result = Token::createCodeString(
      llvm::StringRef(savedPos + 1, curPos - savedPos - 1),
      getSourceLocation());
  advancePos();
  return result;
}

Token Lexer::lexNumberImpl() {
  // TODO: octal and hexadecimal number
  const char *savedPos = curPos;
  char c = *curPos;
  bool isNegative = false;
  if (c == '-') {
    isNegative = true;
    advancePos();
    skipWhiteSpaces();
    savedPos = curPos;
  }
  while (hasMoreChars()) {
    char c1 = *curPos;
    if (!isdigit(c1)) {
      break;
    }
    advancePos();
  }
  if (savedPos == curPos) {
    // FIXME: diag
  }
  llvm::StringRef str(savedPos, curPos - savedPos);
  unsigned bitsNeeded = llvm::APInt::getBitsNeeded(str, 10);
  llvm::APInt num(bitsNeeded, str, 10);
  if (isNegative) {
    num.negate();
  }
  return Token::createNumber(std::move(num), getSourceLocation());
}

Token Lexer::peek() {
  if (!lookahead) {
    lookahead = lex();
  }
  return *lookahead;
}

Token Lexer::lex() {
  if (lookahead) {
    return *std::exchange(lookahead, std::nullopt);
  }
  skipWhiteSpaces();
  if (curPos == buffer.getBufferEnd()) {
    return Token::createEOF(getSourceLocation());
  }
  char c = *curPos;
  switch (c) {
  case '{':
    return lexCodeStringImpl();
  case '"':
    return lexStringImpl();
  case '(': {
    ++curPos;
    skipWhiteSpaces();
    if (!hasMoreChars()) {
      // FIXME: diag
    }
    char c1 = *curPos;
    if (c1 == '"') {
      Token result = lexStringImpl();
      skipWhiteSpaces();
      if (!hasMoreChars()) {
        // FIXME: diag
      }
      char c2 = *curPos;
      if (c2 != ')') {
        // FIXME: diag
      }
      ++curPos;
      return result;
    }
    return Token::createDelimiter(TokenKind::OpenParen, getSourceLocation());
  }
  case ')':
    ++curPos;
    return Token::createDelimiter(TokenKind::CloseParen, getSourceLocation());
  case '[':
    ++curPos;
    return Token::createDelimiter(TokenKind::OpenBracket, getSourceLocation());
  case ']':
    ++curPos;
    return Token::createDelimiter(TokenKind::CloseBracket, getSourceLocation());
  case ':':
    ++curPos;
    return Token::createDelimiter(TokenKind::Colon, getSourceLocation());
  default:
    if (canStartIdentifier(c)) {
      return lexIdentifierImpl();
    }
    if (canStartNumber(c)) {
      return lexNumberImpl();
    }
  }
  assert(false && "unknown how to handle character when lexing");
  advancePos();
  return Token::createInvalid(getSourceLocation());
}
} // namespace grp
