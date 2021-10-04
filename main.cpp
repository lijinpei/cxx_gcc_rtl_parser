#include "parser.h"

#include "llvm/Support/CommandLine.h"

#include <string>

namespace cl = llvm::cl;

cl::opt<std::string> inputFileName(cl::Positional, cl::desc("<input-file>"),
                                   cl::Required);
int main(int argc, const char *argv[]) {
  cl::ParseCommandLineOptions(argc, argv);
  grp::ParserOption option =
      grp::ParserOption::createDefaultOption(inputFileName);
  grp::ParserContext context(option);
  grp::CSTParser parser(context);
  while (auto *result = parser.parseTopCST()) {
  }
}
