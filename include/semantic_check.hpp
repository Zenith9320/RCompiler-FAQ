#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP
#include "parser.hpp"

template<typename T, typename U>
bool isSameDerived(const std::unique_ptr<T>& lhs, const std::unique_ptr<U>& rhs) {
  if (!lhs || !rhs) return false;
  return typeid(*lhs) == typeid(*rhs);
}

struct FieldInfo {
  std::string name;
  TypeNode* type;
};

struct StructInfo {
  std::string name;
  std::vector<FieldInfo> fields;
};

struct Symbol {
  std::string name;
  TypeNode* type;
  bool isMutable;
  bool isRef;
  bool isInitialized;
};

struct FunctionSymbol {
  std::string name;
  FunctionParameter* param_types;
  TypeNode* return_type = nullptr;
  std::optional<std::string> impl_type_name;
};

struct TypeSymbol {
  std::string name;
  TypeNode* node = nullptr;
};

struct TraitSymbol {
  std::string name;
  bool isUnsafe = false;
  std::vector<FunctionSymbol> functions;
  std::vector<std::string> super_traits;
};

struct struct_item {
  std::string struct_id;
  std::string item_id;

  bool operator<(const struct_item& other) const noexcept {
    return std::tie(struct_id, item_id) < std::tie(other.struct_id, other.item_id);
  }

  bool operator==(const struct_item& other) const noexcept {
    return struct_id == other.struct_id && item_id == other.item_id;
  }
};

struct SymbolTable {
  std::unordered_set<std::string> structs;
  std::unordered_map<std::string, FunctionParameter*> functions;
  std::unordered_set<std::string> constants;
  std::map<struct_item, std::string> struct_items; //从identifier到type的map

  SymbolTable() = default;

  bool has_struct(const std::string& name) const {
    return structs.count(name);
  }

  bool has_struct_item(const std::string& struct_name, const std::string& item_name) const {
    return struct_items.count({struct_name, item_name});
  }

  bool has_function(const std::string& name) const {
    return functions.count(name);
  }

  bool has_constant(const std::string& name) const {
    return constants.count(name);
  }

  bool has_struct_item(const std::string& struct_name, const std::string& item_name) {
    return struct_items.count({struct_name, item_name});
  }
};

struct ConstantInfo {
  std::string id;
  TypeNode* type;
  ExpressionNode* expr;
};

class Scope {
 public:
  std::unordered_map<std::string, Symbol> var_table;           // 变量/常量
  std::unordered_map<std::string, FunctionSymbol> func_table;  // 函数/方法
  //如果是一个Type绑定的func，key是Type->toString()
  //如果是一个普通的func，key是function的identifier
  //var_table同理
  std::unordered_map<std::string, TypeSymbol> type_table;      // 类型
  std::unordered_map<std::string, StructInfo> declared_struct; //已经声明的struct
  std::unordered_map<std::string, FunctionSymbol> declared_struct_functions; //已经声明的struct的函数, key是struct+"::"+函数的id，value是这个函数的相关信息
  std::unordered_map<std::string, TraitSymbol> trait_table; //从trait到trait相关信息的函数
  std::unordered_map<std::string, ConstantInfo> const_table; //从const的id到信息
  SymbolTable symbol_table;
  Scope* parent; // 父作用域
  std::string possible_self = "";
  int id = 0;

  Scope(Scope* parent_scope) : parent(parent_scope) {
    if (parent_scope) id = parent_scope->id + 1;
  }

  void insertVar(std::string name, Symbol sym) {
    var_table[name] = sym;
  }

  Symbol* lookupVar(const std::string& name) {
    if (var_table.count(name)) return &var_table[name];
    if (parent) return parent->lookupVar(name);
    return nullptr;
  }

  void insertFunc(std::string name, FunctionSymbol func) {
    if (func_table.count(name)) {
      throw std::runtime_error("Duplicate function declaration: " + name);
    }
    func_table[name] = func;
  }

  FunctionSymbol* lookupFunc(const std::string& name) {
    if (func_table.count(name)) {
      return &func_table[name];
    }
    if (parent) {
      return parent->lookupFunc(name);
    }
    return nullptr;
  }

  FunctionSymbol* lookupStructFunc(const std::string& name) {
    if (declared_struct_functions.count(name)) {
      return &declared_struct_functions[name];
    }
    if (parent) {
      return parent->lookupStructFunc(name);
    }
    return nullptr;
  }

  void insertType(std::string name, TypeSymbol type) {
    if (type_table.count(name)) {
      throw std::runtime_error("Duplicate type declaration: " + name);
    }
    type_table[name] = type;
  }

  TypeSymbol* lookupType(const std::string& name) {
    if (type_table.count(name)) return &type_table[name];
    if (parent) return parent->lookupType(name);
    return nullptr;
  }

  StructInfo* lookupStruct(const std::string& name) {
    if (declared_struct.count(name)) return &declared_struct[name];
    if (parent) return parent->lookupStruct(name);
    return nullptr;
  }

  bool is_forward_declared(const std::string& name) {
   if (symbol_table.has_constant(name) || symbol_table.has_function(name) || symbol_table.has_struct(name)) {
    return true;
   } else {
    if (parent) {
      std::cout << "finding in parent scope" << std::endl;
      return parent->is_forward_declared(name);
    } else {
      return false;
    }
   }
  }

  FunctionParameter* find_func_param(const std::string& name) {
    if (symbol_table.functions.count(name)) {
      return symbol_table.functions[name];
    } else {
      if (parent) return parent->find_func_param(name);
      else return nullptr;
    }
  }

  ConstantInfo* lookupConst(const std::string& name) {
    if (const_table.count(name)) {
      return &const_table[name];
    } else {
      if (parent) return parent->lookupConst(name);
      else return nullptr;
    }
  }
};

class semantic_checker {
 private:
  std::vector<std::unique_ptr<ASTNode>> ast;
  Scope* currentScope;

 public:
  ~semantic_checker() = default;

  semantic_checker(std::vector<std::unique_ptr<ASTNode>> a) : ast(std::move(a)) {
    currentScope = new Scope(nullptr);
  };

  void enterScope() {
    Scope* newScope = new Scope(currentScope);
    currentScope = newScope;
    std::cout << "enter Scope : " << currentScope->id << std::endl;
  }

  void exitScope() {
    if (currentScope && currentScope->parent) {
      Scope* parent = currentScope->parent;
      delete currentScope;
      currentScope = parent;
      std::cout << "exit to Scope: " << currentScope->id << std::endl; 
    }
  }

  void declareVariable(const std::string& name, TypeNode* type, bool isMut) {
    Symbol sym{name, type, isMut, false};
    currentScope->insertVar(name, std::move(sym));
    std::cout << "declaring variable : " << name << std::endl; 
  }
  
  Symbol* resolveVariable(const std::string& name) {
    return currentScope->lookupVar(name);
  }

  bool check_Statment(const StatementNode* stat) {
    if (!stat) return false;
    if (stat->type == StatementType::SEMICOLON) {
      if (stat->expr_statement != nullptr || stat->item != nullptr || stat->let_statement != nullptr) {
        return false;
      }
      return true;
    } else if (stat->type == StatementType::ITEM) {
      std::cout << "checking itemStatement" << std::endl;
      auto *item = dynamic_cast<ItemNode*>(stat->item.get()); 
      return check_Item(item);
    } else if (stat->type == StatementType::EXPRESSIONSTATEMENT) {
      std::cout << "checking ExpressionStatement" << std::endl;
      return check_ExpressionStatement(dynamic_cast<ExpressionStatement*>(stat->expr_statement.get()));
    } else if (stat->type == StatementType::LETSTATEMENT) {
      std::cout << "checking letStatement" << std::endl;
      return check_LetStatement(stat);
    }
    return false;
  }

  bool check_ExpressionStatement(const ExpressionStatement* expr) {
    return check_expression(dynamic_cast<const ExpressionNode*>(expr->expression.get()));
  }

  bool check_ComparisonExpression(const ComparisonExpressionNode* expr) {
    std::string type1 = getExpressionType(expr->expression1.get())->toString();
    std::string type2 = getExpressionType(expr->expression2.get())->toString();
    std::cout << "type of expression1 in comparison expression : " << type1 << std::endl;
    std::cout << "type of expression2 in comparison expression : " << type2 << std::endl;
    if (type1 == type2 || type1 == "usize" && type2 == "i32") return true;
    return false;
  }

  void declareFunctionParameters(const FunctionParameter* params, Scope* currentScope, const std::optional<std::string>& implTypeName = std::nullopt) {
    if (!params) {
      std::cout << "the function has no parameter" << std::endl;
      return;
    }
    if (params->self_param) {
      auto& self = params->self_param;
      std::string selfName = "self"; 
      TypeNode* typeNode = nullptr;

      if (std::holds_alternative<std::unique_ptr<ShorthandSelf>>(self->self)) {
        typeNode = self->type_node.get();
        if (!typeNode && implTypeName) {
          typeNode = new TypePathNode(*implTypeName);
          self->type_node.reset(typeNode);
        }
      } else if (std::holds_alternative<std::unique_ptr<TypedSelf>>(self->self)) {
        auto& typedSelf = std::get<std::unique_ptr<TypedSelf>>(self->self);
        typeNode = typedSelf->type.get();
      }

      Symbol selfSymbol{selfName, typeNode, false, true};
      currentScope->insertVar(selfName, std::move(selfSymbol));
    }

    for (size_t i = 0; i < params->function_params.size(); ++i) {
      const FunctionParam& fp = params->function_params[i];
      std::string paramName;
      TypeNode* typeNode = nullptr;
      bool if_mut = false;

      if (std::holds_alternative<std::unique_ptr<FunctionParamPattern>>(fp.info)) {
        auto& pattern = std::get<std::unique_ptr<FunctionParamPattern>>(fp.info);
        paramName = pattern->pattern->toString();
        typeNode = pattern->type.get();
        if (auto *ref = dynamic_cast<ReferenceTypeNode*>(typeNode)) {
          if_mut = ref->if_mut;
        }
        std::cout << "declaring: " << paramName << " whose if_mut is " << if_mut << " and whose type is : " << typeNode->toString() << std::endl;
      } else if (std::holds_alternative<std::unique_ptr<TypeNode>>(fp.info)) {
        paramName = "_param" + std::to_string(i);
        typeNode = std::get<std::unique_ptr<TypeNode>>(fp.info).get();
        if (auto *ref = dynamic_cast<ReferenceTypeNode*>(typeNode)) {
          if_mut = ref->if_mut;
        }
        std::cout << "declaring: " << paramName << " whose if_mut is " << if_mut << std::endl;
      } else if (std::holds_alternative<std::unique_ptr<ellipsis>>(fp.info)) {
        std::cerr << "Variadic ... parameters not supported" << std::endl;
        continue;
      } else {
        throw std::runtime_error("wrong type of element in functionparameter");
      }

      Symbol paramSymbol{paramName, typeNode, if_mut, true};
      currentScope->insertVar(paramName, std::move(paramSymbol));
    }
  }


