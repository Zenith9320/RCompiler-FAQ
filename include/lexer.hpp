#ifndef LEXER_HPP
#define LEXER_HPP

#include <vector>
#include <string>
#include <regex>
#include <boost/regex.hpp>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <stdexcept>

enum TokenType {
  STRICT_KEYWORD, RESERVED_KEYWORD, IDENTIFIER, 
  CHAR_LITERAL, STRING_LITERAL, RAW_STRING_LITERAL, BYTE_LITERAL, BYTE_STRING_LITERAL, RAW_BYTE_STRING_LITERAL, C_STRING_LITERAL, RAW_C_STRING_LITERAL,
  INTEGER_LITERAL, FLOAT_LITERAL, LIFETIME, PUNCTUATION, DELIMITER, RESERVED_TOKEN, UNKNOWN
};

extern boost::regex keyword_regex;
extern boost::regex strict_keyword_regex;
extern boost::regex reserved_keyword_regex;
extern boost::regex identifier_regex;
extern boost::regex char_literal_regex;
extern boost::regex string_literal_regex;
extern boost::regex raw_string_regex;
extern boost::regex byte_literal_regex;
extern boost::regex byte_string_regex;
extern boost::regex raw_byte_string_regex;
extern boost::regex c_str_regex;
extern boost::regex raw_c_str_regex;
extern boost::regex int_literal_regex;
extern boost::regex float_literal_regex;
extern boost::regex lifetime_regex;
extern boost::regex punctuation_regex;
extern boost::regex delimiter_regex;
extern boost::regex reserved_token_regex;

struct Token {
  TokenType type;  
  std::string value;
  int line;
  int column;

  Token() = default;
  Token(TokenType t, std::string v, int l, int c): value(v), line(l), column(c) {
    type = t;
  };
};

struct TokenRule {
  TokenType type; 
  boost::regex rule;

  TokenRule() = default;
  TokenRule(TokenType t, boost::regex r): rule(r) {
    type = t;
  };

};

extern std::unordered_set<std::string> keywords;
extern std::vector<TokenRule> type_rules;

class lexer {
private:
    std::string input;
    int pos;
    int line;
    int column;

    void skip_comment();

    void skip_whitespace();

public:
    explicit lexer(const std::string &src);

    std::vector<Token> tokenize();

    Token next_token();

    void output(std::vector<Token> res);
};

#endif