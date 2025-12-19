#include "include/lexer.hpp"
#include <fstream>
int main() {
  freopen("testcases/1.out", "w", stdout);
  std::ifstream infile("testcases/1.out");
  std::string source((std::istreambuf_iterator<char>(infile)),std::istreambuf_iterator<char>());
  lexer lex(source);
  std::vector<Token> tokens = lex.tokenize();
  lex.output(tokens);
}