  void declareStruct(const StructStructNode* structNode) {
    if (!structNode) return;

    StructInfo structInfo;
    structInfo.name = structNode->identifier;

    for (const auto& field : structNode->struct_fields->struct_fields) {
      FieldInfo fieldInfo;
      fieldInfo.name = field->identifier;
      fieldInfo.type = field->type.get();
      structInfo.fields.push_back(std::move(fieldInfo));
    }

    TypeSymbol typeSym;
    typeSym.name = structNode->identifier;
    //不要type，直接到declaredScope里面找有没有这个identifier对应的struct

    if (!currentScope) {
      throw std::runtime_error("No current scope to insert struct type");
    }

    Scope* targetScope = currentScope;
    try {
      targetScope->insertType(structNode->identifier, typeSym);
      std::cout << "Declared struct struct type: " << structNode->identifier << " in scope: " << currentScope->id << std::endl;
    } catch (const std::exception& e) {
      std::cerr << "Error declaring struct: " << e.what() << std::endl;
    }

    currentScope->declared_struct[structNode->identifier] = std::move(structInfo);
  }

  std::string get_return_type_in_expression(ExpressionNode* expr) {
    std::cout << "get return type in expression" << std::endl;
    if (auto* return_expr = dynamic_cast<ReturnExpressionNode*>(expr)) {
      std::cout << "try to get return type in returnexpression" << std::endl;
      return getExpressionType(return_expr->expression.get())->toString();
    } else if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expr)) {
      std::cout << "try to get return type in ifexpression" << std::endl;
      std::string type = get_return_type(if_expr->block_expression.get());
      if (type != "") {
        return type;
      } else {
        auto* ewb = if_expr->block_expression->expression_without_block.get();
        if (!ewb) return "";
        return getExpressionType(ewb)->toString();
      }
    } else if (auto* expr_node = dynamic_cast<BorrowExpressionNode*>(expr)) {
      return get_return_type_in_expression(expr_node->expression.get());
    } else if (auto* expr_node = dynamic_cast<PredicateLoopExpressionNode*>(expr)) {
      return get_return_type(expr_node->block_expression.get());
    } else if (auto* expr_node = dynamic_cast<ComparisonExpressionNode*>(expr)) {
      return "bool";
    } else if (auto* expr_node = dynamic_cast<LazyBooleanExpressionNode*>(expr)) {
      return "bool";
    }
    return "";
  }

  std::string get_return_type_in_statement(StatementNode* stat) {
    std::cout << "get return type in statement" << std::endl;
    if (stat->let_statement) {
      check_LetStatement(stat);
    }
    auto* expr_stat = stat->expr_statement.get();
    if (!expr_stat) {
      return "";
    } else {
      std::cout << "checking expressionStatement" << std::endl;
      return get_return_type_in_expression(expr_stat->expression.get());
    }
  } 

  bool check_return_type(BlockExpressionNode* block) {
    std::cout << "enter scope in checking return type in block expression" << std::endl;
    enterScope();
    std::unordered_set<std::string> types;
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode" << std::endl;
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        types.insert(temp);
      }
    }
    if (auto* expr_node = dynamic_cast<ExpressionWithoutBlockNode*>(block->expression_without_block.get())) {
      std::string s = std::visit([this](auto& node_ptr) -> std::string {
        using T = std::decay_t<decltype(node_ptr)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          if (node_ptr && node_ptr->expression && getExpressionType(node_ptr->expression.get())) return getExpressionType(node_ptr->expression.get())->toString();
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IfExpressionNode>>) {
          if (node_ptr) {
            std::string t = get_return_type(node_ptr->then_block.get());
            if (t != "") return t;
            if (node_ptr->else_block) return get_return_type(node_ptr->else_block.get());
          }
          if (auto* last_expr = node_ptr->block_expression->expression_without_block.get()) {
            std::string res = getExpressionType(last_expr)->toString();
            return res;
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BlockExpressionNode>>) {
          if (node_ptr) return get_return_type(node_ptr.get());
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
          return "bool";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          if (node_ptr) {
            if (std::holds_alternative<std::unique_ptr<integer_literal>>(node_ptr->literal)) {
              const auto& intLit = std::get<std::unique_ptr<integer_literal>>(node_ptr->literal);
              std::string s = intLit->value;

              std::cout << "get integer_literal: " << s << std::endl;

              size_t pos = 0;
              while (pos < s.size() && std::isdigit(s[pos])) ++pos;

              std::string suffix = s.substr(pos);
              std::string type;

              if (!suffix.empty()) {
                type = suffix;
              } else {
                type = "i32";
              }
              return type;
            }
            if (std::holds_alternative<std::unique_ptr<float_literal>>(node_ptr->literal)) {
              std::cout << "get float_literal" << std::endl;
              return "f64";
            }
            if (std::holds_alternative<std::unique_ptr<bool>>(node_ptr->literal)) {
              std::cout << "get bool_literal" << std::endl;
              return "bool";
            }
            if (std::holds_alternative<std::unique_ptr<char_literal>>(node_ptr->literal)) {
              return "char";
            }
            if (std::holds_alternative<std::unique_ptr<string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<c_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_c_string_literal>>(node_ptr->literal)) {
              return "str";
            }
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
          auto* expr1 = node_ptr->expression1.get();
          auto* type = getExpressionType(expr1);
          if (auto* path_type = dynamic_cast<TypePathNode*>(type)) {
            std::string name = path_type->toString();
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else {
          return "";
        }
      }, expr_node->expr);
      if (s == "") std::cout << "failed to get type in expressionwithoutblock" << std::endl;
      if (s != "") {
        types.insert(s);
        std::cout << "type in expression without block: " << s << std::endl;
      }
    }
    if (types.size() != 1) {
      for (auto it = types.begin(); it != types.end(); it++) {
        std::cout << "return type in block expression : " << *it << std::endl;
      }
      exitScope();
      return false;
    }
    exitScope();
    return true;
  }

  bool check_return_type_without_changing_scope(BlockExpressionNode* block) {
    std::cout << "enter scope in checking return type in block expression" << std::endl;
    std::unordered_set<std::string> types;
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode" << std::endl;
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        types.insert(temp);
      }
    }
    if (auto* expr_node = dynamic_cast<ExpressionWithoutBlockNode*>(block->expression_without_block.get())) {
      std::cout << "getting return type in expressionwithoutblock in blockexpression" << std::endl;
      std::string s = std::visit([this](auto& node_ptr) -> std::string {
        using T = std::decay_t<decltype(node_ptr)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          if (node_ptr && node_ptr->expression && getExpressionType(node_ptr->expression.get())) return getExpressionType(node_ptr->expression.get())->toString();
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
          return "bool";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
          std::cout << "return type: got in struct expression" << std::endl;
          return node_ptr->pathin_expression->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IfExpressionNode>>) {
          if (node_ptr) {
            std::string t = get_return_type(node_ptr->then_block.get());
            if (t != "") return t;
            if (node_ptr->else_block) return get_return_type(node_ptr->else_block.get());
          }
          if (auto* last_expr = node_ptr->block_expression->expression_without_block.get()) {
            std::string res = getExpressionType(last_expr)->toString();
            return res;
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BlockExpressionNode>>) {
          if (node_ptr) return get_return_type(node_ptr.get());
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          if (node_ptr) {
            if (std::holds_alternative<std::unique_ptr<integer_literal>>(node_ptr->literal)) {
              const auto& intLit = std::get<std::unique_ptr<integer_literal>>(node_ptr->literal);
              std::string s = intLit->value;

              std::cout << "get integer_literal: " << s << std::endl;

              size_t pos = 0;
              while (pos < s.size() && std::isdigit(s[pos])) ++pos;

              std::string suffix = s.substr(pos);
              std::string type;

              if (!suffix.empty()) {
                type = suffix;
              } else {
                type = "i32";
              }
              return type;
            }
            if (std::holds_alternative<std::unique_ptr<float_literal>>(node_ptr->literal)) {
              std::cout << "get float_literal" << std::endl;
              return "f64";
            }
            if (std::holds_alternative<std::unique_ptr<bool>>(node_ptr->literal)) {
              std::cout << "get bool_literal" << std::endl;
              return "bool";
            }
            if (std::holds_alternative<std::unique_ptr<char_literal>>(node_ptr->literal)) {
              return "char";
            }
            if (std::holds_alternative<std::unique_ptr<string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<c_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_c_string_literal>>(node_ptr->literal)) {
              return "str";
            }
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
          auto* expr1 = node_ptr->expression1.get();
          auto* type = getExpressionType(expr1);
          if (auto* path_type = dynamic_cast<TypePathNode*>(type)) {
            std::string name = path_type->toString();
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
          if (!node_ptr->expression) {
            std::cout << "[GroupedExpressionError]: missing expression" << std::endl;
            return "";
          }
          return getExpressionType(node_ptr->expression.get())->toString();
        } else {
          return "";
        }
      }, expr_node->expr);
      if (s == "") std::cout << "failed to get type in expressionwithoutblock" << std::endl;
      if (s != "") {
        types.insert(s);
        std::cout << "type in expression without block: " << s << std::endl;
      }
    }
    if (types.size() != 1) {
      for (auto it = types.begin(); it != types.end(); it++) {
        std::cout << "return type in block expression : " << *it << std::endl;
      }
      if (types.size() == 0) {
        std::cout << "failed getting any type in block expression" << std::endl;
      }
      return false;
    }
    return true;
  }

  std::string get_return_type(BlockExpressionNode* block) {
    std::cout << "enter scope in getting return type in block expression" << std::endl;
    enterScope();
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode" << std::endl;
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        exitScope();
        return temp;
      }
    }
    if (auto* expr_node = dynamic_cast<ExpressionWithoutBlockNode*>(block->expression_without_block.get())) {
      return std::visit([this](auto& node_ptr) -> std::string {
        using T = std::decay_t<decltype(node_ptr)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          if (node_ptr && node_ptr->expression) return getExpressionType(node_ptr->expression.get())->toString();
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IfExpressionNode>>) {
          if (node_ptr) {
            std::string t = get_return_type(node_ptr->then_block.get());
            if (t != "") return t;
            if (node_ptr->else_block) {
              auto res = get_return_type(node_ptr->else_block.get());
              return res;
            }
          }
          if (auto* last_expr = node_ptr->block_expression->expression_without_block.get()) {
            std::string res = getExpressionType(last_expr)->toString();
            return res;
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BlockExpressionNode>>) {
          if (node_ptr) return get_return_type(node_ptr.get());
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>> || std::is_same_v<T, std::unique_ptr<ComparisonExpressionNode>>) {
          return "bool";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
          return node_ptr->pathin_expression->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          if (node_ptr) {
            if (std::holds_alternative<std::unique_ptr<integer_literal>>(node_ptr->literal)) {
              const auto& intLit = std::get<std::unique_ptr<integer_literal>>(node_ptr->literal);
              std::string s = intLit->value;

              std::cout << "get integer_literal: " << s << std::endl;

              size_t pos = 0;
              while (pos < s.size() && std::isdigit(s[pos])) ++pos;

              std::string suffix = s.substr(pos);
              std::string type;

              if (!suffix.empty()) {
                type = suffix;
              } else {
                type = "i32";
              }
              return type;
            }
            if (std::holds_alternative<std::unique_ptr<float_literal>>(node_ptr->literal)) {
              std::cout << "get float_literal" << std::endl;
              return "f64";
            }
            if (std::holds_alternative<std::unique_ptr<bool>>(node_ptr->literal)) {
              std::cout << "get bool_literal" << std::endl;
              return "bool";
            }
            if (std::holds_alternative<std::unique_ptr<char_literal>>(node_ptr->literal)) {
              return "char";
            }
            if (std::holds_alternative<std::unique_ptr<string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<c_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_c_string_literal>>(node_ptr->literal)) {
              return "str";
            }
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
          auto* expr1 = node_ptr->expression1.get();
          auto* type = getExpressionType(expr1);
          if (auto* path_type = dynamic_cast<TypePathNode*>(type)) {
            std::string name = path_type->toString();
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else {
          std::cout << "failed getting return type in get_return_type" << std::endl;
          return "";
        }
      }, expr_node->expr);
    }
    exitScope();
    return "";
  }

  std::string get_return_type_without_changing_scope(BlockExpressionNode* block) {
    std::cout << "enter scope in getting return type in block expression" << std::endl;
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode" << std::endl;
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        return temp;
      }
    }
    if (auto* expr_node = dynamic_cast<ExpressionWithoutBlockNode*>(block->expression_without_block.get())) {
      return std::visit([this](auto& node_ptr) -> std::string {
        using T = std::decay_t<decltype(node_ptr)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          if (node_ptr && node_ptr->expression) return getExpressionType(node_ptr->expression.get())->toString();
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IfExpressionNode>>) {
          if (node_ptr) {
            std::string t = get_return_type(node_ptr->then_block.get());
            if (t != "") return t;
            if (node_ptr->else_block) {
              auto res = get_return_type(node_ptr->else_block.get());
              return res;
            }
          }
          if (auto* last_expr = node_ptr->block_expression->expression_without_block.get()) {
            std::string res = getExpressionType(last_expr)->toString();
            return res;
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BlockExpressionNode>>) {
          if (node_ptr) return get_return_type(node_ptr.get());
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>> || std::is_same_v<T, std::unique_ptr<ComparisonExpressionNode>>) {
          return "bool";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          if (node_ptr) {
            if (std::holds_alternative<std::unique_ptr<integer_literal>>(node_ptr->literal)) {
              const auto& intLit = std::get<std::unique_ptr<integer_literal>>(node_ptr->literal);
              std::string s = intLit->value;

              std::cout << "get integer_literal: " << s << std::endl;

              size_t pos = 0;
              while (pos < s.size() && std::isdigit(s[pos])) ++pos;

              std::string suffix = s.substr(pos);
              std::string type;

              if (!suffix.empty()) {
                type = suffix;
              } else {
                type = "i32";
              }
              return type;
            }
            if (std::holds_alternative<std::unique_ptr<float_literal>>(node_ptr->literal)) {
              std::cout << "get float_literal" << std::endl;
              return "f64";
            }
            if (std::holds_alternative<std::unique_ptr<bool>>(node_ptr->literal)) {
              std::cout << "get bool_literal" << std::endl;
              return "bool";
            }
            if (std::holds_alternative<std::unique_ptr<char_literal>>(node_ptr->literal)) {
              return "char";
            }
            if (std::holds_alternative<std::unique_ptr<string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<c_string_literal>>(node_ptr->literal) ||
              std::holds_alternative<std::unique_ptr<raw_c_string_literal>>(node_ptr->literal)) {
              return "str";
            }
          }
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
          auto* expr1 = node_ptr->expression1.get();
          auto* type = getExpressionType(expr1);
          if (auto* path_type = dynamic_cast<TypePathNode*>(type)) {
            std::string name = path_type->toString();
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
        } else {
          std::cout << "failed getting return type in get_return_type_without_changing_scope" << std::endl;
          return "";
        }
      }, expr_node->expr);
    }
    return "";
  }

  bool is_legal_type(const std::string& type) {
    if (type[0] == '[') {
      std::string act_str = "";
      int pos = 0;
      while (type[pos] == '[') pos++;
      while (type[pos] != ']' && pos < type.length()) {
        act_str += type[pos];
        pos++;
      }
      return is_legal_type(act_str);
    }
    return (type == "i32" || type == "str" || type == "bool" || type == "u32" || type == "usize" || type == "isize" || type == "f32" 
          || type == "bool" || type == "char" || type == "()");
  }

  //在impl中，function可能会有selfParam，需要记录self指向的是什么类型，同时需要记录这个类型的函数和成员来检查methodcall是否合法
  //function本身只需要记录可能存在的self是指向什么类，在Scope中存储这个类的相关信息
  bool check_Item(const ItemNode* expr) {
    //===Function===
    if (auto* function = dynamic_cast<const FunctionNode*>(expr)) {
      FunctionSymbol func_info;
      func_info.name = function->identifier;
      func_info.param_types = function->function_parameter.get();

      if (function->return_type) func_info.return_type = function->return_type->type.get();

      currentScope->insertFunc(function->identifier, func_info);

      if (function->block_expression) {
        std::cout << "checking blockexpression of function in scope : " << currentScope->id << std::endl;
        enterScope();
        declareFunctionParameters(function->function_parameter.get(), currentScope, function->impl_type_name);
        bool ans = check_BlockExpression_without_changing_scope(dynamic_cast<BlockExpressionNode*>(function->block_expression.get()));
        exitScope();
        if (!ans) {
          std::cout << "error in block expression of function : " << function->identifier << std::endl;
          return ans;
        }
      }

      if (function->return_type && function->block_expression) {
        std::cout << "checking if return type mismatch in function : " << function->identifier << std::endl;
        std::string declared_return_type = function->return_type->type->toString();
        if (!currentScope->is_forward_declared(declared_return_type) && !is_legal_type(declared_return_type)) {
          std::cout << "undefined return type : " << declared_return_type << " in function : " << function->identifier << std::endl;
        }
        if (function->block_expression) {
          std::cout << "checking if the return types match in blockexpression" << std::endl;
          std::cout << "current scope : " << currentScope->id << std::endl;
          enterScope();
          declareFunctionParameters(function->function_parameter.get(), currentScope, function->impl_type_name);
          if (!check_return_type_without_changing_scope(function->block_expression.get())) {
            std::cout << "return type mismatch in blockexpression" << std::endl;
            exitScope();
            return false;
          }
          std::cout << "getting return type in blockexpression" << std::endl;
          std::cout << "current scope : " << currentScope->id << std::endl;
          std::string actual_return_type = get_return_type_without_changing_scope(function->block_expression.get());
          size_t pos = actual_return_type.rfind("::");
          if (pos != std::string::npos) {
            actual_return_type = actual_return_type.substr(0, pos);
          }
          std::cout << "declared return type: " << declared_return_type << std::endl;
          std::cout << "actual return type: " << actual_return_type << std::endl;
          if (declared_return_type != "Self" && declared_return_type != "self" && actual_return_type != declared_return_type) {
            std::cout << "[Return Type Error]: function " << function->identifier << std::endl;
            exitScope();
            return false;
          }
        }
      }
      if (function->block_expression) exitScope();
      return true;
    }
    //===Trait===
    if (auto* Trait = dynamic_cast<const TraitNode*>(expr)) {
      //在trait_table里插入对应信息
      if (currentScope->trait_table.find(Trait->identifier) != currentScope->trait_table.end()) {
        std::cerr << "Error: duplicate trait definition: " << Trait->identifier << std::endl;
        return false;
      }

      TraitSymbol traitSym;
      traitSym.name = Trait->identifier;

      for (const auto& item : Trait->associatedItems) {
        if (auto funcPtr = std::get_if<std::unique_ptr<FunctionNode>>(&item->associated_item)) {
          FunctionNode* func = funcPtr->get();

          FunctionSymbol funcSym;
          funcSym.name = func->identifier;
          funcSym.param_types = func->function_parameter.get();
          funcSym.return_type = func->return_type ? func->return_type->type.get() : nullptr;
          funcSym.impl_type_name = std::nullopt;

          bool dup = false;
          for (auto& existing : traitSym.functions) {
            if (existing.name == funcSym.name) {
              std::cerr << "Error: duplicate function " << funcSym.name << " in trait " << Trait->identifier << std::endl;
              dup = true;
              break;
            }
          }
          if (!dup) traitSym.functions.push_back(funcSym);
        }
        else if (auto constPtr = std::get_if<std::unique_ptr<ConstantItemNode>>(&item->associated_item)) {

        }
        else {
          std::cerr << "Warning: unknown associated item in trait " << Trait->identifier << std::endl;
        }
      }

      currentScope->trait_table[Trait->identifier] = std::move(traitSym);
      return true;
    }
    //===Struct===
    if (auto* Struct = dynamic_cast<const StructStructNode*>(expr)) {
      //在Scope中插入变量
      std::cout << "declaring structstruct" << std::endl;
      declareStruct(Struct);
    }
    
    //===constant===
    if (auto* Const = dynamic_cast<const ConstantItemNode*>(expr)) {
      if (Const) {
        ConstantInfo info{Const->identifier.value(), Const->type.get(), Const->expression.get()};
        std::cout << "declaring constant : " << Const->identifier.value() << std::endl;
        currentScope->const_table[Const->identifier.value()] = info;
      }
    }

    //===InherentImplementation===
    if (auto* Impl = dynamic_cast<const InherentImplNode*>(expr)) {
      std::string type = Impl->type->toString();
      std::cout << "type of InherentImplNode : " << type << std::endl;
      if (currentScope->declared_struct.find(type) != currentScope->declared_struct.end()) {//是已经declared过的struct
        enterScope();
        currentScope->possible_self = type;
        for (int i = 0; i < Impl->associated_item.size(); i++) {
          std::cout << "checking the " << i << "th item in impl node" << std::endl;
          if (auto* funcNode = std::get_if<std::unique_ptr<FunctionNode>>(&Impl->associated_item[i]->associated_item)) {
            FunctionNode* function = funcNode->get();

            FunctionSymbol func_info;
            func_info.name = function->identifier;
            func_info.param_types = function->function_parameter.get();

            if (function->return_type) func_info.return_type = function->return_type->type.get();

            currentScope->insertFunc(function->identifier, func_info);

            if (function->block_expression) {
              std::cout << "checking blockexpression of function in scope : " << currentScope->id << std::endl;
              enterScope();
              declareFunctionParameters(function->function_parameter.get(), currentScope, function->impl_type_name);
              bool ans = check_BlockExpression_without_changing_scope(dynamic_cast<BlockExpressionNode*>(function->block_expression.get()));
              exitScope();
              std::cout << "finish checking block expression" << std::endl;
              if (!ans) {
                return ans;
              }
            }
          
            if (function->return_type && function->block_expression) {
              std::cout << "checking if return type mismatch in function : " << function->identifier << std::endl;
              std::string declared_return_type = function->return_type->type->toString();
              if (!currentScope->is_forward_declared(declared_return_type) && !is_legal_type(declared_return_type)) {
                std::cout << "undefined return type : " << declared_return_type << " in function : " << function->identifier << std::endl;
              }
              if (function->block_expression) {
                std::cout << "checking if the return types match in blockexpression" << std::endl;
                std::cout << "current scope : " << currentScope->id << std::endl;
                enterScope();
                declareFunctionParameters(function->function_parameter.get(), currentScope, function->impl_type_name);
                if (!check_return_type_without_changing_scope(function->block_expression.get())) {
                  std::cout << "return type mismatch in blockexpression" << std::endl;
                  exitScope();
                  return false;
                }
                std::cout << "getting return type in blockexpression" << std::endl;
                std::cout << "current scope : " << currentScope->id << std::endl;
                std::string actual_return_type = get_return_type_without_changing_scope(function->block_expression.get());
                size_t pos = actual_return_type.rfind("::");
                if (pos != std::string::npos) {
                  actual_return_type = actual_return_type.substr(0, pos);
                }
                std::cout << "declared return type: " << declared_return_type << std::endl;
                std::cout << "actual return type: " << actual_return_type << std::endl;
                if (declared_return_type != "Self" && declared_return_type != "self" && actual_return_type != declared_return_type) {
                  std::cout << "[Return Type Error]: function " << function->identifier << std::endl;
                  exitScope();
                  return false;
                }
              }
            }

            FunctionSymbol fs;
            fs.name = function->identifier;

            if (function->function_parameter) {
              fs.param_types = function->function_parameter.get();
            }

            if (function->return_type) {
              fs.return_type = function->return_type->type.get();
            }

            fs.impl_type_name = type;
            std::string func_to_declare = fs.impl_type_name.value() + "::" + fs.name;
            currentScope->parent->declared_struct_functions[func_to_declare] = fs;
            std::cout << "declaring function: " << func_to_declare << " in scope: " << currentScope->parent->id << std::endl; 
            std::cout << "size of function_table in scope function added to: " << currentScope->parent->declared_struct_functions.size() << std::endl;
          }
        }
        exitScope();
      } else {
        std::cout << "[Error]: Trying to implement an undeclared struct" << std::endl;
        return false;
      }
    }

    //===TraitImplNode===
    if (auto* TraitImpl = dynamic_cast<const TraitImplNode*>(expr)) {
      std::string traitName = TraitImpl->traitType->toString();
      std::string targetType = TraitImpl->forType->toString();
  
      // 查找 trait 是否存在
      auto it = currentScope->trait_table.find(traitName);
      if (it == currentScope->trait_table.end()) {
        std::cerr << "Error: undefined trait '" << traitName << "' used in implementation for type '" << targetType << "'\n";
        return false;
      }
  
      TraitSymbol& traitSym = it->second;
  
      std::unordered_map<std::string, FunctionNode*> implFunctions;
      for (const auto& assoc : TraitImpl->associatedItems) {
        if (auto funcPtr = std::get_if<std::unique_ptr<FunctionNode>>(&assoc->associated_item)) {
          FunctionNode* func = funcPtr->get();
          implFunctions[func->identifier] = func;
        }
      }
  
      // 检查每个 trait 函数是否被实现
      bool allImplemented = true;
      for (const auto& traitFunc : traitSym.functions) {
        if (implFunctions.find(traitFunc.name) == implFunctions.end()) {
          std::cerr << "Error: trait '" << traitName 
                    << "' requires method '" << traitFunc.name 
                    << "', but it is not implemented in '" << targetType << "'\n";
          allImplemented = false;
        }
      }
      if (allImplemented) {
        std::cout << "all trait are implemented " << std::endl;
      }
  
      // 检查 impl 中是否定义了多余的函数（trait 中未声明）
      for (const auto& [fname, fnode] : implFunctions) {
        bool declaredInTrait = false;
        for (const auto& tf : traitSym.functions) {
          if (tf.name == fname) { declaredInTrait = true; break; }
        }
        if (!declaredInTrait) {
            std::cerr << "Warning: function '" << fname 
                      << "' in impl of '" << traitName 
                      << "' for '" << targetType 
                      << "' is not declared in the trait.\n";
        }
      }
  
      //检查parameter是否匹配
      for (const auto& tf : traitSym.functions) {
        auto implIt = implFunctions.find(tf.name);
        if (implIt != implFunctions.end()) {
          auto implFunc = implIt->second;
          
        }
      }
  
      if (!allImplemented) {
        std::cerr << "Trait implementation for '" << traitName << "' on type '" << targetType << "' is incomplete." << std::endl;
        return false;
      }
  
      currentScope->trait_table[traitName] = traitSym;
  
      return true;
    }

    //===Enum===
    if (auto* Enum = dynamic_cast<const EnumerationNode*>(expr)) {
      std::string base = Enum->identifier;
      for (int i = 0; i < Enum->enum_variants->enum_variants.size(); i++) {
        std::string var_name = base + "::" + Enum->enum_variants->enum_variants[i]->identifier;
        Symbol symbol{var_name, new TypePathNode(var_name), false, false, false};
        currentScope->var_table[var_name] = symbol;
      }
    }

    return true;
  }

  bool check_BlockExpression(const BlockExpressionNode* block_expr) {
    std::cout << "checking blockexpression" << std::endl;
    if (!block_expr) return true;
    enterScope();
    for (int i = 0; i < block_expr->statement.size(); i++) {
      if (!check_Statment(dynamic_cast<StatementNode*>(block_expr->statement[i].get()))) {
        exitScope();
        return false;
      }
    }
    exitScope();
    return true;
  }

  bool check_BlockExpression_without_changing_scope(const BlockExpressionNode* block_expr) {
    std::cout << "checking blockexpression" << std::endl;
    if (!block_expr) return true;
    std::cout << "number of statements : " << block_expr->statement.size() << std::endl;
    for (int i = 0; i < block_expr->statement.size(); i++) {
      if (!check_Statment(dynamic_cast<StatementNode*>(block_expr->statement[i].get()))) {
        return false;
      }
    }
    return true;
  }

  int get_array_length(const ArrayTypeNode* arrType) {
    if (!arrType || !arrType->expression) return -1;
    if (auto *lenLit = dynamic_cast<LiteralExpressionNode*>(arrType->expression.get())) {
      if (auto intLit = std::get_if<std::unique_ptr<integer_literal>>(&lenLit->literal)) {
        std::cout << "array length: " << (*intLit)->value << std::endl;
        return std::stoi((*intLit)->value);
      }
    }
    return -1;
  }

  std::string PathIdentSegmentToString(const PathIdentSegment& segment) {
    switch (segment.type) {
      case PathIdentSegment::ID:
        return segment.identifier.value_or("<null>");
      case PathIdentSegment::super:
        return "super";
      case PathIdentSegment::self:
        return "self";
      case PathIdentSegment::Self:
        return "Self";
      case PathIdentSegment::crate:
        return "crate";
      case PathIdentSegment::$crate:
        return "$crate";
      default:
        return "<unknown>";
    }
  }

  std::string TypePathFnInputsToString(const TypePathFnInputs& inputs) {
    std::string s = "(";
    for (size_t i = 0; i < inputs.types.size(); ++i) {
      s += TypetoString(inputs.types[i].get());
      if (i + 1 < inputs.types.size()) s += ", ";
    }
    s += ")";
    return s;
  }

  std::string TypePathFnToString(const TypePathFn& fn) {
    std::string s;
    if (fn.type_path_fn_inputs) {
      s += TypePathFnInputsToString(*fn.type_path_fn_inputs);
    } else {
      s += "()";
    }

    if (fn.type_no_bounds) {
      s += " -> " + TypetoString(fn.type_no_bounds.get());
    }
    return s;
  }

  std::string TypePathSegmentToString(const TypePathSegment& segment) {
    std::string s;
    if (segment.path_ident_segment) {
      s += PathIdentSegmentToString(*segment.path_ident_segment);
    }

    if (segment.type_path_fn) {
      s += TypePathFnToString(*segment.type_path_fn);
    }

    return s;
  }

  std::string TypePathToString(const TypePath* path) {
    std::string s;
    bool first = true;
    for (const auto& seg : path->segments) {
      if (!first) s += "::";
      s += TypePathSegmentToString(*seg);
      first = false;
    }
    return s;
  }


  std::string TypetoString(const TypeNode* type) {
    if (!type) return "<null>";

    switch (type->node_type) {
      case TypeType::ParenthesizedType_node: {
        auto* t = dynamic_cast<const ParenthesizedTypeNode*>(type);
        return "(" + TypetoString(t->type.get()) + ")";
      }
      case TypeType::TypePath_node: {
        auto* t = dynamic_cast<const TypePathNode*>(type);
        return t->type_path ? TypePathToString(t->type_path.get()) : "<null>";
      }
      case TypeType::TupleType_node: {
        auto* t = dynamic_cast<const TupleTypeNode*>(type);
        std::string s = "(";
        for (size_t i = 0; i < t->types.size(); i++) {
          s += TypetoString(t->types[i].get());
          if (i + 1 < t->types.size()) s += ", ";
        }
        s += ")";
        return s;
      }
      case TypeType::NeverType_node: {
        return "!";
      }
      case TypeType::ArrayType_node: {
        const ArrayTypeNode* t = dynamic_cast<const ArrayTypeNode*>(type);
        return "[" + TypetoString(t->type.get()) + "; " + std::to_string(get_array_length(t)) + "]";
      }
      case TypeType::SliceType_node: {
        auto* t = dynamic_cast<const SliceTypeNode*>(type);
        return "[" + TypetoString(t->type.get()) + "]";
      }
      case TypeType::InferredType_node: {
        return "_";
      }
      default: return "<unknown_type>";
    }
  }

  bool type_equal(TypeNode* a, TypeNode* b) {
    if (!a || !b) {
      std::cout << "as least one of the typenodes is nullptr" << std::endl;
      return false;
    }

    if (auto arrA = dynamic_cast<ArrayTypeNode*>(a)) {
      std::cout << "checking if arraytypenodes are equal" << std::endl;
      auto arrB = dynamic_cast<ArrayTypeNode*>(b);
      if (!arrB) return false;
      return check_arrayType(arrA, arrB, currentScope);
    }
    if (auto pathA = dynamic_cast<ReferenceTypeNode*>(a)) {
      auto pathB = dynamic_cast<ReferenceTypeNode*>(b);
      if (!pathB) return false;
      return type_equal(pathA->type.get(), pathB->type.get());
    }
    std::cout << "checking string to check if the types are equal" << std::endl;
    return TypetoString(a) == TypetoString(b); 
  }

  TypeNode* getExpressionType(ExpressionNode* expr) {
    if (auto* indexExpr = dynamic_cast<IndexExpressionNode*>(expr)) {
      TypeNode* arrayType = getExpressionType(indexExpr->base.get());
      if (auto* arr = dynamic_cast<ArrayTypeNode*>(arrayType)) {
        return arr->type.get();
      }
    }
    if (auto* borrowExpr = dynamic_cast<BorrowExpressionNode*>(expr)) {
      TypeNode* type = getExpressionType(borrowExpr->expression.get());
      auto res = std::make_unique<ReferenceTypeNode>(std::unique_ptr<TypeNode>(type), borrowExpr->if_mut, 0, 0);
      return res.release();
    }
    if (auto* pathExpr = dynamic_cast<PathExpressionNode*>(expr)) {
      std::string path = pathExpr->toString();
      std::cout << "path in getting expression type : " << path << std::endl;
      std::string path_pattern = "IdentifierPattern(" + path + ")";
      if (!currentScope->lookupVar(path_pattern) && !currentScope->lookupVar(path)) {
        std::cout << "Variable not found: " << path << " in scope : " << currentScope->id << std::endl;
        std::cout << "var_table size: " << currentScope->var_table.size() << std::endl;
      }
      TypeNode* t = currentScope->lookupVar(path_pattern) ? currentScope->lookupVar(path_pattern)->type : currentScope->lookupVar(path)->type;
      std::cout << "type of pathexpression got : " << t->toString() << std::endl;
      return t;
    }
    if (auto* arrayExpr = dynamic_cast<ArrayExpressionNode*>(expr)) {
      std::cout << "getting type of arrayExpression" << std::endl;
      TypeNode* t = getExpressionType(arrayExpr->expressions[0].get());
      ExpressionNode* e = arrayExpr->expressions[1].get();
      if (arrayExpr->type == ArrayExpressionType::LITERAL) {
        auto res = new ArrayTypeNode(t, new LiteralExpressionNode(new integer_literal(std::to_string(arrayExpr->expressions.size())), 0, 0), 0, 0);
        return res;
      } else {
        std::cout << "get Literal type of arrayExpression" << std::endl;
        std::cout << "type of elements in arrayExpression : " << t->toString() << std::endl;
        if (auto* lenlit = dynamic_cast<LiteralExpressionNode*>(arrayExpr->expressions[1].get())) {
          auto res = new ArrayTypeNode(t, arrayExpr->expressions[1].get(), 0, 0);
          return res;
        } else {
          ExpressionNode* len = new LiteralExpressionNode(new integer_literal(std::to_string(-1)), 0, 0);
          auto res = new ArrayTypeNode(t, len, 0, 0);
          return res;
        }
      }
    }
    if (auto* litExpr = dynamic_cast<LiteralExpressionNode*>(expr)) {
      std::cout << "getting type of LiteralExpression" << std::endl;

      if (std::holds_alternative<std::unique_ptr<integer_literal>>(litExpr->literal)) {
        const auto& intLit = std::get<std::unique_ptr<integer_literal>>(litExpr->literal);
        std::string s = intLit->value;
          
        std::cout << "get integer_literal: " << s << std::endl;
          
        size_t pos = 0;
        while (pos < s.size() && std::isdigit(s[pos])) ++pos;
          
        std::string suffix = s.substr(pos);
        std::string type;
          
        if (!suffix.empty()) {
          type = suffix;
        } else {
          type = "i32";
        }
        return new TypePathNode(type);
      }
      if (std::holds_alternative<std::unique_ptr<float_literal>>(litExpr->literal)) {
        std::cout << "get float_literal" << std::endl;
        return new TypePathNode("f64");
      }
      if (std::holds_alternative<std::unique_ptr<bool>>(litExpr->literal)) {
        std::cout << "get bool_literal" << std::endl;
        return new TypePathNode("bool");
      }
      if (std::holds_alternative<std::unique_ptr<char_literal>>(litExpr->literal)) {
        return new TypePathNode("char");
        std::cout << "get char_literal" << std::endl;
      }
      if (std::holds_alternative<std::unique_ptr<string_literal>>(litExpr->literal) ||
        std::holds_alternative<std::unique_ptr<raw_string_literal>>(litExpr->literal) ||
        std::holds_alternative<std::unique_ptr<c_string_literal>>(litExpr->literal) ||
        std::holds_alternative<std::unique_ptr<raw_c_string_literal>>(litExpr->literal)) {
        return new ReferenceTypeNode(new TypePathNode("str"), false, 0, 0);
      }
    }
    if (auto* index_expr = dynamic_cast<IndexExpressionNode*>(expr)) {
      std::cout << "getting type in indexexpression" << std::endl;
      if (auto* path = dynamic_cast<PathExpressionNode*>(index_expr->base.get())) {
        if (auto* symbol = currentScope->lookupVar(path->toString())) {
          std::cout << "path in indexpression : " << path->toString() << std::endl;
          if (auto* array = dynamic_cast<ArrayTypeNode*>(symbol->type)) {
            return array->type.get();
          } else if (auto* ref = dynamic_cast<ReferenceTypeNode*>(symbol->type)) {
            if (auto* array = dynamic_cast<ArrayTypeNode*>(ref->type.get())) {
              return array->type.get();
            }
          }
          return symbol->type;
        } else {
          std::cout << "path not found in indexexpression: " << path->toString() << std::endl;
          return nullptr;
        }
      } else if (auto* method_call_expr = dynamic_cast<MethodCallExpressionNode*>(index_expr->base.get())) {
        std::cout << "the base in indexexpression is method call expression" << std::endl;
        if (auto* path_expr = dynamic_cast<PathExpressionNode*>(method_call_expr->expression.get())) {
          if (path_expr->toString() == "self" || path_expr->toString() == "Self") {
            if (currentScope->possible_self == "") {
              std::cout << "invalid self" << std::endl;
              return nullptr;
            } else {
              if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
                std::string item_name = method_call_expr->PathtoString();
                for (int i = 0; i < structInfo->fields.size(); i++) {
                  if (structInfo->fields[i].name == item_name) {
                    auto* t = structInfo->fields[i].type;
                    if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                      return array->type.get();
                    }
                  }
                }
                return nullptr;
              } else {
                std::cout << "invalid base in MethodCallExpression : " << currentScope->possible_self << std::endl;
                return nullptr;
              }
            }
          } else {
            if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
              std::string item_name = method_call_expr->PathtoString();
              for (int i = 0; i < structInfo->fields.size(); i++) {
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                    return array->type.get();
                  }
                }
              }
              return nullptr;
            } else {
              std::cout << "invalid base in MethodCallExpression : " << currentScope->possible_self << std::endl;
              return nullptr;
            }
          }
        }
      } else if (auto* field_expr = dynamic_cast<FieldExpressionNode*>(index_expr->base.get())) {
        std::cout << "the base in indexexpression is field expression" << std::endl;
        if (auto* path_expr = dynamic_cast<PathExpressionNode*>(field_expr->expression.get())) {
          if (path_expr->toString() == "self" || path_expr->toString() == "Self") {
            if (currentScope->possible_self == "") {
              std::cout << "invalid self" << std::endl;
              return nullptr;
            } else {
              if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
                std::string item_name = field_expr->identifier.id;
                for (int i = 0; i < structInfo->fields.size(); i++) {
                  if (structInfo->fields[i].name == item_name) {
                    auto* t = structInfo->fields[i].type;
                    if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                      return array->type.get();
                    }
                  }
                }
                std::cout << "unknown item : " << item_name << "in struct : " << path_expr->toString() << std::endl; 
                return nullptr;
              } else {
                std::cout << "invalid base in FieldExpression : " << currentScope->possible_self << std::endl;
                return nullptr;
              }
            }
          } else {
            std::string item_name = field_expr->identifier.id;
            if (auto* structInfo = currentScope->lookupStruct(path_expr->toString())) {
              for (int i = 0; i < structInfo->fields.size(); i++) {
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                    return array->type.get();
                  }
                }
              }
              std::cout << "unknown item : " << item_name << "in struct : " << path_expr->toString() << std::endl;
              return nullptr;
            } else {
              std::cout << "invalid base in FieldExpression : " << currentScope->possible_self << std::endl;
              return nullptr;
            }
          }
        }
      } else {
        std::cout << "unknown type in indexexpression" << std::endl;
      }
    }
    //这里最后输出的结果是i32，有一个i32被当成TypePathNode
    if (auto* logic_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr)) {
      std::cout << "getting type of ArithmeticOrLogicalExpressionNode" << std::endl;
      auto* expr1 = logic_expr->expression1.get();
      auto* type = getExpressionType(expr1);
      if (auto* path_type = dynamic_cast<TypePathNode*>(type)) {
        std::string name = path_type->toString();
        if (is_legal_type(name)) return new TypePathNode(name);
        std::cout << "looking up var : " << name << std::endl;
        auto* info = currentScope->lookupVar(name);
        if (!info) std::cout << "var not found : " << name << " in scope : " << currentScope->id << std::endl;
        return info->type;
      } else {
        return type;
      }
    }
    if (auto* ewb = dynamic_cast<ExpressionWithoutBlockNode*>(expr)) {
      return std::visit([this](auto& node_ptr) -> TypeNode* {
          using T = std::decay_t<decltype(node_ptr)>;

          if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<OperatorExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
            return getExpressionType(node_ptr->expression.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<TupleExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<TupleIndexingExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<MethodCallExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<RangeExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<UnderscoreExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
          else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
            return getExpressionType(node_ptr.get());
          }
      }, ewb->expr);
    } else if (auto* call = dynamic_cast<CallExpressionNode*>(expr)) {
      std::cout << "getting callexpression" << std::endl;
      if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
        std::string name = path->toString();
        std::cout << "func name in call expression: " << name << std::endl;
        auto* func_info = currentScope->lookupFunc(name);
        if (!func_info) std::cout << "function not found in Scope: " << currentScope->id << std::endl;
        std::cout << "get type of callexpression : " << func_info->return_type->toString() << std::endl;
        return func_info->return_type;
      } else {
        std::cout << "fail to get type of callexpression" << std::endl;
        return nullptr;
      }
    } else if (auto* typecast = dynamic_cast<TypeCastExpressionNode*>(expr)) {
      return typecast->type.get();
    } else if (auto* index = dynamic_cast<IndexExpressionNode*>(expr)) {
      if (auto* path = dynamic_cast<PathExpressionNode*>(index->base.get())) {
        auto* type = currentScope->lookupVar(path->toString());
        if (auto* arrayType = dynamic_cast<ArrayTypeNode*>(type->type)) {
          std::cout << "getting array element type in index expression : " << arrayType->type->toString() << std::endl;
          return arrayType->type.get();
        }
      }
    } else if (auto* paren = dynamic_cast<GroupedExpressionNode*>(expr)) {
      std::cout << "get type of grouped expression : " << getExpressionType(paren->expression.get())->toString() << std::endl;
      return getExpressionType(paren->expression.get());
    }
    // TODO: 其他表达式类型...
    std::cout << "other type of expression" << std::endl;
    return nullptr;
  }

  bool check_array_assignment(LetStatement& letStmt) {
    if (!letStmt.type || !letStmt.expression) return true;

    auto* declaredType = letStmt.type.get();

    auto* rhsType = getExpressionType(letStmt.expression.get());
    std::cout << "finish getting type of rhs" << std::endl;
    if (!rhsType) {
      std::cout << "Error in getting type of rhs" << std::endl; 
      return false;
    }

    if (!type_equal(declaredType, rhsType)) {
      std::cout << "Type mismatch in let statement: declared "
                << TypetoString(declaredType) 
                << ", but found " 
                << TypetoString(rhsType) << std::endl;
      return false;
    }

    return true;
  }

  bool check_arrayType(ArrayTypeNode* lhs, ArrayTypeNode* rhs, Scope* currentScope) {
    if (!lhs || !rhs) return false;
    auto* lhs_inner = dynamic_cast<ArrayTypeNode*>(lhs->type.get());
    auto* rhs_inner = dynamic_cast<ArrayTypeNode*>(rhs->type.get());
    if (lhs_inner && rhs_inner) {
      int lhsLen = -1, rhsLen = -1;
    
      if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(lhs->expression.get())) {
        if (auto intLit = std::get_if<std::unique_ptr<integer_literal>>(&lenLit->literal)) {
          lhsLen = std::stoi((*intLit)->value);
        }
      } else if (auto* path = dynamic_cast<PathExpressionNode*>(lhs->expression.get())) {
        std::string name = path->toString();
        if (auto* info = currentScope->lookupConst(name)) {
          if (auto* lit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
            std::string literal = lit->toString();
            if (std::all_of(literal.begin(), literal.end(), ::isdigit)) {
              lhsLen = std::stoi(literal);
            }
            else {
              std::cout << "invalid const type for array size: " << name << std::endl;
              return false;
            }
          }
        }
      }

      if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(rhs->expression.get())) {
        if (auto intLit = std::get_if<std::unique_ptr<integer_literal>>(&lenLit->literal)) {
          rhsLen = std::stoi((*intLit)->value);
        }
      } else if (auto* path = dynamic_cast<PathExpressionNode*>(rhs->expression.get())) {
        std::string name = path->toString();
        if (auto* info = currentScope->lookupConst(name)) {
          if (auto* lit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
            std::string literal = lit->toString();
            if (std::all_of(literal.begin(), literal.end(), ::isdigit)) {
              rhsLen = std::stoi(literal);
            }
            else {
              std::cout << "invalid const type for array size: " << name << std::endl;
              return false;
            }
          }
        }
      }

      if (lhsLen != -1 && rhsLen != -1 && lhsLen != rhsLen) {
        std::cout << "Array length mismatch at one dimension: expected " 
                  << lhsLen << ", got " << rhsLen << std::endl;
        return false;
      }

      return check_arrayType(lhs_inner, rhs_inner, currentScope);
    } 

    if ((lhs_inner && !rhs_inner) || (!lhs_inner && rhs_inner)) {
      std::cout << "Array dimension mismatch" << std::endl;
      return false;
    }

    int lhsLen = -1, rhsLen = -1;

    if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(lhs->expression.get())) {
      if (auto intLit = std::get_if<std::unique_ptr<integer_literal>>(&lenLit->literal)) {
        lhsLen = std::stoi((*intLit)->value);
      }
    } else if (auto* path = dynamic_cast<PathExpressionNode*>(lhs->expression.get())) {
      std::string name = path->toString();
      if (auto* info = currentScope->lookupConst(name)) {
        if (auto* lit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
          std::string literal = lit->toString();
          if (std::all_of(literal.begin(), literal.end(), ::isdigit))
            lhsLen = std::stoi(literal);
        }
      }
    }

    if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(rhs->expression.get())) {
      if (auto intLit = std::get_if<std::unique_ptr<integer_literal>>(&lenLit->literal)) {
        rhsLen = std::stoi((*intLit)->value);
      }
    } else if (auto* path = dynamic_cast<PathExpressionNode*>(rhs->expression.get())) {
      std::string name = path->toString();
      if (auto* info = currentScope->lookupConst(name)) {
        if (auto* lit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
          std::string literal = lit->toString();
          if (std::all_of(literal.begin(), literal.end(), ::isdigit)) {
            rhsLen = std::stoi(literal);
          }
        }
      }
    }

    if (lhsLen != -1 && rhsLen != -1 && lhsLen != rhsLen) {
      std::cout << "Array length mismatch at final dimension: expected "
                << lhsLen << ", got " << rhsLen << std::endl;
      return false;
    }

    return true;
  }


  bool check_LetStatement(const StatementNode* expr) {
    std::cout << "checking LetStatement" << std::endl;
    if (!expr) {
      std::cout << "error: no letstatement" << std::endl;
      return false;
    }
    if (expr->let_statement == nullptr) return true;
    LetStatement& letStatement = *(expr->let_statement);

    try {
      std::cout << "try to declare var : " << letStatement.pattern->toString()  << " in scope :" << currentScope->id << std::endl;
      declareVariable(letStatement.pattern->toString(), letStatement.type.get(), letStatement.get_if_mutable());
    } catch (const std::exception& e) {
      std::cerr << "[Declare Error in LetStatement] : " << e.what() << std::endl;
    }
    //Array:检查类型和数量
    if (auto *d = dynamic_cast<ArrayTypeNode*>(letStatement.type.get())) {
      //检查数量
      auto *rhs = dynamic_cast<ArrayExpressionNode*>(letStatement.expression.get());
      auto *call_expr = dynamic_cast<CallExpressionNode*>(letStatement.expression.get());
      if (!rhs && !call_expr) {
        std::cout << "Expected array expression in array assignment" << std::endl;
        return false;
      }
      if (call_expr) {
        return true;
      }
      int declaredLength = -1;
      if (auto *lenLit = dynamic_cast<LiteralExpressionNode*>(d->expression.get())) {
        if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
          declaredLength = std::stoi(intLit->value);
        } else {
          std::cout << "wrong type of length in initializer" << std::endl;
          return false;
        }
      }
      int itemLength = -1;
      if (rhs->type == ArrayExpressionType::LITERAL) {
        std::cout << "get literal array expression" << std::endl;
        itemLength = rhs->expressions.size();
      } else {
        if (rhs->expressions.size() != 2) {
          std::cout << "wrong number of expressions for repeat type of arrayExpression" << std::endl;
          return false;
        }
        auto *lengthLiteral = dynamic_cast<LiteralExpressionNode*>(rhs->expressions[1].get());//得到表示repeat次数的LiteralExpression
        if (!lengthLiteral) { 
          std::cout << "the type of the second expression in repeat ArrayExpression is not literalexpression" << std::endl;
          if (auto *path = dynamic_cast<PathExpressionNode*>(rhs->expressions[1].get())) {
            std::string var_name = path->toString();
            auto* info = currentScope->lookupConst(var_name);
            if (info) {
              std::cout << "constant: " << var_name << " found" << std::endl;
              if (auto* lit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
                std::string literal = lit->toString();
                for (int i = 0; i < literal.size(); i++) {
                  if (!isdigit(literal[i])) {
                    std::cout << "invalid type of const in array definition" << std::endl;
                    return false;
                  }
                }
                itemLength = std::stoi(literal);
              } else {
                std::cout << "invalid type of const in array definition" << std::endl;
                return false;
              }
            }
          }
        }
        if (lengthLiteral) {
          std::cout << "the second expression in repeat arrayexpression is leteralexpression" << std::endl;
          auto& repeat_time = std::get<std::unique_ptr<integer_literal>>(lengthLiteral->literal);
          itemLength = std::stoi(repeat_time->value);
        }
      }
      std::cout << "declaredLength : " << declaredLength << std::endl;
      std::cout << "itemLength : " << itemLength << std::endl;
      if (declaredLength != -1 && declaredLength != itemLength && itemLength != -1) {
        std::cout << "Array length mismatch: declared length = " << std::to_string(declaredLength) 
                  << ", but initializer has " << itemLength << std::endl;
        return false;
      }
      //检查类型
      auto* array = dynamic_cast<ArrayTypeNode*>(getExpressionType(rhs));
      if (!array || !check_arrayType(array, d, currentScope)) {
        std::cout << "arrayType mismatch" << std::endl;
      }
      //if (auto *innerType = dynamic_cast<ArrayTypeNode*>(d->type.get())) {
      //  // 嵌套数组
      //  for (int i = 0; i < rhs->expressions.size(); ++i) {
      //    auto *innerArr = dynamic_cast<ArrayExpressionNode*>(rhs->expressions[i].get());
      //    if ((!innerArr && rhs->type == ArrayExpressionType::LITERAL) || (i == 0 && !innerArr && rhs->type == ArrayExpressionType::REPEAT)) {
      //      std::cout << "Expected nested array at element " << i << std::endl;
      //      return false;
      //    }
      //    int innerLen = -1;
      //    if (innerArr->type == ArrayExpressionType::LITERAL) {
      //      int innerLen = innerArr->expressions.size();
      //    }
      //    auto *lengthlit = dynamic_cast<LiteralExpressionNode*>(innerArr->expressions[1].get());
      //    if (innerArr->type == ArrayExpressionType::REPEAT) {
      //      if (lengthlit) {
      //        auto& repeat = std::get<std::unique_ptr<integer_literal>>(lengthlit->literal);
      //        innerLen = std::stoi(repeat->value); 
      //      } else {
      //        if (auto *path = dynamic_cast<PathExpressionNode*>(innerArr->expressions[1].get())) {
      //          std::string var_name = path->toString();
      //          auto* info = currentScope->lookupConst(var_name);
      //          if (info) {
      //            std::cout << "constant: " << var_name << " found" << std::endl;
      //            if (auto* lit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
      //              std::string literal = lit->toString();
      //              for (int i = 0; i < literal.size(); i++) {
      //                if (!isdigit(literal[i])) {
      //                  std::cout << "invalid type of const in array definition" << std::endl;
      //                  return false;
      //                }
      //              }
      //              itemLength = std::stoi(literal);
      //            } else {
      //              std::cout << "invalid type of const in array definition" << std::endl;
      //              return false;
      //            }
      //          }
      //        }
      //      }
      //    }
      //    int expectedInnerLen = -1;
      //    if (auto *innerLenLit = dynamic_cast<LiteralExpressionNode*>(innerType->expression.get())) {
      //      if (auto innerIntLit = std::get_if<std::unique_ptr<integer_literal>>(&innerLenLit->literal)) {
      //        expectedInnerLen = std::stoi((*innerIntLit)->value);
      //      }
      //    }
      //  
      //    if (expectedInnerLen != -1 && expectedInnerLen != innerLen) {
      //      std::cout << "Inner array length mismatch at index " << i
      //                << ": expected " << expectedInnerLen
      //                << ", got " << innerLen << std::endl;
      //      return false;
      //    }
      //  }
      //}
    }
    std::cout << "finish checking let statement" << std::endl;
    return true;
  }

  std::vector<ExpressionNode*> get_item_in_logic_expr(const ArithmeticOrLogicalExpressionNode* expr) {
    std::vector<ExpressionNode*> res;
    auto* expr1 = expr->expression1.get();
    auto* expr2 = expr->expression2.get();
    if (auto* arith_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr1)) {
      std::vector<ExpressionNode*> vec1 = get_item_in_logic_expr(arith_expr);
      res.insert(res.end(), vec1.begin(), vec1.end());
    } else {
      res.push_back(expr1);
    }
    if (auto* arith_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr2)) {
      std::vector<ExpressionNode*> vec2 = get_item_in_logic_expr(arith_expr);
      res.insert(res.end(), vec2.begin(), vec2.end());
    } else {
      res.push_back(expr2);
    }
    return res;
  }

  std::vector<TypeNode*> get_logic_expr_types(const ArithmeticOrLogicalExpressionNode* expr) {
    std::cout << "getting types in logic_expr" << std::endl;
    std::vector<TypeNode*> res;
    auto ex = get_item_in_logic_expr(expr);
    for (int i = 0; i < ex.size(); i++) {
      res.push_back(getExpressionType(ex[i]));
      std::cout << getExpressionType(ex[i])->toString() << std::endl;
    }
    return res;
  }

  bool check_expression(const ExpressionNode* expr) {
    if (auto *d = dynamic_cast<const ArithmeticOrLogicalExpressionNode*>(expr)) {
      std::cout << "checking ArithmeticOrLogicalExpressionNode" << std::endl;
      auto types = get_logic_expr_types(d);
      std::unordered_set<std::string> t;
      for (int i = 0; i < types.size(); i++) {
        t.insert(types[i]->toString());
      }
      if (t.size() != 1) return false;
    } else if (auto *d = dynamic_cast<const PredicateLoopExpressionNode*>(expr)) {
      std::cout << "checking predicate loop expression" << std::endl;
      return check_BlockExpression(d->block_expression.get());
    } else if (auto *d = dynamic_cast<const IfExpressionNode*>(expr)) {
      std::cout << "checking if expression" << std::endl;
      if (d->block_expression) {
        if (!check_BlockExpression(d->block_expression.get())) return false;
      }
      if (d->else_block) {
        if (!check_BlockExpression(d->else_block.get())) return false;
      }
      if (d->else_if) {
        if (!check_expression(d->else_if)) return false;
      }
      return true;
    } else if (auto *d = dynamic_cast<const ComparisonExpressionNode*>(expr)) {
      std::cout << "checking ComparisonExpressionNode" << std::endl;
      if (!check_ComparisonExpression(d)) return false;
    } else if (auto *d = dynamic_cast<const LazyBooleanExpressionNode*>(expr)) {
      if (!isSameDerived(d->expression1, d->expression2)) return false;
    } else if (auto *d = dynamic_cast<const CompoundAssignmentExpressionNode*>(expr)) {
      //检查expr1是否mutable
      auto* ex = d->expression1.get();
      if (auto* path_expression = dynamic_cast<PathExpressionNode*>(ex)) {
        std::string id = std::visit([](auto &ptr) {
          using T = std::decay_t<decltype(ptr)>;
          if constexpr (std::is_same_v<T, std::unique_ptr<PathInExpression>>) {
            std::cout << "get PathInExpression" << std::endl;
            return ptr->toString();
          } else if constexpr (std::is_same_v<T, std::unique_ptr<QualifiedPathInExpression>>) {
            std::cout << "get QualifiedPathInExpression" << std::endl;
            return ptr->toString();
          } else {
            return "<unknown pathexpression>";
          }
        }, path_expression->path);
        std::string pattern_id = "IdentifierPattern(" + id + ")";
        std::cout << "checking pattern : " << pattern_id << std::endl;
        auto *symbol = currentScope->lookupVar(pattern_id) ? currentScope->lookupVar(pattern_id) : currentScope->lookupVar(id);
        if (!symbol) std::cout << "the var has not been declared" << std::endl;
        if (symbol) std::cout << "get declared variable: " << id << std::endl;
        if (!symbol->isMutable) std::cout << "the var is not mutable" << std::endl;
        return symbol->isMutable;
      }
    } else if (auto *d = dynamic_cast<const AssignmentExpressionNode*>(expr)) {
      std::cout << "checking assignment expression node" << std::endl;
      if (auto *expr1 = dynamic_cast<const IndexExpressionNode*>(d->expression1.get())) {
        //检查ArrayPattern是否mutable
        if (auto *path_expression = dynamic_cast<PathExpressionNode*>(expr1->base.get())) {
          std::string id = std::visit([](auto &ptr) {
            using T = std::decay_t<decltype(ptr)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<PathInExpression>>) {
              std::cout << "get PathInExpression" << std::endl;
              return ptr->toString();
            } else if constexpr (std::is_same_v<T, std::unique_ptr<QualifiedPathInExpression>>) {
              std::cout << "get QualifiedPathInExpression" << std::endl;
              return ptr->toString();
            } else {
              return "<unknown pathexpression>";
            }
          }, path_expression->path);
          std::string pattern_id = "IdentifierPattern(" + id + ")";
          std::cout << "checking pattern : " << pattern_id << std::endl;
          auto *symbol = currentScope->lookupVar(pattern_id) ? currentScope->lookupVar(pattern_id) : currentScope->lookupVar(id);
          if (!symbol) std::cout << "the array has not been declared" << std::endl;
          if (!symbol->isMutable) std::cout << "the array is not mutable" << std::endl;
          return symbol->isMutable;
        } else if (auto *index2 = dynamic_cast<IndexExpressionNode*>(expr1->base.get())) {
          std::cout << "getting 2D array expression" << std::endl;
          if (auto *path_expression = dynamic_cast<PathExpressionNode*>(index2->base.get())) {
            std::string id = std::visit([](auto &ptr) {
              using T = std::decay_t<decltype(ptr)>;
              if constexpr (std::is_same_v<T, std::unique_ptr<PathInExpression>>) {
                std::cout << "get PathInExpression" << std::endl;
                return ptr->toString();
              } else if constexpr (std::is_same_v<T, std::unique_ptr<QualifiedPathInExpression>>) {
                std::cout << "get QualifiedPathInExpression" << std::endl;
                return ptr->toString();
              } else {
                return "<unknown pathexpression>";
              }
            }, path_expression->path);
            std::string pattern_id = "IdentifierPattern(" + id + ")";
            std::cout << "checking pattern : " << pattern_id << std::endl;
            auto *symbol = currentScope->lookupVar(pattern_id) ? currentScope->lookupVar(pattern_id) : currentScope->lookupVar(id);
            if (!symbol) std::cout << "the array has not been declared" << std::endl;
            if (!symbol->isMutable) std::cout << "the array is not mutable" << std::endl;
            return symbol->isMutable;
          }
        } else {
          return false;
        }
      }
    } else if (auto *callExpr = dynamic_cast<const MethodCallExpressionNode*>(expr)) {
      std::cout << "checking method call expression node" << std::endl;
      if (auto *pathExpr = dynamic_cast<const PathExpressionNode*>(callExpr->expression.get())) {
        std::string var = pathExpr->toString();
        std::cout << "path expression in method call expression: " << var << std::endl;
        //var是一个struct
        std::string pattern = "IdentifierPattern(" + var + ")";
        auto *symbol = currentScope->lookupVar(pattern);
        if (!symbol) {
          symbol = currentScope->lookupVar(var);
        }
        if (!symbol) {
          std::cout << "var not found: " << pattern << std::endl;
        }
        bool var_if_mut = symbol->isMutable;
        auto *type = symbol->type;
        if (auto *reference_type = dynamic_cast<ReferenceTypeNode*>(type)) {
          if (auto *t = dynamic_cast<TypePathNode*>(reference_type->type.get())) {
            std::string type_name = t->toString();
            //struct
            std::cout << "finding struct: " << type_name << std::endl;
            if (!currentScope->is_forward_declared(type_name)) {
              std::cout << "struct : " << type_name << "not found" << std::endl;
              
            }
            std::string FunctionCalled = type_name + "::" + callExpr->PathtoString();
            std::cout << "Function to find : " << FunctionCalled << std::endl;
            auto *parameters = currentScope->find_func_param(FunctionCalled);
            if (!parameters) {
              std::cout << "struct function found" << std::endl;
              return false;
            }
            bool required_if_mut = parameters->is_selfParam_mut();
            if (!var_if_mut && required_if_mut) {
              std::cout << "[MethodCall Error]: the function needs item, which is actually not mutable, to be mutable" << std::endl;
              return false;
            } else if (var_if_mut && required_if_mut) {
              std::cout << "the item is mutable" << std::endl;
            }
          }
        } else if (auto *path_type = dynamic_cast<TypePathNode*>(type)) {
          std::string type_name = path_type->toString();
          //struct
          std::cout << "finding struct: " << type_name << std::endl;
         
          if (!currentScope->is_forward_declared(type_name)) {
            std::cout << "struct : " << type_name << "not found" << std::endl;
          }
          std::string FunctionCalled = type_name + "::" + callExpr->PathtoString();
          std::cout << "Function to find : " << FunctionCalled << std::endl;
          if (!currentScope->is_forward_declared(FunctionCalled)) {
            return false;
          }
          std::cout << "struct function found" << std::endl;
          auto *parameters = currentScope->find_func_param(FunctionCalled);
          bool required_if_mut = parameters->is_selfParam_mut();
          if (!var_if_mut && required_if_mut) {
            std::cout << "[MethodCall Error]: the function needs item, which is actually not mutable, to be mutable" << std::endl;
            return false;
          } else if (var_if_mut && required_if_mut) {
            std::cout << "the item is mutable" << std::endl;
          }
        }
      }
    }
    return true;
  }

  void forward_declare(const ItemNode* expr) {
    if (auto* func = dynamic_cast<const FunctionNode*>(expr)) {
      currentScope->symbol_table.functions.insert({func->identifier, func->function_parameter.get()});
    } else if (auto* structstruct = dynamic_cast<const StructStructNode*>(expr)) {
      std::cout << "inserting structstruct : " << structstruct->identifier << std::endl;
      currentScope->symbol_table.structs.insert(structstruct->identifier);
      for (int i = 0; i < structstruct->struct_fields->struct_fields.size(); i++) {
        currentScope->symbol_table.struct_items[{structstruct->identifier, structstruct->struct_fields->struct_fields[i]->identifier}] 
        = structstruct->struct_fields->struct_fields[i]->type->toString();
      }
    } else if (auto* tuplestruct = dynamic_cast<const TupleStructNode*>(expr)) {
      currentScope->symbol_table.structs.insert(tuplestruct->identifier);

    } else if (auto* constant = dynamic_cast<const ConstantItemNode*>(expr)) {
      currentScope->symbol_table.constants.insert(constant->identifier.value());
    } else if (auto* trait = dynamic_cast<const TraitNode*>(expr)) {
      std::string base = trait->identifier;
      std::cout << "base: " << base << std::endl;
      for (int i = 0; i < trait->associatedItems.size(); i++) {
        if (auto* constantPtr = std::get_if<std::unique_ptr<ConstantItemNode>>(&trait->associatedItems[i]->associated_item)) {
          ConstantItemNode* node = constantPtr->get();
          std::string constant = base + "::" + node->identifier.value();
          std::cout << "inserting : " << constant << std::endl;
          currentScope->symbol_table.constants.insert(constant);
        } else if (auto* funcPtr = std::get_if<std::unique_ptr<FunctionNode>>(&trait->associatedItems[i]->associated_item)) {
          FunctionNode* node = funcPtr->get();
          std::string func = base + "::" + node->identifier;
          std::cout << "inserting : " << func << std::endl;
          currentScope->symbol_table.functions[func] = node->function_parameter.get();
        }
      }
    } else if (auto* inherent_impl = dynamic_cast<const InherentImplNode*>(expr)) {
      std::string base = inherent_impl->type->toString();
      std::cout << "base: " << base << std::endl;
      for (int i = 0; i < inherent_impl->associated_item.size(); i++) {
        if (auto* constantPtr = std::get_if<std::unique_ptr<ConstantItemNode>>(&inherent_impl->associated_item[i]->associated_item)) {
          ConstantItemNode* node = constantPtr->get();
          std::string constant = base + "::" + node->identifier.value();
          std::cout << "inserting : " << constant << std::endl;
          currentScope->symbol_table.constants.insert(constant);
        } else if (auto* funcPtr = std::get_if<std::unique_ptr<FunctionNode>>(&inherent_impl->associated_item[i]->associated_item)) {
          FunctionNode* node = funcPtr->get();
          std::string func = base + "::" + node->identifier;
          std::cout << "inserting : " << func << std::endl;
          currentScope->symbol_table.functions[func] = node->function_parameter.get();
        }
      }
    } else if (auto* Enum = dynamic_cast<const EnumerationNode*>(expr)) {
      std::string base = Enum->identifier;
      for (int i = 0; i < Enum->enum_variants->enum_variants.size(); i++) {
        std::string var_name = base + "::" + Enum->enum_variants->enum_variants[i]->identifier;
        Symbol symbol{var_name, new TypePathNode(var_name), false, false, false};
        currentScope->var_table[var_name] = symbol;
        std::cout << "forward declaring: " << var_name << std::endl;
      }
    } else if (auto* TraitImpl = dynamic_cast<const TraitImplNode*>(expr)) {
      std::string base = TraitImpl->forType->toString();
      for (int i = 0; i < TraitImpl->associatedItems.size(); i++) {
        if (auto* constantPtr = std::get_if<std::unique_ptr<ConstantItemNode>>(&TraitImpl->associatedItems[i]->associated_item)) {
          ConstantItemNode* node = constantPtr->get();
          std::string constant = base + "::" + node->identifier.value();
          std::cout << "inserting : " << constant << std::endl;
          currentScope->symbol_table.constants.insert(constant);
        } else if (auto* funcPtr = std::get_if<std::unique_ptr<FunctionNode>>(&TraitImpl->associatedItems[i]->associated_item)) {
          FunctionNode* node = funcPtr->get();
          std::string func = base + "::" + node->identifier;
          std::cout << "inserting : " << func << std::endl;
          currentScope->symbol_table.functions[func] = node->function_parameter.get();
        }
      }
    }
  }
 
  bool check() {
    for (int i = 0; i < ast.size(); i++) {
      if (auto item = dynamic_cast<ItemNode*>(ast[i].get())) {
        std::cout << "forward declaring item which is the " << i << "th node in ast" << std::endl;
        forward_declare(item);
      }
    }

    std::cout << "declared structs : " << std::endl;
    for (auto it = currentScope->symbol_table.structs.begin(); it != currentScope->symbol_table.structs.end(); it++) {
      std::cout << *it << std::endl;
    }

    std::cout << "declared functions : " << std::endl;
    for (auto it = currentScope->symbol_table.functions.begin(); it != currentScope->symbol_table.functions.end(); it++) {
      std::cout << it->first << std::endl;
    }
    
    for (int i = 0; i < ast.size(); i++) {
      std::cout << "checking the " << i + 1 << "th ASTNode" << std::endl;
      if (auto *d = dynamic_cast<StatementNode*>(ast[i].get())) {
        std::cout << "checking StatementNode" << std::endl;
        if (d->let_statement != nullptr) {
          if (!check_LetStatement(d)) return false;
        }
      }
      if (auto *d = dynamic_cast<ItemNode*>(ast[i].get())) {
        std::cout << "checking item node" << std::endl;
        if (!check_Item(d)) return false;
      }
      if (auto *d = dynamic_cast<ExpressionNode*>(ast[i].get())) {
        std::cout << "checking expression node" << std::endl;
        if (!check_expression(d)) return false;
      }
      std::cout << "finish checking the " << i + 1 << "th ASTNode" << std::endl;
    }
    return true;
  }
};
#endif