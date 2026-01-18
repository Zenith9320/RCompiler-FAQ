#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include "include/ir.hpp"
#include "include/semantic_check.hpp"

int main() {
    std::string source((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
    try {
        // 词法分析
        lexer lex(source);
        std::vector<Token> tokens = lex.tokenize();

        // 语法分析 - 禁止输出
        std::ofstream nullstream("/dev/null");
        std::streambuf* oldcout = std::cout.rdbuf(nullstream.rdbuf());
        std::streambuf* oldcerr = std::cerr.rdbuf(nullstream.rdbuf());
        parser par(tokens);
        parser par1(tokens);
        std::vector<std::unique_ptr<ASTNode>> ast = par.parse();
        std::vector<std::unique_ptr<ASTNode>> ast1 = par1.parse();
        std::cout.rdbuf(oldcout);
        std::cerr.rdbuf(oldcerr);

        // 生成IR
        IRGenerator generator;
        std::string irCode;
        try {
            irCode = generator.generate(std::move(ast1));
        } catch (const std::exception& e) {
            return 1;
        }

        // 输出IR
        std::cout << irCode;

    } catch (const std::exception& e) {
        return 1;
    }

    return 0;
}