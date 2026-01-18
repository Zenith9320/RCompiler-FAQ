#include <iostream>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include "include/ir.hpp"

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
        std::vector<std::unique_ptr<ASTNode>> ast = par.parse();
        std::cout.rdbuf(oldcout);
        std::cerr.rdbuf(oldcerr);

        // 生成IR
        IRGenerator generator;
        std::string irCode;
        try {
            irCode = generator.generate(ast);
        } catch (const std::exception& e) {
            // 如果生成过程中出错，输出已经生成的IR
            irCode = generator.getCurrentIR();
            std::cout << irCode;
            std::cout << "Error: " << e.what() << std::endl;
            return 1;
        }

        // 输出IR
        std::cout << irCode;

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}