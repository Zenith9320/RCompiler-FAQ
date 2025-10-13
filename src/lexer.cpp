#include "../include/lexer.hpp"

boost::regex keyword_regex(R"(\b(as|break|const|continue|crate|else|enum|extern|false|fn|for|if|impl|in|let|loop|match|mod|move|mut|pub|ref|return|self|Self|static|use|where|while|struct|super|trait|true|type|unsafe|async|await|dyn|abstract|become|box|do|final|macro|override|priv|typeof|unsized|virtual|yield|try|gen|macro_rules|raw|safe|union)\b)");

boost::regex strict_keyword_regex(R"(\b(as|break|const|continue|crate|else|enum|extern|false|fn|for|if|impl|in|let|loop|match|mod|move|mut|pub|ref|return|Self|static|use|where|while|struct|super|trait|true|type|unsafe|async|await|dyn)\b)");

boost::regex reserved_keyword_regex(R"(\b(abstract|become|box|do|final|macro|override|priv|typeof|unsized|virtual|yield|try|gen)\b)");

boost::regex identifier_regex("[A-Za-z][A-Za-z0-9_]{0,63}");

boost::regex char_literal_regex(R"('([^'\\\n\r\t]|\\'|\\"|\\"|\\x[0-9a-fA-F]{2}|\\n|\\r|\\t|\\\\|\\0|\\u\{[0-9a-fA-F_]{1,6}\})')");

boost::regex string_literal_regex(R"("([^"\\\r\n]|\\["\\nrt0]|\\x[0-9a-fA-F]{2}|\\u\{[0-9a-fA-F_]{1,6}\}|\\\n)*"([a-zA-Z_][a-zA-Z0-9_]*)?)");

boost::regex raw_string_regex(R"foo(r(#+)\"([^\r]*?)\"\1([a-zA-Z_][a-zA-Z0-9_]*)?)foo");

boost::regex byte_literal_regex(R"foo(b'([^'\\\n\r\t]|\\x[0-9A-Fa-f]{2}|\\n|\\r|\\t|\\\\|\\0|\\'|\\")'([a-zA-Z_][a-zA-Z0-9_]*)?)foo");

boost::regex byte_string_regex(R"foo(b"([^"\\\r]|\\x[0-9A-Fa-f]{2}|\\n|\\r|\\t|\\\\|\\0|\\'|\\"|\\\n)*"([a-zA-Z_][a-zA-Z0-9_]*)?)foo");

boost::regex raw_byte_string_regex(R"foo(br(#+)\"((?:[^\r]|\\r)*?)\"\1([a-zA-Z_][a-zA-Z_0-9]*)?)foo");

boost::regex c_str_regex(R"foo(c"(?:[^"\\\r\x00]|\\(?:[nrt'"\\]|x(?!00)[0-9A-Fa-f]{2})|\\u\{(?!0+(?:\}|$))[0-9A-Fa-f]{1,6}\}|\\\r?\n)*"([a-zA-Z_][a-zA-Z_0-9]*)?)foo");

boost::regex raw_c_str_regex(R"(cr(#+)\"([^\r\x00]*?)\"\1([a-zA-Z_][a-zA-Z0-9_]*)?)");

boost::regex int_literal_regex(R"foo(([0-9](?:[0-9_]*)|0b(?:[01_]*[01])(?:[01_]*)?|0o(?:[0-7_]*[0-7])(?:[0-7_]*)?|0x(?:[0-9a-fA-F_]*[0-9a-fA-F])(?:[0-9a-fA-F_]*)?)(?:[a-df-zA-DF-Z_][a-zA-Z0-9_]*)?)foo");

boost::regex float_literal_regex(R"foo((?:[0-9](?:[0-9_]*))\.(?![._a-zA-Z])|(?:[0-9](?:[0-9_]*))\.(?:[0-9](?:[0-9_]*))(?:[a-df-zA-DF-Z_][a-zA-Z0-9_]*)?)foo");

boost::regex lifetime_regex(R"foo('(?:_|[a-zA-Z_][a-zA-Z0-9_]*|r#(?!crate|self|super|Self\b)[a-zA-Z_][a-zA-Z0-9_]*|_)(?!'))foo");

boost::regex punctuation_regex(R"foo((==|!=|<=|>=|&&|\|\||<<=|>>=|\+=|-=|\*=|/=|%=|\^=|&=|\|=|<<|>>|::|->|<-|=>|\.{3}|\.\.=|\.{2}|…|[=<>!~+\-*/%^&|@.,，;；:：#$?_{}\[\]\(\)]))foo");

boost::regex delimiter_regex(R"foo([{}\[\]\(\)])foo");

