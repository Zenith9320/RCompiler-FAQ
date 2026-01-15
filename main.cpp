#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include "include/lexer.hpp"
#include "include/parser.hpp"
#include "include/semantic_check.hpp"
#include "include/ir.hpp"

int main() {
    std::string source;
    std::string line;
    while (std::getline(std::cin, line)) {
        source += line + "\n";
    }

    try {
        // 词法分析
        lexer lex(source);
        std::vector<Token> tokens = lex.tokenize();

        // 语法分析
        parser par(tokens);
        std::vector<std::unique_ptr<ASTNode>> ast = par.parse();

        // 语义检查
        semantic_checker checker(std::move(ast));
        bool semantic_ok = checker.check();

        if (!semantic_ok) {
            //std::cout << "return 0" << std::endl;
            return 0;
        }

        // 生成IR
        IRGenerator generator;
        std::string irCode = generator.generate(ast);

        // 输出IR
        if (semantic_ok) std::cout << irCode;

    } catch (const std::exception& e) {
        return 0;
    }

    //std::cout << "return 1" << std::endl;
    return 1;
}