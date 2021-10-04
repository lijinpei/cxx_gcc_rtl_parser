#pragma once

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"

#include <cctype>
#include <cstdint>
#include <optional>

namespace grp {

inline bool isWhileSpace(char c) {
  // TODO: '\r'
  return isspace(c);
}

inline bool canStartIdentifier(char c) {
  return c == '?' || c == '<' || c == '_' || c == '$' || isalpha(c);
}

inline bool canContIdentifier(char c) {
  return c == '*' || c == ':' || c == '>' || canStartIdentifier(c) ||
         isdigit(c);
}

inline bool canStartNumber(char c) {
  // FIXME: only decimial digit now
  return c == '-' || isdigit(c);
}

// FIXME: more sophisticated source location implementation, currently this is
// enough
class SourceLocation {
  // line count and columnt count starts with 1
  // 0 means invalid value
  uint64_t line, column;
  // as known to llvm::SourceMge
  unsigned fileID;

public:
  // 0 is a invalid fileID for SourceMgr
  SourceLocation() : line(0), column(0), fileID(0) {}
  SourceLocation(uint64_t line, uint64_t column, unsigned fileID)
      : line(line), column(column), fileID(fileID) {}
  uint64_t getLine() const { return line; }
  uint64_t getColumn() const { return column; }
  unsigned getFileID() const { return fileID; }
};

enum class TokenKind {
  Invalid,
  Identifier,
  String,
  CodeString,
  Number,
  OpenParen,
  CloseParen,
  OpenBracket,
  CloseBracket,
  Colon,
  EndOfStream
};

class IdentifierInterner {
public:
  using IDTy = uint64_t;
  enum : uint64_t { InvalidID = 0 };

private:
  IDTy lastID = InvalidID;
  llvm::DenseMap<llvm::StringRef, IDTy> internedIDs;

public:
  IDTy get(llvm::StringRef str) {
    auto &val = internedIDs[str];
    if (val == InvalidID) {
      return val = ++lastID;
    } else {
      return val;
    }
  }
};

using IDTy = IdentifierInterner::IDTy;

class Token {
private:
  void destructContent() {
    if (kind == TokenKind::String) {
      str.~StringRef();
    } else if (kind == TokenKind::Number) {
      num.~APInt();
    }
  }
  void constructContentFrom(const Token &other) {
    switch (other.kind) {
    default:
      assert(false && "unknown token kind");
    case TokenKind::Identifier:
      id = other.id;
      break;
    case TokenKind::String:
    case TokenKind::CodeString:
      new (&str) llvm::StringRef(other.str);
      break;
    case TokenKind::Number:
      new (&num) llvm::APInt(other.num);
    case TokenKind::EndOfStream:
    case TokenKind::OpenParen:
    case TokenKind::CloseParen:
    case TokenKind::OpenBracket:
    case TokenKind::CloseBracket:
    case TokenKind::Colon:
      break;
    }
  }
  TokenKind kind;
  union {
    int placeHolder;
    IdentifierInterner::IDTy id;
    llvm::StringRef str;
    llvm::APInt num;
  };
  SourceLocation loc;

public:
  Token() : kind(TokenKind::Invalid), loc() {}
  Token(TokenKind kind, const SourceLocation &loc)
      : kind(kind), placeHolder(0), loc(loc) {}
  ~Token() { destructContent(); }
  Token(const Token &other) : kind(other.kind), loc(other.loc) {
    constructContentFrom(other);
  }
  Token &operator=(const Token &other) {
    destructContent();
    kind = other.kind;
    loc = other.loc;
    constructContentFrom(other);
    return *this;
  }
  TokenKind getKind() const { return kind; }
  bool isValid() const { return kind != TokenKind::Invalid; }
  bool isAnyString() const {
    return kind == TokenKind::String || kind == TokenKind::CodeString;
  }
  bool isPlainString() const { return kind == TokenKind::String; }
  bool isCodeString() const { return kind == TokenKind::CodeString; }
  bool isIdentifier() const { return kind == TokenKind::Identifier; }
  bool isNumber() const { return kind == TokenKind::Number; }
  bool isEOS() const { return kind == TokenKind::EndOfStream; }
  IdentifierInterner::IDTy getID() const {
    assert(kind == TokenKind::Identifier);
    return id;
  }
  llvm::StringRef getString() const {
    assert(kind == TokenKind::String || kind == TokenKind::CodeString);
    return str;
  }
  const llvm::APInt &getNum() const {
    assert(kind == TokenKind::Number);
    return num;
  }
  llvm::APInt &&takeNum() { return std::move(num); }
  const SourceLocation &getLoc() const { return loc; }
  static Token createEOF(const SourceLocation &loc);
  static Token createInvalid(const SourceLocation &loc);
  static Token createIdentifier(IdentifierInterner::IDTy ID,
                                const SourceLocation &loc);
  static Token createString(llvm::StringRef str, const SourceLocation &loc);
  static Token createCodeString(llvm::StringRef str, const SourceLocation &loc);
  static Token createNumber(llvm::APInt &&num, const SourceLocation &loc);
  static Token createDelimiter(TokenKind kind, const SourceLocation &loc);
};

// For RTL, `include` is handled on the parser level, so this class only a
// single file
class Lexer {
  const llvm::MemoryBuffer &buffer;
  IdentifierInterner &ii;
  // as known to llvm::SourceMgr
  unsigned fileID;
  int64_t line;
  const char *curPos;
  const char *lineStart;
  std::optional<Token> lookahead;
  bool hasMoreChars() const { return curPos < buffer.getBufferEnd(); }
  void skipWhiteSpaces();
  void advancePos() {
    // TODO: C-style escape-newline ???
    char c = *curPos;
    ++curPos;
    if (c == '\n') {
      ++line;
      lineStart = curPos;
    }
  }
  // return if we have more characters to look-at
  bool advanceCodePos() {
    char c = *curPos;
    ++curPos;
    if (c == '\n') {
      ++line;
    }
    if (!hasMoreChars()) {
      return false;
    }
    c = *curPos;
    if (c == '\\') {
      const char *pos1 = curPos + 1;
      const char *posEnd = buffer.getBufferEnd();
      while (pos1 < posEnd) {
        char c1 = *pos1;
        if (c1 == '\n') {
          curPos = pos1 + 1;
          ++line;
          return hasMoreChars();
        }
        if (!isspace(c1)) {
          break;
        }
      }
    }
    return true;
  }
  Token lexIdentifierImpl();
  Token lexStringImpl();
  Token lexCodeStringImpl();
  Token lexNumberImpl();

public:
  Lexer(const llvm::MemoryBuffer &buffer, IdentifierInterner &ii,
        unsigned fileID)
      : buffer(buffer), ii(ii), fileID(fileID), line(0),
        curPos(buffer.getBufferStart()), lineStart(curPos),
        lookahead(std::nullopt) {}
  SourceLocation getSourceLocation() const;
  Token lex();
  Token peek();
  const char *getCurPos() const { return curPos; }
};
} // namespace grp
