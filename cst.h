#pragma once

#include "lexer.h"
#include "machine_mode.h"

#include <memory>
#include <vector>

namespace grp {

enum class CST_Kind {
  Invalid,
  Expression,
  Identifier,
  Int,
  HostInt,
  String,
  CodeString,
  Vector,
  EndOfStream,
};

class CST {
  CST_Kind kind;
  SourceLocation loc;

public:
  CST(CST_Kind kind, const SourceLocation &loc) : kind(kind), loc(loc) {}
  CST_Kind getKind() const { return kind; }
  const SourceLocation &getLoc() const { return loc; }
  bool isInvalid() const { return kind == CST_Kind::Invalid; }
};

class EOS_CST : public CST {
public:
  EOS_CST() : CST(CST_Kind::EndOfStream, SourceLocation()) {}
};

class IdentifierCST : public CST {
  IDTy id;

public:
  IdentifierCST(const SourceLocation &loc, IDTy id)
      : CST(CST_Kind::Identifier, loc), id(id) {}
  IDTy getID() const { return id; }
};

class ExpressionCST : public CST {
  IDTy machineMode;
  std::vector<CST *> subforms;

public:
  ExpressionCST(const SourceLocation &loc, IDTy machineMode,
                std::vector<CST *> &&subforms)
      : CST(CST_Kind::Expression, loc), machineMode(machineMode),
        subforms(std::move(subforms)) {}
  IDTy getLeadID() const {
    if (subforms.empty() || subforms[0]->getKind() != CST_Kind::Identifier) {
      return IdentifierInterner::InvalidID;
    }
    return static_cast<IdentifierCST *>(subforms[0])->getID();
  }
  IDTy getMachineMode() const { return machineMode; }
  llvm::ArrayRef<CST *> getSubforms() const { return subforms; }
};

class IntCST : public CST {
  llvm::APInt value;

public:
  IntCST(const SourceLocation &loc, llvm::APInt &&value)
      : CST(CST_Kind::Int, loc), value(std::move(value)) {}
  const llvm::APInt &getValue() const { return value; }
};

class HostIntCST : public CST {
  llvm::APInt value;

public:
  HostIntCST(const SourceLocation &loc, llvm::APInt &&value)
      : CST(CST_Kind::HostInt, loc), value(std::move(value)) {}
  const llvm::APInt &getValue() const { return value; }
};

class StringCST : public CST {
  llvm::StringRef str;

public:
  StringCST(const SourceLocation &loc, llvm::StringRef str)
      : CST(CST_Kind::String, loc), str(str) {}
  llvm::StringRef getStr() const { return str; }
};

class CodeStringCST : public CST {
  llvm::StringRef str;

public:
  CodeStringCST(const SourceLocation &loc, llvm::StringRef str)
      : CST(CST_Kind::CodeString, loc), str(str) {}
  llvm::StringRef getStr() const { return str; }
};

class VectorCST : public CST {
  std::vector<CST *> members;

public:
  VectorCST(const SourceLocation &loc, std::vector<CST *> &&members)
      : CST(CST_Kind::Vector, loc), members(std::move(members)) {}
};

} // namespace grp
