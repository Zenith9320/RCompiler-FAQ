#include "include/semantic_check.hpp"
#include "include/lexer.hpp"
#include<fstream>
int main() {
  freopen("testcases/1.out", "w", stdout);
  std::ifstream infile("testcases/1.data");
  std::string source((std::istreambuf_iterator<char>(infile)),std::istreambuf_iterator<char>());
  lexer lex(source);
  std::vector<Token> tokens = lex.tokenize();
  lex.output(tokens);
  parser p(tokens);
  try {
    auto ast = p.parse();
    semantic_checker sc(std::move(ast));
    if (sc.check()) std::cout << "0" << std::endl;
    else std::cout << "-1" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "parse failed : " << e.what() << std::endl;
    std::cout << "-1" << std::endl;
    return 0;
  }
}