boost::regex reserved_token_regex(R"foo((?:[a-zA-Z_]\w*#?(?:"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'))|(\b\d(?:_?\d)*(?:\.\d(?:_?\d)*)?(?:[eE][+-]?\d(?:_?\d)*)?[a-zA-Z_]\w*))foo");

std::unordered_set<std::string> keywords = {
    "as", "break", "const", "continue", "crate", "else", "enum", "extern",
    "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
    "move", "mut", "pub", "ref", "return", "self", "Self", "static", "use",
    "where", "while", "struct", "super", "trait", "true", "type", "unsafe",
    "async", "await", "dyn",
    "abstract", "become", "box", "do", "final", "macro", "override", "priv",
    "typeof", "unsized", "virtual", "yield", "try", "gen",
    "'static", "macro_rules", "raw", "safe", "union"
};

std::vector<TokenRule> type_rules = {
  TokenRule(TokenType::STRICT_KEYWORD, strict_keyword_regex),
  TokenRule(TokenType::RESERVED_KEYWORD, reserved_keyword_regex),
  TokenRule(TokenType::IDENTIFIER, identifier_regex),
  TokenRule(TokenType::CHAR_LITERAL, char_literal_regex),
  TokenRule(TokenType::STRING_LITERAL, string_literal_regex),
  TokenRule(TokenType::RAW_STRING_LITERAL, raw_string_regex),
  TokenRule(TokenType::BYTE_LITERAL, byte_literal_regex),
  TokenRule(TokenType::BYTE_STRING_LITERAL, byte_string_regex),
  TokenRule(TokenType::RAW_BYTE_STRING_LITERAL, raw_byte_string_regex),
  TokenRule(TokenType::C_STRING_LITERAL, c_str_regex),
  TokenRule(TokenType::RAW_C_STRING_LITERAL, raw_c_str_regex),
  TokenRule(TokenType::FLOAT_LITERAL, float_literal_regex),
  TokenRule(TokenType::INTEGER_LITERAL, int_literal_regex),
  TokenRule(TokenType::PUNCTUATION, punctuation_regex),
  TokenRule(TokenType::DELIMITER, delimiter_regex),
  TokenRule(TokenType::RESERVED_TOKEN, reserved_token_regex)
};

lexer::lexer(const std::string &src) : input(src), pos(0), line(1), column(1) {}

void lexer::skip_comment() {
  int length = input.size();

  while (pos + 1 < length) {
    if (input[pos] == '/' && input[pos + 1] == '/') {
      pos += 2;
      while (pos < length && input[pos] != '\n') pos++;
      if (pos < length && input[pos] == '\n') {
        pos++;
        line++;
        column = 1;
      }
      continue;
    }

    else if (input[pos] == '/' && input[pos + 1] == '*') {
      pos += 2;
      bool closed = false;
      while (pos < length) {
        if (pos + 1 < length && input[pos] == '*' && input[pos + 1] == '/') {
          pos += 2;
          closed = true;
          break;
        }
        if (input[pos] == '\n') {
          line++;
          column = 1;
        } else {
          column++;
        }
        pos++;
      }

      if (!closed) {
        std::cerr << "Warning: unterminated block comment at line "
                  << line << ", column " << column << std::endl;
        return;
      }
      continue;
    }
    else break;
  }
}


void lexer::skip_whitespace() {
  while (pos < input.size()) {
    char c = input[pos];
    if (c == ' ' || c == '\t') {
      pos++;
      column++;
    } else if (c == '\n') {
      pos++;
      line++;
      column = 1;
    } else {
      break;
    }
  }
}

Token lexer::next_token() {
  int length = input.size();
  skip_whitespace();
  skip_comment();
  skip_whitespace();
  if (pos >= length) {
    return Token(TokenType::UNKNOWN, "", line, column);
  }
  std::string remaining = input.substr(pos);
  for (const auto &rule : type_rules) {
    boost::smatch match;
    if (boost::regex_search(remaining, match, rule.rule, boost::match_continuous)) {
      std::string lexeme = match.str();
      Token tok(rule.type, lexeme, line, column);
      for (char c : lexeme) {
        if (c == '\n') {
          line++;
          column = 1;
        } else {
          column++;
        }
      }
      pos += lexeme.size();
      return tok;
    }
  }
  char c = input[pos];
  Token tok(TokenType::UNKNOWN, std::string(1, c), line, column);
  pos++;
  column++;
  return tok;
}

std::vector<Token> lexer::tokenize() {
  lexer self = *this;
  std::vector<Token> tokens;
  while (self.pos < self.input.size()) {
    Token tok = self.next_token();
    if (tok.type == TokenType::UNKNOWN && tok.value.empty()) {
      break;
    }
    tokens.push_back(tok);
  }
  return tokens;
}

void lexer::output(std::vector<Token> res) {
  for (int i = 0; i < res.size(); i++) {
    std::cout << '{' ;
    switch(res[i].type) {
      case TokenType::BYTE_LITERAL: std::cout << "BYTE_LITERAL, "; break;
      case TokenType::BYTE_STRING_LITERAL: std::cout << "BYTE_STRING_LITERAL, "; break;
      case TokenType::C_STRING_LITERAL: std::cout << "C_STRING_LITERAL, "; break;
      case TokenType::CHAR_LITERAL: std::cout << "CHAR_LITERAL, "; break;
      case TokenType::DELIMITER: std::cout << "DELIMITER, "; break;
      case TokenType::FLOAT_LITERAL: std::cout << "FLOAT_LITERAL, "; break;
      case TokenType::IDENTIFIER: std::cout << "IDENTIFIER, "; break;
      case TokenType::INTEGER_LITERAL: std::cout << "INTEGER_LITERAL, "; break;
      case TokenType::LIFETIME: std::cout << "LIFETIME, "; break;
      case TokenType::PUNCTUATION: std::cout << "PUNCTUATION, "; break;
      case TokenType::RAW_BYTE_STRING_LITERAL: std::cout << "RAW_BYTE_STRING_LITERAL, "; break;
      case TokenType::RAW_C_STRING_LITERAL: std::cout << "RAW_C_STRING_LITERAL, "; break;
      case TokenType::RAW_STRING_LITERAL: std::cout << "RAW_STRING_LITERAL, "; break;
      case TokenType::RESERVED_KEYWORD: std::cout << "RESERVED KEYWORD, "; break;
      case TokenType::STRICT_KEYWORD: std::cout << "STRICT KEYWORD, "; break;
      case TokenType::STRING_LITERAL: std::cout << "STRING LITERAL, "; break;
      case TokenType::UNKNOWN: std::cout << "UNKNOWN, "; break;
      default: throw std::runtime_error("didn't match");
    }
    std::cout << res[i].value << '}' << std::endl;
  }
}
