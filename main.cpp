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

    return -1;
}