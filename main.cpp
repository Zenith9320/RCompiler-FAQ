#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include "include/ir.hpp"
#include "include/semantic_check.hpp"

int main() {
    std::ofstream nullstream("/dev/null");
    std::streambuf* oldcerr = std::cerr.rdbuf(nullstream.rdbuf());
    std::string source((std::istreambuf_iterator<char>(std::cin)),std::istreambuf_iterator<char>());
    try {
        // 词法分析
        lexer lex(source);
        std::vector<Token> tokens = lex.tokenize();

        // 语法分析 - 禁止输出
        std::streambuf* oldcout = std::cout.rdbuf(nullstream.rdbuf());
        parser par(tokens);
        std::vector<std::unique_ptr<ASTNode>> ast;
        try {
            ast = par.parse();
        } catch (const std::exception& e) {
            std::cout.rdbuf(oldcout);
            std::cerr.rdbuf(oldcerr);
            return 1;
        }

        semantic_checker sc(std::move(ast));
        if (!sc.check()) {
            //std::cout << "Semantic error" << std::endl;
            return 1;
        }

        parser par1(tokens);
        std::vector<std::unique_ptr<ASTNode>> ast1;
        try {
            ast1 = par1.parse();
        } catch (const std::exception& e) {
            return 0;
        }
        std::cout.rdbuf(oldcout);
        // 生成IR
        IRGenerator generator;
        std::string irCode;
        try {
            irCode = generator.generate(std::move(ast1));
        } catch (const std::exception& e) {
            return 0;
        }

        std::cout << irCode;

    } catch (const std::exception& e) {
        return 0;
    }

    std::cerr.rdbuf(oldcerr);
    return 0;
}
