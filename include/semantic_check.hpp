#ifndef SEMANTIC_HPP
#define SEMANTIC_HPP
#include "parser.hpp"

template<typename T, typename U>
bool isSameDerived(const std::unique_ptr<T>& lhs, const std::unique_ptr<U>& rhs) {
  if (!lhs || !rhs) return false;
  return typeid(*lhs) == typeid(*rhs);
}

bool isOverflow(const std::string num) {
  const std::string MAX_INT_STR = "2147483647";

  if (num.size() > MAX_INT_STR.size()) return true;

  if (num.size() < MAX_INT_STR.size()) return false;

  return num > MAX_INT_STR;
}

std::string getFunctionParamTypeString(const FunctionParam* param) {
  return std::visit([](auto&& arg) -> std::string {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::unique_ptr<TypeNode>>) {
      return arg ? arg->toString() : "<null type>";
    } else if constexpr (std::is_same_v<T, std::unique_ptr<FunctionParamPattern>>) {
      return (arg && arg->type) ? arg->type->toString() : "<null type>";
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ellipsis>>) {
      return arg ? arg->ellip : "<ellipsis>";
    } else {
      return "<unknown>";
    }
  }, param->info);
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
  std::unordered_map<std::string, TypeNode*> function_types;
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
  bool if_cycle = false;//判断是不是在循环体内

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
    std::cout << "try to insert function in insertFunc: " << name << std::endl;
    if (func_table.count(name)) {
      if (name != "getInt") throw std::runtime_error("Duplicate function declaration: " + name);
    }
    func_table[name] = func;
  }

  FunctionSymbol* lookupFunc(const std::string& name) {
    if (func_table.count(name)) {
      return &func_table[name];
    }
    if (declared_struct_functions.count(name)) {
      return &declared_struct_functions[name];
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

  
  TypeNode* get_function_type(const std::string& name) {
    if (symbol_table.function_types.count(name)) {
      return symbol_table.function_types[name];
    } else if (parent) {
      return parent->get_function_type(name);
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

bool is_type_equal(const TypeNode* type1, const TypeNode* type2) {
  std::string s1 = type1->toString();
  std::string s2 = type2->toString();
  std::size_t pos = s1.rfind("::");
  if (pos != std::string::npos) {
    s1 = s1.substr(0, pos);
  }
  pos = s2.rfind("::");
  if (pos != std::string::npos) {
    s2 = s2.substr(0, pos);
  }
  if (auto* ref = dynamic_cast<const ReferenceTypeNode*>(type1)) {
    while (!s1.empty() && s1.front() == '&') s1.erase(s1.begin());
    if (ref->if_mut && s1.rfind("mut", 0) == 0) s1.erase(0, 3);
  }
  if (auto* ref = dynamic_cast<const ReferenceTypeNode*>(type2)) {
    while (!s2.empty() && s2.front() == '&') s2.erase(s2.begin());
    if (ref->if_mut && s2.rfind("mut", 0) == 0) s2.erase(0, 3);
  }
  return s1 == s2;
}

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
    currentScope->possible_self = currentScope->parent->possible_self;
    currentScope->if_cycle = currentScope->parent->if_cycle;
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
    size_t pos = type1.rfind("::");
    if (pos != std::string::npos) {
      type1 = type1.substr(0, pos);
    }
    pos = type2.rfind("::");
    if (pos != std::string::npos) {
      type2 = type2.substr(0, pos);
    }
    if (type1 == type2 || (type1 == "usize" || type1 == "u32") && type2 == "i32") {
      if (type1 != type2) {
        auto* lit = dynamic_cast<LiteralExpressionNode*>(expr->expression2.get());
        std::string literal = lit->toString();
        if (literal[0] != '-') {
          return true;
        } else {
          return false;
        }
      }
      return true;
    }
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
      const FunctionParam* fp = params->function_params[i].get();
      std::string paramName;
      TypeNode* typeNode = nullptr;
      bool if_mut = false;

      if (std::holds_alternative<std::unique_ptr<FunctionParamPattern>>(fp->info)) {
        auto& pattern = std::get<std::unique_ptr<FunctionParamPattern>>(fp->info);
        paramName = pattern->pattern->toString();
        typeNode = pattern->type.get();
        if (auto *ref = dynamic_cast<ReferenceTypeNode*>(typeNode)) {
          if_mut = ref->if_mut || if_mut;
        }
        auto if_mut_pattern = std::visit([](auto const& ptr) -> bool {
          if (!ptr) return true;

          using T = std::decay_t<decltype(ptr)>;
          if constexpr (std::is_same_v<T, std::unique_ptr<PatternWithoutRange>>) {
            return std::visit([](auto const& innerPtr) -> bool {
              if (!innerPtr) return true;
              using InnerT = std::decay_t<decltype(innerPtr)>;
              if constexpr (std::is_same_v<InnerT, std::unique_ptr<IdentifierPattern>>) {
                return innerPtr->if_mut;
              } else if constexpr (std::is_same_v<InnerT, std::unique_ptr<ReferencePattern>>) {
                std::cout << "getting reference pattern in letstatement" << std::endl;
                return innerPtr->if_mut;
              } else {
                return true;
              }
            }, ptr->pattern);
          } else {
            return true;
          }
        }, pattern->pattern->pattern);
        if_mut = if_mut || if_mut_pattern;
        std::cout << "declaring: " << paramName << " whose if_mut is " << if_mut << " and whose type is : " << typeNode->toString() << std::endl;
      } else if (std::holds_alternative<std::unique_ptr<TypeNode>>(fp->info)) {
        paramName = "_param" + std::to_string(i);
        typeNode = std::get<std::unique_ptr<TypeNode>>(fp->info).get();
        if (auto *ref = dynamic_cast<ReferenceTypeNode*>(typeNode)) {
          if_mut = ref->if_mut;
        }
        std::cout << "declaring: " << paramName << " whose if_mut is " << if_mut << std::endl;
      } else if (std::holds_alternative<std::unique_ptr<ellipsis>>(fp->info)) {
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
    } else if (auto* break_expr = dynamic_cast<BreakExpressionNode*>(expr)) {
      std::cout << "try to get return type in breakexpression" << std::endl;
      if (!break_expr->expr) return "";
      return getExpressionType(break_expr->expr.get())->toString();
    } else if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expr)) {
      std::cout << "try to get return type in ifexpression" << std::endl;
      if (!if_expr->block_expression) return "";
      enterScope();
      std::string type = get_return_type_without_changing_scope(if_expr->block_expression.get());
      exitScope();
      if (type != "") {
        std::cout << "getting return type in if_expression : " << type << std::endl;
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
    } else if (auto* expr_node = dynamic_cast<InfiniteLoopExpressionNode*>(expr)) {
      return get_return_type(expr_node->block_expression.get());;
    } else if (auto* expr_node = dynamic_cast<NegationExpressionNode*>(expr)) {
      return getExpressionType(expr_node->expression.get())->toString();
    } else if (auto* node_ptr = dynamic_cast<DereferenceExpressionNode*>(expr)) {
      if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
        std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
        auto* info = currentScope->lookupVar(path->toString());
        if (!info) {
          std::cout << "var: " << path->toString() << " not found" << std::endl;
          return "";
        } else {
          auto* type = info->type;
          if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
            return ref->type->toString();
          } else {
            std::cout << "not reference type after *" << std::endl;
            return "";
          }
        }
      } else {
        return "";
      }
    } else {
      return "";
    }
    return "";
  }

  std::string get_return_type_in_expression_in_let(ExpressionNode* expr) {
    std::cout << "get return type in expression" << std::endl;
    if (auto* return_expr = dynamic_cast<ReturnExpressionNode*>(expr)) {
      std::cout << "try to get return type in returnexpression" << std::endl;
      return "NeverType";
    } else if (auto* break_expr = dynamic_cast<BreakExpressionNode*>(expr)) {
      std::cout << "try to get return type in breakexpression" << std::endl;
      if (!break_expr->expr) return "";
      return getExpressionType(break_expr->expr.get())->toString();
    } else if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expr)) {
      std::cout << "try to get return type in ifexpression" << std::endl;
      if (!if_expr->block_expression) return "";
      enterScope();
      std::string type = get_return_type_without_changing_scope(if_expr->block_expression.get());
      exitScope();
      if (type != "") {
        std::cout << "getting return type in if_expression : " << type << std::endl;
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
    } else if (auto* expr_node = dynamic_cast<InfiniteLoopExpressionNode*>(expr)) {
      return get_return_type(expr_node->block_expression.get());;
    } else if (auto* expr_node = dynamic_cast<NegationExpressionNode*>(expr)) {
      return getExpressionType(expr_node->expression.get())->toString();
    } else if (auto* node_ptr = dynamic_cast<DereferenceExpressionNode*>(expr))  {
      if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
        std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
        auto* info = currentScope->lookupVar(path->toString());
        if (!info) {
          std::cout << "var: " << path->toString() << " not found" << std::endl;
          return "";
        } else {
          auto* type = info->type;
          if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
            return ref->type->toString();
          } else {
            std::cout << "not reference type after *" << std::endl;
            return "";
          }
        }
      } else {
        return "";
      }
    } else {
      return "";
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

  std::string get_return_type_in_statement_in_let(StatementNode* stat) {
    std::cout << "get return type in statement" << std::endl;
    if (stat->let_statement) {
      check_LetStatement(stat);
    }
    auto* expr_stat = stat->expr_statement.get();
    if (!expr_stat) {
      return "";
    } else {
      std::cout << "checking expressionStatement" << std::endl;
      return get_return_type_in_expression_in_let(expr_stat->expression.get());
    }
  } 

  std::unordered_set<std::string> get_return_type_in_if_in_let(IfExpressionNode* if_expr) {
    std::unordered_set<std::string> types;
    if (if_expr->block_expression) {
      enterScope();
      std::string type1 = get_return_type_without_changing_scope_in_let(if_expr->block_expression.get());
      exitScope();
      if (type1 == "usize") type1 = "i32";
      size_t pos = type1.rfind("::");
      if (pos != std::string::npos) {
        type1 = type1.substr(0, pos);
      }
      types.insert(type1);
      std::cout << "getting type in main block: " << type1 << std::endl;
    } 
    if (if_expr->else_block) {
      std::string type1 = get_return_type_without_changing_scope_in_let(if_expr->else_block.get());
      if (type1 == "usize") type1 = "i32";
      size_t pos = type1.rfind("::");
      if (pos != std::string::npos) {
        type1 = type1.substr(0, pos);
      }
      types.insert(type1);
      std::cout << "getting return type in else_block: " << type1 << std::endl;
    }
    if (if_expr->else_if) {
      if (auto* if_branch = dynamic_cast<IfExpressionNode*>(if_expr->else_if.get())) {
        auto branch_types = get_return_type_in_if_in_let(if_branch);
        for (auto it = branch_types.begin(); it != branch_types.end(); it++) {
          types.insert(*it);
        }
      }
    }
    for (auto it = types.begin(); it != types.end(); it++) {
      std::cout << "return type in if expression: " << *it << std::endl;
    } 
    if (types.size() != 1) {
      std::cout << "return type mismatch in if expression" << std::endl;
    }
    return types;
  }

  std::unordered_set<std::string> get_return_type_in_if(IfExpressionNode* if_expr) {
    std::unordered_set<std::string> types;
    if (if_expr->block_expression) {
      enterScope();
      std::string type1 = get_return_type_without_changing_scope(if_expr->block_expression.get());
      exitScope();
      if (type1 == "usize") type1 = "i32";
      size_t pos = type1.rfind("::");
      if (pos != std::string::npos) {
        type1 = type1.substr(0, pos);
      }
      types.insert(type1);
      std::cout << "getting type in main block: " << type1 << std::endl;
    } 
    if (if_expr->else_block) {
      std::string type1 = get_return_type_without_changing_scope(if_expr->else_block.get());
      if (type1 == "usize") type1 = "i32";
      size_t pos = type1.rfind("::");
      if (pos != std::string::npos) {
        type1 = type1.substr(0, pos);
      }
      types.insert(type1);
      std::cout << "getting return type in else_block: " << type1 << std::endl;
    }
    if (if_expr->else_if) {
      if (auto* if_branch = dynamic_cast<IfExpressionNode*>(if_expr->else_if.get())) {
        auto branch_types = get_return_type_in_if(if_branch);
        for (auto it = branch_types.begin(); it != branch_types.end(); it++) {
          types.insert(*it);
        }
      }
    }
    for (auto it = types.begin(); it != types.end(); it++) {
      std::cout << "return type in if expression: " << *it << std::endl;
    } 
    if (types.size() != 1) {
      std::cout << "return type mismatch in if expression" << std::endl;
    }
    return types;
  }

  bool check_return_type(BlockExpressionNode* block) {
    std::cout << "enter scope in checking return type in block expression" << std::endl;
    enterScope();
    std::unordered_set<std::string> types;
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode" << std::endl;
      check_Statment(block->statement[i].get());
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        types.insert(temp);
      }
      if (block->statement[i]->expr_statement) {
        if (auto* if_expr = dynamic_cast<IfExpressionNode*>(block->statement[i]->expr_statement->expression.get())) {
          if (get_return_type_in_if(if_expr).size() != 1){
            return false;
          }
        }
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
            std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
            auto* info = currentScope->lookupVar(path->toString());
            if (!info) {
              std::cout << "var: " << path->toString() << " not found" << std::endl;
              return "";
            } else {
              auto* type = info->type;
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
                return ref->type->toString();
              } else {
                std::cout << "not reference type after *" << std::endl;
                return "";
              }
            }
          } else {
            return "";
          }
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

  std::unordered_set<std::string> delete_empty_string(std::unordered_set<std::string> s) {
    for (auto it = s.begin(); it != s.end(); ) {
      if (it->empty()) {
        it = s.erase(it);
      } else {
        ++it;
      }
    }
    return s;
  }

  bool check_return_type_without_changing_scope(BlockExpressionNode* block) {
    bool has_minus = false;
    std::cout << "enter scope in checking return type in block expression" << std::endl;
    std::unordered_set<std::string> types;
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode in func: check_return_type_without_changing_scope" << std::endl;
      check_Statment(block->statement[i].get());
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        std::cout << "origin type : " << temp << std::endl;
        std::string actual_temp = temp;
        size_t pos = temp.rfind("::");
        if (pos != std::string::npos) {
          actual_temp = temp.substr(0, pos);
        }
        std::cout << "getting return type in statement : " << actual_temp << std::endl;
        types.insert(actual_temp);
      }
      if (temp == "i32") {
        if (block->statement[i]->expr_statement) {
          if (auto* neg = dynamic_cast<NegationExpressionNode*>(block->statement[i]->expr_statement->expression.get())) {
            has_minus = true;
          }
        }
      }
      if (block->statement[i]->expr_statement) {
        if (auto* if_expr = dynamic_cast<IfExpressionNode*>(block->statement[i]->expr_statement->expression.get())) {
          auto types = get_return_type_in_if(if_expr);
          if (i != block->statement.size() - 1) {
            types = delete_empty_string(types);
          }
          if (types.size() > 1) {
            if (!has_minus && types.size() == 2) {
              if (!(types.count("i32") && (types.count("usize") || types.count("u32")))) {
                return false;
              }
            } else {
              return false;
            }
          }
        }
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
          return "bool";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
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
            std::cout << "getting typepathnode in logical expression : " << path_type->toString() << std::endl;
            std::string name = path_type->toString();
            if (is_legal_type(name)) return name;
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
          std::cout << "getting field expression in ewb" << std::endl;
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
          if (!node_ptr->expression) {
            std::cout << "[GroupedExpressionError]: missing expression" << std::endl;
            return "";
          }
          return getExpressionType(node_ptr->expression.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
            std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
            auto* info = currentScope->lookupVar(path->toString());
            if (!info) {
              std::cout << "var: " << path->toString() << " not found" << std::endl;
              return "";
            } else {
              auto* type = info->type;
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
                return ref->type->toString();
              } else {
                std::cout << "not reference type after *" << std::endl;
                return "";
              }
            }
          } else {
            return "";
          }
        } else {
          std::cout << "unknown type of expression without block" << std::endl;
          return "";
        }
      }, expr_node->expr);
      if (s == "") std::cout << "failed to get type in expressionwithoutblock" << std::endl;
      if (s != "") {
        if (s == "i32") {
          if (std::holds_alternative<std::unique_ptr<NegationExpressionNode>>(expr_node->expr)) {
            has_minus = true;
          }
        }
        std::cout << "origin type: " << s << std::endl;
        std::string actual_temp = s;
        size_t pos = s.rfind("::");
        if (pos != std::string::npos) {
          actual_temp = s.substr(0, pos);
        }
        types.insert(actual_temp);
        std::cout << "type in expression without block: " << actual_temp << std::endl;
      }
    }
    if (types.size() != 1) {
      for (auto it = types.begin(); it != types.end(); it++) {
        std::cout << "return type in block expression : " << *it << std::endl;
      }
      if (types.size() == 0) {
        std::cout << "failed getting any type in block expression" << std::endl;
      }
      if (!has_minus && types.size() == 2) {
        if (!(types.count("i32") && (types.count("usize") || types.count("u32")))) {
          return false;
        } else {
          if(has_minus) return false;
        }
      } else {
        return false;
      }
    } else {
      std::cout << "type in blockexpression : " << *types.begin() << std::endl;
    }
    return true;
  }

  //搞一个set获得所有可能return的type，然后如果有usize/u32优先返回usize/u32
  std::unordered_set<std::string> get_return_type_set_without_changing_scope(BlockExpressionNode* block) {
    bool has_minus = false;
    std::cout << "enter scope in checking return type in block expression" << std::endl;
    std::unordered_set<std::string> types;
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode" << std::endl;
      check_Statment(block->statement[i].get());
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        std::cout << "origin type : " << temp << std::endl;
        std::string actual_temp = temp;
        size_t pos = temp.rfind("::");
        if (pos != std::string::npos) {
          actual_temp = temp.substr(0, pos);
        }
        std::cout << "getting return type in statement : " << actual_temp << std::endl;
        types.insert(actual_temp);
      }
      if (temp == "i32") {
        if (block->statement[i]->expr_statement) {
          if (auto* neg = dynamic_cast<NegationExpressionNode*>(block->statement[i]->expr_statement->expression.get())) {
            has_minus = true;
          }
        }
      }
      if (block->statement[i]->expr_statement) {
        if (auto* if_expr = dynamic_cast<IfExpressionNode*>(block->statement[i]->expr_statement->expression.get())) {
          auto types = get_return_type_in_if(if_expr);
          if (i != block->statement.size() - 1) {
            types = delete_empty_string(types);
          }
        }
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
          return "bool";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
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
            std::cout << "getting typepathnode in logical expression : " << path_type->toString() << std::endl;
            std::string name = path_type->toString();
            if (is_legal_type(name)) return name;
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
          if (!node_ptr->expression) {
            std::cout << "[GroupedExpressionError]: missing expression" << std::endl;
            return "";
          }
          return getExpressionType(node_ptr->expression.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
            std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
            auto* info = currentScope->lookupVar(path->toString());
            if (!info) {
              std::cout << "var: " << path->toString() << " not found" << std::endl;
              return "";
            } else {
              auto* type = info->type;
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
                return ref->type->toString();
              } else {
                std::cout << "not reference type after *" << std::endl;
                return "";
              }
            }
          } else {
            return "";
          }
        } else {
          std::cout << "unknown type of expression without block" << std::endl;
          return "";
        }
      }, expr_node->expr);
      if (s == "") std::cout << "failed to get type in expressionwithoutblock" << std::endl;
      if (s != "") {
        if (s == "i32") {
          if (std::holds_alternative<std::unique_ptr<NegationExpressionNode>>(expr_node->expr)) {
            has_minus = true;
          }
        }
        std::cout << "origin type: " << s << std::endl;
        std::string actual_temp = s;
        size_t pos = s.rfind("::");
        if (pos != std::string::npos) {
          actual_temp = s.substr(0, pos);
        }
        types.insert(actual_temp);
        std::cout << "type in expression without block: " << actual_temp << std::endl;
      }
    }
    return types;
  }

  std::string get_return_type(BlockExpressionNode* block) {
    std::cout << "enter scope in getting return type in block expression" << std::endl;
    auto temp_types = get_return_type_set_without_changing_scope(block);
    if (temp_types.size() == 2) {
      if ((temp_types.count("usize") || temp_types.count("u32")) && temp_types.count("i32")) {
        if (temp_types.count("usize")) return "usize";
        else return "u32";
      }
    }
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
      auto res = std::visit([this](auto& node_ptr) -> std::string {
        using T = std::decay_t<decltype(node_ptr)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          if (node_ptr && node_ptr->expression) return getExpressionType(node_ptr->expression.get())->toString();
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
          std::cout << "return type: got in struct expression" << std::endl;
          return node_ptr->pathin_expression->toString();
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
            std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
            auto* info = currentScope->lookupVar(path->toString());
            if (!info) {
              std::cout << "var: " << path->toString() << " not found" << std::endl;
              return "";
            } else {
              auto* type = info->type;
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
                return ref->type->toString();
              } else {
                std::cout << "not reference type after *" << std::endl;
                return "";
              }
            }
          } else {
            return "";
          }
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type->toString();
        } else {
          std::cout << "failed getting return type in get_return_type" << std::endl;
          return "";
        }
      }, expr_node->expr);
      exitScope();
      return res;
    }
    exitScope();
    return "";
  }

  std::string get_return_type_without_changing_scope_in_let(BlockExpressionNode* block) {
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode" << std::endl;
      std::string temp = get_return_type_in_statement_in_let(block->statement[i].get());
      if (temp != "") {
        std::cout << "getting type in statements of blockexpression: " << temp << std::endl;
        return temp;
      }
    }
    if (auto* expr_node = dynamic_cast<ExpressionWithoutBlockNode*>(block->expression_without_block.get())) {
      std::cout << "having ewb in block Expression" << std::endl;
      return std::visit([this](auto& node_ptr) -> std::string {
        using T = std::decay_t<decltype(node_ptr)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          return "NeverType";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          std::cout << "getting type of negation expression in function get_return_type_without_changing_scope_in_let" << std::endl;
          return getExpressionType(node_ptr->expression.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
          std::cout << "return type: got in struct expression" << std::endl;
          return node_ptr->pathin_expression->toString();
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
        } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
            std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
            auto* info = currentScope->lookupVar(path->toString());
            if (!info) {
              std::cout << "var: " << path->toString() << " not found" << std::endl;
              return "";
            } else {
              auto* type = info->type;
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
                return ref->type->toString();
              } else {
                std::cout << "not reference type after *" << std::endl;
                return "";
              }
            }
          } else {
            return "";
          }
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
            if (is_legal_type(name)) return name;
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          std::cout << "getting type of path expression in function get_return_type_without_changing_scope" << std::endl;
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
          return getExpressionType(node_ptr->expr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
          return currentScope->get_function_type(dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())->toString())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
          if (dynamic_cast<PathExpressionNode*>(node_ptr->base.get())) {
            std::string base = dynamic_cast<PathExpressionNode*>(node_ptr->base.get())->toString();
            std::cout << "base of index expression : " << base << std::endl;
            if (!currentScope->lookupVar(base)) {
              std::cout << "var: " << base << " not found" << std::endl;
              return "";
            }
            if (!dynamic_cast<ArrayTypeNode*>(currentScope->lookupVar(base)->type)) {
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(currentScope->lookupVar(base)->type)) {
                if (auto* inner_array = dynamic_cast<ArrayTypeNode*>(ref->type.get())) {
                  return inner_array->type->toString();
                }
              }
              std::cout << "wrong type in index expression" << std::endl;
              return "";
            }
            return (dynamic_cast<ArrayTypeNode*>(currentScope->lookupVar(base)->type))->type->toString();
          } else if (auto* field_expr = dynamic_cast<FieldExpressionNode*>(node_ptr->base.get())) {
            if (auto* base_path = dynamic_cast<PathExpressionNode*>(field_expr->expression.get())) {
              std::cout << "base path in field expression in index expression: " << base_path->toString() << std::endl;
              auto* struct_type = getExpressionType(base_path);
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(struct_type)) {
                struct_type = ref->type.get();
              }
              std::string struct_name = struct_type->toString();
              auto* structInfo = currentScope->lookupStruct(struct_name);
              std::string item_name = field_expr->identifier.id;
              std::cout << "finding " << item_name << " in struct: " << structInfo->name << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "item in struct " << structInfo->name << " : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  auto* array_type = dynamic_cast<ArrayTypeNode*>(t);
                  return array_type->type->toString();
                }
              }
            }
            std::cout << "error in getting type in indexexpression with field expression" << std::endl;
            return "";
          } 
          return "";
        } else {
          std::cout << "failed getting return type in get_return_type_without_changing_scope" << std::endl;
          return "";
        }
      }, expr_node->expr);
    }
    return "";
  }

  std::string get_return_type_without_changing_scope(BlockExpressionNode* block) {
    std::cout << "getting return type in blockexpressionnode" << std::endl;
    auto temp_types = get_return_type_set_without_changing_scope(block);
    if (temp_types.size() == 2) {
      if ((temp_types.count("usize") || temp_types.count("u32")) && temp_types.count("i32")) {
        if (temp_types.count("usize")) return "usize";
        else return "u32";
      }
    }
    for (int i = 0; i < block->statement.size(); i++) {
      std::cout << "checking the " << i << "th statment in blockExpressionNode in func: get_return_type_without_changing_scope" << std::endl;
      std::string temp = get_return_type_in_statement(block->statement[i].get());
      if (temp != "") {
        std::cout << "getting type in statements of blockexpression: " << temp << std::endl;
        return temp;
      }
    }
    if (auto* expr_node = dynamic_cast<ExpressionWithoutBlockNode*>(block->expression_without_block.get())) {
      std::cout << "having ewb in block Expression" << std::endl;
      return std::visit([this](auto& node_ptr) -> std::string {
        using T = std::decay_t<decltype(node_ptr)>;
        if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          if (node_ptr && node_ptr->expression) return getExpressionType(node_ptr->expression.get())->toString();
          return "";
        } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())) {
            std::cout << "path in dereferenceexpression: " << path->toString() << std::endl;
            auto* info = currentScope->lookupVar(path->toString());
            if (!info) {
              std::cout << "var: " << path->toString() << " not found" << std::endl;
              return "";
            } else {
              auto* type = info->type;
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
                return ref->type->toString();
              } else {
                std::cout << "not reference type after *" << std::endl;
                return "";
              }
            }
          } else {
            return "";
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
          std::cout << "return type: got in struct expression" << std::endl;
          return node_ptr->pathin_expression->toString();
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
            if (is_legal_type(name)) return name;
            auto* info = currentScope->lookupVar(name);
            return info->type->toString();
          } else {
            return path_type->toString();
          }
        } else if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          std::cout << "getting type of path expression in function get_return_type_without_changing_scope" << std::endl;
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
          return getExpressionType(node_ptr->expr.get())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
          return currentScope->get_function_type(dynamic_cast<PathExpressionNode*>(node_ptr->expression.get())->toString())->toString();
        } else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
          return getExpressionType(node_ptr.get())->toString();
          if (dynamic_cast<PathExpressionNode*>(node_ptr->base.get())) {
            std::string base = dynamic_cast<PathExpressionNode*>(node_ptr->base.get())->toString();
            std::cout << "base of index expression : " << base << std::endl;
            if (!currentScope->lookupVar(base)) {
              std::cout << "var: " << base << " not found" << std::endl;
              return "";
            }
            if (!dynamic_cast<ArrayTypeNode*>(currentScope->lookupVar(base)->type)) {
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(currentScope->lookupVar(base)->type)) {
                if (auto* inner_array = dynamic_cast<ArrayTypeNode*>(ref->type.get())) {
                  return inner_array->type->toString();
                }
              }
              std::cout << "wrong type in index expression" << std::endl;
              return "";
            }
            return (dynamic_cast<ArrayTypeNode*>(currentScope->lookupVar(base)->type))->type->toString();
          } else if (auto* field_expr = dynamic_cast<FieldExpressionNode*>(node_ptr->base.get())) {
            if (auto* base_path = dynamic_cast<PathExpressionNode*>(field_expr->expression.get())) {
              std::cout << "base path in field expression in index expression: " << base_path->toString() << std::endl;
              auto* struct_type = getExpressionType(base_path);
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(struct_type)) {
                struct_type = ref->type.get();
              }
              std::string struct_name = struct_type->toString();
              auto* structInfo = currentScope->lookupStruct(struct_name);
              std::string item_name = field_expr->identifier.id;
              std::cout << "finding " << item_name << " in struct: " << structInfo->name << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "item in struct " << structInfo->name << " : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  auto* array_type = dynamic_cast<ArrayTypeNode*>(t);
                  return array_type->type->toString();
                }
              }
            }
            std::cout << "error in getting type in indexexpression with field expression" << std::endl;
            return "";
          }
          return "";
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

  bool check_array_length_const(ArrayTypeNode* array) {//检查arrayType的长度是pathExpression的时候是不是const类型
    if (auto* len = dynamic_cast<PathExpressionNode*>(array->expression.get())) {
      std::string length = len->toString();
      std::cout << "length of array : " << length << std::endl;
      if (!currentScope->lookupConst(length)) return false;
      if (auto* subArray = dynamic_cast<ArrayTypeNode*>(array->type.get())) {
        if (!check_array_length_const(subArray)) return false;
      }
    }
    return true;
  }

  bool has_exit_in_block(const BlockExpressionNode* block) {
    for (int i = 0; i < block->statement.size(); i++) {
      if (block->statement[i]->expr_statement != nullptr) {
        if (auto* call = dynamic_cast<CallExpressionNode*>(block->statement[i]->expr_statement->expression.get())) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
            if (path->toString() == "exit") return true;
          }
        }
      }
    }
    if (!block->expression_without_block) return false;
    if (std::holds_alternative<std::unique_ptr<CallExpressionNode>>(block->expression_without_block->expr)) {
      auto& call = std::get<std::unique_ptr<CallExpressionNode>>(block->expression_without_block->expr);
      if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
        if (path->toString() == "exit") return true;
      }
    }
    return false;
  }

  bool has_sth_after_exit(const BlockExpressionNode* block) {
    for (int i = 0; i < block->statement.size(); i++) {
      if (block->statement[i]->expr_statement != nullptr) {
        if (auto* call = dynamic_cast<CallExpressionNode*>(block->statement[i]->expr_statement->expression.get())) {
          if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
            if (path->toString() == "exit") {
              std::cout << "id of exit : " << i << "  " << "size of statement:" << block->statement.size() << std::endl;
              if (i == block->statement.size() - 1 && !block->expression_without_block) return false;
              else return true;
            }
          }
        }
      }
    }
    if (!block->expression_without_block) return false;
    if (std::holds_alternative<std::unique_ptr<CallExpressionNode>>(block->expression_without_block->expr)) {
      auto& call = std::get<std::unique_ptr<CallExpressionNode>>(block->expression_without_block->expr);
      if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
        if (path->toString() == "exit") return false;
      }
    }
    return false;
  }

  bool has_else_in_if(const IfExpressionNode* if_expr) {
    if (if_expr->else_block) return true;
    if (if_expr->else_if) {
      auto* else_if_expr = dynamic_cast<const IfExpressionNode*>(if_expr->else_if.get());
      return has_else_in_if(else_if_expr);
    }
    return false;
  }

  //在impl中，function可能会有selfParam，需要记录self指向的是什么类型，同时需要记录这个类型的函数和成员来检查methodcall是否合法
  //function本身只需要记录可能存在的self是指向什么类，在Scope中存储这个类的相关信息
  bool check_Item(const ItemNode* expr) {
    //===Function===
    if (auto* function = dynamic_cast<const FunctionNode*>(expr)) {
      //如果是main函数，先要检查有没有exit函数，并且要检查返回值要么没有要么是->()
      if (function->identifier == "main") {
        std::cout << "checking main_func" << std::endl;
        if (!function->block_expression) return false;
        if (!has_exit_in_block(function->block_expression.get())) {
          std::cout << "missing exit in main function" << std::endl;
          return false;
        }
        if (has_sth_after_exit(function->block_expression.get())) {
          std::cerr << "nothing allowed after exit in main function" << std::endl;
          return false;
        }
        if (function->return_type) {
          std::cout << "checking if return_type is ->() in main function" << std::endl;
          if (function->return_type->type->toString() == "()") {
            return true;
          } else {
            std::cout << "invalid return type in main function: " << function->return_type->type->toString() << std::endl;
            return false;
          }
        }
      } else {
        if (function->block_expression) {
          if (has_exit_in_block(function->block_expression.get())) {
            std::cerr << "function exit cannot be used in non_main function" << std::endl;
            return false;
          }
        }
      }
      
      FunctionSymbol func_info;
      func_info.name = function->identifier;
      func_info.param_types = function->function_parameter.get();

      if (function->return_type) func_info.return_type = function->return_type->type.get();

      currentScope->insertFunc(function->identifier, func_info);

      currentScope->symbol_table.functions[function->identifier] = function->function_parameter.get();
      currentScope->symbol_table.function_types[function->identifier] = func_info.return_type;

      if (function->block_expression) {
        std::cout << "checking blockexpression of function in scope : " << currentScope->id << std::endl;
        enterScope();
        declareFunctionParameters(function->function_parameter.get(), currentScope, function->impl_type_name);
        if (function->return_type) {
          if (auto* array = dynamic_cast<ArrayTypeNode*>(function->return_type->type.get())) {
            if (!check_array_length_const(array)) {
              std::cout << "length of array is dynamic" << std::endl;
              return false;
            }
          }
        }
        bool ans = check_BlockExpression_without_changing_scope(dynamic_cast<BlockExpressionNode*>(function->block_expression.get()));
        exitScope();
        if (!ans) {
          std::cout << "error in block expression of function : " << function->identifier << std::endl;
          return ans;
        }
      }
      if (function->return_type) {
        if (auto* paren = dynamic_cast<ParenthesizedTypeNode*>(function->return_type->type.get())) {
          if (paren->type == nullptr) return true;
        }
        if (auto* tuple = dynamic_cast<TupleTypeNode*>(function->return_type->type.get())) {
          if (tuple->types.size() == 0) return true;
        }
      }
      if (function->return_type && function->block_expression) {
        std::cout << "checking if return type mismatch in function : " << function->identifier << std::endl;
        if (!function->block_expression->expression_without_block) {
          if (function->block_expression->statement.size() == 0) {
            return false;
          } else {
            if (function->block_expression->statement[function->block_expression->statement.size() - 1]->expr_statement) {
              if (auto* if_expr = dynamic_cast<IfExpressionNode*>(function->block_expression->statement[function->block_expression->statement.size() - 1]->expr_statement->expression.get())) {
                if (!has_else_in_if(if_expr)) {
                  std::cout << "missing else in if" << std::endl;
                  return false;
                }
              }
            }
          }
        }
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
          exitScope();
          enterScope();
          declareFunctionParameters(function->function_parameter.get(), currentScope, function->impl_type_name);
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
          exitScope();
        }
      }
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
      std::cout << "inserting trait : " << Trait->identifier << " in scope : " << currentScope->id << std::endl;
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
      ConstantInfo info{Const->identifier.value(), Const->type.get(), Const->expression.get()};
      std::cout << "declaring constant : " << Const->identifier.value() << std::endl;
      currentScope->const_table[Const->identifier.value()] = info;
      Symbol symbol{Const->identifier.value(), Const->type.get(), false, false, true};
      currentScope->insertVar(Const->identifier.value(), symbol);
      if (Const->type->toString() != getExpressionType(Const->expression.get())->toString()) {
        if ((Const->type->toString() == "usize" || Const->type->toString() == "u32") && getExpressionType(Const->expression.get())->toString() == "i32") {
          auto* lit = dynamic_cast<LiteralExpressionNode*>(Const->expression.get());
          if (lit->toString()[0] != '-') {
            return true;
          }
        }
        std::cout << "type mismatch in constant assignment : " << std::endl;
        std::cout << "declared type : " << Const->type->toString() << std::endl;
        std::cout << "actual type : " << getExpressionType(Const->expression.get())->toString() << std::endl;
        return false;
      }
    }

    //===InherentImplementation===
    if (auto* Impl = dynamic_cast<const InherentImplNode*>(expr)) {
      std::string type = Impl->type->toString();
      std::cout << "type of InherentImplNode : " << type << std::endl;
      if (currentScope->declared_struct.find(type) != currentScope->declared_struct.end()) {//是已经declared过的struct
        currentScope->possible_self = type;
        std::cout << "setting current possible self : " << type << std::endl;
        for (int i = 0; i < Impl->associated_item.size(); i++) {
          if (auto* funcNode = std::get_if<std::unique_ptr<FunctionNode>>(&Impl->associated_item[i]->associated_item)) {
            if (funcNode->get()->block_expression && has_exit_in_block(funcNode->get()->block_expression.get())) {
              std::cerr << "exit not allowed in methods" << std::endl;
              return false;
            }
          }
        }
        enterScope();
        for (int i = 0; i < Impl->associated_item.size(); i++) {
          std::cout << "checking the " << i << "th item in impl node" << std::endl;
          if (auto* funcNode = std::get_if<std::unique_ptr<FunctionNode>>(&Impl->associated_item[i]->associated_item)) {
            FunctionNode* function = funcNode->get();

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
            if (function->return_type) fs.return_type = function->return_type->type.get();
            fs.param_types = function->function_parameter.get();
            currentScope->parent->declared_struct_functions[func_to_declare] = fs;
            currentScope->parent->insertFunc(func_to_declare, fs);
            std::cout << "declaring function: " << func_to_declare << " in scope: " << currentScope->parent->id << std::endl; 
            std::cout << "size of function_table in scope function added to: " << currentScope->parent->declared_struct_functions.size() << std::endl;            

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
              if (declared_return_type == "Self" || declared_return_type == "self") {
                declared_return_type = Impl->type->toString();
              }
              std::cout << "declared return type: " << declared_return_type << std::endl;
              std::cout << "actual return type: " << actual_return_type << std::endl;
              if (declared_return_type != "Self" && declared_return_type != "self" && actual_return_type != declared_return_type) {
                std::cout << "[Return Type Error]: function " << function->identifier << std::endl;
                exitScope();
                return false;
              }
              exitScope();
            }
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
    std::cout << "number of statements : " << block_expr->statement.size() << std::endl;
    for (int i = 0; i < block_expr->statement.size(); i++) {
      std::cout << "statement id in block expression : " << i << std::endl;
      if (!check_Statment(dynamic_cast<StatementNode*>(block_expr->statement[i].get()))) {
        exitScope();
        return false;
      }
    }
    if (block_expr->expression_without_block) {
      if (!check_expression(block_expr->expression_without_block.get())) {
        std::cout << "error in expression without block in block expression" << std::endl;
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
    if (block_expr->expression_without_block) {
      if (!check_expression(block_expr->expression_without_block.get())) {
        std::cout << "error in expression without block in block expression" << std::endl;
        return false;
      }
    }
    std::cout << "finish function check_BlockExpression_without_changing_scope" << std::endl;
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

  TypeNode* get_type_in_loop(const InfiniteLoopExpressionNode* loop) {
    auto* block = loop->block_expression.get();
    if (block->expression_without_block) {
      return getExpressionType(block->expression_without_block.get());
    } else {
      for (int i = 0; i < block->statement.size(); i++) {
        auto* stat = block->statement[i].get();
        if (auto* expr = stat->expr_statement.get()) {
          auto* expression = expr->expression.get();
          if (auto* ret = dynamic_cast<ReturnExpressionNode*>(expression)) {
            return getExpressionType(ret->expression.get());
          } else if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expression)) {
            std::cout << "getting type of if expression in loop expression" << std::endl;
            if (getExpressionType(if_expr)) return getExpressionType(if_expr);
          } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expression)) {
            std::cout << "getting type of break expression in loop expression" << std::endl;
            return getExpressionType(bre->expr.get());
          }
        }
      }
    }
    return nullptr;
  }

  TypeNode* get_type_in_if(const IfExpressionNode* origin_if_expr) {
    std::cout << "func: get_type_in_if" << std::endl;
    auto* block = origin_if_expr->block_expression.get();
    enterScope();
    for (int i = 0; i < block->statement.size(); i++) {
      auto* stat = block->statement[i].get();
      if (auto* letStat = stat->let_statement.get()) {
        try {
          std::cout << "try to declare var : " << letStat->pattern->toString()  << " in scope :" << currentScope->id << "whose if_mut is " << letStat->get_if_mutable() << std::endl;
          declareVariable(letStat->pattern->toString(), letStat->type.get(), letStat->get_if_mutable());
        } catch (const std::exception& e) {
          std::cerr << "[Declare Error in LetStatement] : " << e.what() << std::endl;
        }
      }
      if (auto* expr = stat->expr_statement.get()) {
        auto* expression = expr->expression.get();
        if (auto* ret = dynamic_cast<ReturnExpressionNode*>(expression)) {
          auto* res = getExpressionType(ret->expression.get());
          exitScope();
          return res;
        } else if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expression)) {
          std::cout << "getting type of if expression in if expression" << std::endl;
          if (getExpressionType(if_expr)) {
            auto* res = getExpressionType(if_expr);
            exitScope();
            return res;
          }
        } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expression)) {
          std::cout << "getting type of break expression in if expression" << std::endl;
          auto* res = getExpressionType(bre->expr.get());
          exitScope();
          return res;
        }
      }
    }
    if (block->expression_without_block) {
      std::cout << "having ewb in if" << std::endl;
      auto* res = getExpressionType(block->expression_without_block.get());
      exitScope();
      return res;
    }
    return nullptr;
  }

  TypeNode* get_type_in_if_in_let(const IfExpressionNode* origin_if_expr) {
    std::cout << "func: get_type_in_if_in_let" << std::endl;
    auto* block = origin_if_expr->block_expression.get();
    enterScope();
    for (int i = 0; i < block->statement.size(); i++) {
      auto* stat = block->statement[i].get();
      if (auto* letStat = stat->let_statement.get()) {
        try {
          std::cout << "try to declare var : " << letStat->pattern->toString()  << " in scope :" << currentScope->id << "whose if_mut is " << letStat->get_if_mutable() << std::endl;
          declareVariable(letStat->pattern->toString(), letStat->type.get(), letStat->get_if_mutable());
        } catch (const std::exception& e) {
          std::cerr << "[Declare Error in LetStatement] : " << e.what() << std::endl;
        }
      }
      if (auto* expr = stat->expr_statement.get()) {
        auto* expression = expr->expression.get();
        if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expression)) {
          std::cout << "getting type of if expression in if expression" << std::endl;
          if (getExpressionTypeInLet(if_expr)) {
            auto* res = getExpressionTypeInLet(if_expr);
            exitScope();
            return res;
          }
        } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expression)) {
          std::cout << "getting type of break expression in if expression" << std::endl;
          auto* res = getExpressionTypeInLet(bre->expr.get());
          exitScope();
          return res;
        }
      }
    }
    if (block->expression_without_block) {
      std::cout << "having ewb in if" << std::endl;
      auto* res = getExpressionTypeInLet(block->expression_without_block.get());
      exitScope();
      return res;
    }
    if (origin_if_expr->else_if) {
      return get_type_in_if_in_let(dynamic_cast<const IfExpressionNode*>(origin_if_expr->else_if.get()));
    }
    if (origin_if_expr->else_block) {
      std::cout << "getting type in else block" << std::endl;
      block = origin_if_expr->else_block.get();
      enterScope();
      for (int i = 0; i < block->statement.size(); i++) {
        auto* stat = block->statement[i].get();
        if (auto* letStat = stat->let_statement.get()) {
          try {
            std::cout << "try to declare var : " << letStat->pattern->toString()  << " in scope :" << currentScope->id << "whose if_mut is " << letStat->get_if_mutable() << std::endl;
            declareVariable(letStat->pattern->toString(), letStat->type.get(), letStat->get_if_mutable());
          } catch (const std::exception& e) {
            std::cerr << "[Declare Error in LetStatement] : " << e.what() << std::endl;
          }
        }
        if (auto* expr = stat->expr_statement.get()) {
          auto* expression = expr->expression.get();
          if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expression)) {
            std::cout << "getting type of if expression in if expression" << std::endl;
            if (getExpressionTypeInLet(if_expr)) {
              auto* res = getExpressionTypeInLet(if_expr);
              exitScope();
              return res;
            }
          } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expression)) {
            std::cout << "getting type of break expression in if expression" << std::endl;
            auto* res = getExpressionTypeInLet(bre->expr.get());
            exitScope();
            return res;
          }
        }
      }
      if (block->expression_without_block) {
        std::cout << "having ewb in if" << std::endl;
        auto* res = getExpressionTypeInLet(block->expression_without_block.get());
        exitScope();
        return res;
      }
      exitScope();
    }
    
    return nullptr;
  }

  TypeNode* getExpressionType(ExpressionNode* expr) {
    if (auto* block = dynamic_cast<BlockExpressionNode*>(expr)) {
      enterScope();
      for (int i = 0; i < block->statement.size(); i++) {
        if (block->statement[i]->let_statement) {
          try {
            std::cout << "try to declare var : " << block->statement[i]->let_statement->pattern->toString()  << " in scope :" << currentScope->id << "whose if_mut is " << block->statement[i]->let_statement->get_if_mutable() << std::endl;
            declareVariable(block->statement[i]->let_statement->pattern->toString(), block->statement[i]->let_statement->type.get(), block->statement[i]->let_statement->get_if_mutable());
          } catch (const std::exception& e) {
            std::cerr << "[Declare Error in LetStatement] : " << e.what() << std::endl;
          }
        }
        auto* stat = block->statement[i].get();
        if (auto* expr = stat->expr_statement.get()) {
          auto* expression = expr->expression.get();
          if (auto* ret = dynamic_cast<ReturnExpressionNode*>(expression)) {
            auto* res = getExpressionType(ret->expression.get());
            exitScope();
            return res;
          } else if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expression)) {
            std::cout << "getting type of if expression in block expression" << std::endl;
            if (getExpressionType(if_expr)) {
              auto* res = getExpressionType(if_expr);
              exitScope();
              return res;
            }
          } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expression)) {
            std::cout << "getting type of break expression in block expression" << std::endl;
            auto* res = getExpressionType(bre->expr.get());
            exitScope();
            return res;
          }
        }
      }
      if (block->expression_without_block) {
        std::cout << "having ewb in block" << std::endl;
        auto* res = getExpressionType(block->expression_without_block.get());
        exitScope();
        return res;
      }
      exitScope();
    }
    if (auto* indexExpr = dynamic_cast<IndexExpressionNode*>(expr)) {
      TypeNode* arrayType = getExpressionType(indexExpr->base.get());
      if (auto* arr = dynamic_cast<ArrayTypeNode*>(arrayType)) {
        return arr->type.get();
      }
    }
    if (auto* returnExpr = dynamic_cast<ReturnExpressionNode*>(expr)) {
      return getExpressionType(returnExpr->expression.get());
    }
    if (auto* DerefExpr = dynamic_cast<DereferenceExpressionNode*>(expr)) {
      std::cout << "getting type of dereference expression" << std::endl;
      if (auto* ref = dynamic_cast<ReferenceTypeNode*>(getExpressionType(DerefExpr->expression.get()))) {
        return ref->type.get();
      } 
    }
    if (auto* loop = dynamic_cast<InfiniteLoopExpressionNode*>(expr)) {
      std::cout << "getting type of loop expression" << std::endl;
      return get_type_in_loop(loop);
    }
    if (auto* field_expr = dynamic_cast<FieldExpressionNode*>(expr)) {
      std::cout << "getting type of field expression" << std::endl;
      if (auto* path_expr = dynamic_cast<PathExpressionNode*>(field_expr->expression.get())) {
        std::cout << "path_expr: " << path_expr->toString() << std::endl;
        if (path_expr->toString() == "self" || path_expr->toString() == "Self" 
            || getExpressionType(path_expr)->toString() == "self"
            || getExpressionType(path_expr)->toString() == "Self") {
          if (currentScope->possible_self == "") {
            std::cout << "invalid self" << std::endl;
            return nullptr;
          } else {
            std::cout << "getting valid self : " << currentScope->possible_self << std::endl;
            if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
              std::string item_name = field_expr->identifier.id;
              std::cout << "finding " << item_name << " in struct" << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "item in struct " << structInfo->name << " : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  return t;
                }
              }
              std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl; 
              return nullptr;
            } else {
              std::cout << "invalid base in FieldExpression : " << currentScope->possible_self << std::endl;
              return nullptr;
            }
          }
        } else {
          std::string item_name = field_expr->identifier.id;
          std::cout << "finding struct : " << path_expr->toString() << std::endl;
          if (auto* structInfo = currentScope->lookupStruct(path_expr->toString())) {
            std::cout << "finding item in struct : " << item_name << std::endl;
            for (int i = 0; i < structInfo->fields.size(); i++) {
              std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
              if (structInfo->fields[i].name == item_name) {
                auto* t = structInfo->fields[i].type;
                return t;
              }
            }
            std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl;
            return nullptr;
          } else {
            auto* type = getExpressionType(path_expr);
            std::string typeStr;
            if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
              typeStr = ref->type->toString();
            } else {
              typeStr = type->toString();
            }
            typeStr.erase(0, typeStr.find_first_not_of('&'));
            if (auto* structInfo = currentScope->lookupStruct(typeStr)) {
              std::cout << "finding item in struct : " << item_name << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  return t;
                }
              }
              std::cout << "unknown item : " << item_name << " in struct : " << getExpressionType(path_expr)->toString() << std::endl;
              return nullptr;
            } 
            std::cout << "invalid base in FieldExpression : " << item_name << std::endl;
            return nullptr;
          }
        }
      } else if (auto* index_expr = dynamic_cast<IndexExpressionNode*>(field_expr->expression.get())) {
        auto* index_type = getExpressionType(index_expr);
        if (auto* path = dynamic_cast<TypePathNode*>(index_type)) {
          std::string item_name = path->toString();
          std::cout << "struct in fieldexpression : " << item_name << std::endl;
          if (auto* structInfo = currentScope->lookupStruct(item_name)) {
            std::cout << "found strcut : " << item_name << " whose field size is " << structInfo->fields.size() << std::endl;
            std::cout << "finding item: " << field_expr->identifier.id << " in struct: " << item_name << std::endl;
            for (int i = 0; i < structInfo->fields.size(); i++) {
              std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
              if (structInfo->fields[i].name == field_expr->identifier.id) {
                auto* t = structInfo->fields[i].type;
                return t;
              }
            }
            std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl;
            return nullptr;
          } else {
            std::cout << "strcut : " << item_name << " not found" << std::endl;
            return nullptr;
          }
        }
      } else if (auto* inner_field = dynamic_cast<FieldExpressionNode*>(field_expr->expression.get())) {
        auto* type = getExpressionType(inner_field);
        if (auto* path_type = dynamic_cast<TypePathNode*>(type)) {
          std::string path = path_type->toString();
          std::cout << "path of innerfield in field expression : " << path << std::endl;
          auto* info = currentScope->lookupStruct(path);
          if (!info) {
            std::cout << "error in finding struct : " << path << std::endl;
          }
          std::string item_name = field_expr->identifier.id;
          std::cout << "finding item in struct : " << item_name << std::endl;
          for (int i = 0; i < info->fields.size(); i++) {
            std::cout << "declared item in field : " << info->fields[i].name << std::endl;
            if (info->fields[i].name == item_name) {
              auto* t = info->fields[i].type;
              return t;
            }
          }
          std::cout << "unknown item : " << item_name << " in struct : " << path << std::endl;
          return nullptr;
        }
      }
    }
    if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expr)) {
      std::cout << "getting type of if expression" << std::endl;
      return get_type_in_if(if_expr);
    }
    if (auto* borrowExpr = dynamic_cast<BorrowExpressionNode*>(expr)) {
      std::cout << "getting type of borrow expression node" << std::endl;
      TypeNode* type = getExpressionType(borrowExpr->expression.get());
      return new ReferenceTypeNode(type, borrowExpr->if_mut, 0, 0);;
    }
    if (auto* pathExpr = dynamic_cast<PathExpressionNode*>(expr)) {
      std::string path = pathExpr->toString();
      std::cout << "path in getting expression type : " << path << std::endl;
      std::string path_pattern = "IdentifierPattern(" + path + ")";
      if (!currentScope->lookupVar(path_pattern) && !currentScope->lookupVar(path)) {
        std::cout << "Variable not found: " << path << " in scope : " << currentScope->id << std::endl;
        std::cout << "var_table size: " << currentScope->var_table.size() << std::endl;
        return nullptr;
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
        std::string s = t->toString();
        size_t pos = s.rfind("::");
        if (pos != std::string::npos) {
          s.erase(pos);
          t = new TypePathNode(s);
        }
        if (auto* inner_array = dynamic_cast<ArrayTypeNode*>(t)) {
          for (int i = 1; i < arrayExpr->expressions.size(); i++) {
            auto* temp_array = dynamic_cast<ArrayTypeNode*>(getExpressionType(arrayExpr->expressions[i].get()));
            if (!temp_array) {
              std::cout << "expected arraytype" << std::endl;
              return nullptr;
            }
            if (!check_arrayType(temp_array, inner_array, currentScope)) {
              std::cout << "arraytype mismatch in rhs of array assignment" << std::endl;
              return nullptr;
            }
          }
        }
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
      std::cout << "getting type of LiteralExpression: " << litExpr->toString() << std::endl;

      if (std::holds_alternative<std::unique_ptr<integer_literal>>(litExpr->literal)) {
        const auto& intLit = std::get<std::unique_ptr<integer_literal>>(litExpr->literal);
        std::string s = intLit->raw;
          
        std::cout << "get integer_literal: " << s << std::endl;
          
        size_t pos = 0;
        while (pos < s.size() && std::isdigit(s[pos])) ++pos;

        bool check = true;
        for(int i = pos; i < s.size(); i++) {
          if (std::isdigit(s[i])) {
            check = false;
            break;
          }
        }
        
        if (s[0] == 0 && s.size() > 1) {
          if (s[1] == 'o' || s[1] == 'x' || s[1] == 'b') {
            check = false;
          }
        }
          
        std::string suffix;
        std::string type;

        if (check) {
          suffix = s.substr(pos);
        }
          
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
              std::cout << "getting valid self : " << currentScope->possible_self << std::endl;
              if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
                std::cout << "getting valid struct of self" << std::endl;
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
          std::cout << "path_expr: " << path_expr->toString() << std::endl;
          if (path_expr->toString() == "self" || path_expr->toString() == "Self") {
            if (currentScope->possible_self == "") {
              std::cout << "invalid self" << std::endl;
              return nullptr;
            } else {
              std::cout << "getting valid self : " << currentScope->possible_self << std::endl;
              if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
                std::string item_name = field_expr->identifier.id;
                std::cout << "finding item in struct : " << item_name << std::endl;
                for (int i = 0; i < structInfo->fields.size(); i++) {
                  std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                  if (structInfo->fields[i].name == item_name) {
                    auto* t = structInfo->fields[i].type;
                    if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                      return array->type.get();
                    }
                  }
                }
                std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl; 
                return nullptr;
              } else {
                std::cout << "invalid base in FieldExpression : " << currentScope->possible_self << std::endl;
                return nullptr;
              }
            }
          } else {
            std::string item_name = field_expr->identifier.id;
            std::cout << "finding struct : " << path_expr->toString() << std::endl;
            if (auto* structInfo = currentScope->lookupStruct(path_expr->toString())) {
              std::cout << "finding item in struct : " << item_name << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                    return array->type.get();
                  }
                }
              }
              std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl;
              return nullptr;
            } else {
              std::string typeStr = getExpressionType(path_expr)->toString();
              typeStr.erase(0, typeStr.find_first_not_of('&'));
              if (auto* structInfo = currentScope->lookupStruct(typeStr)) {
                std::cout << "finding item in struct : " << item_name << std::endl;
                for (int i = 0; i < structInfo->fields.size(); i++) {
                  std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                  if (structInfo->fields[i].name == item_name) {
                    auto* t = structInfo->fields[i].type;
                    if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                      return array->type.get();
                    }
                  }
                }
                std::cout << "unknown item : " << item_name << " in struct : " << getExpressionType(path_expr)->toString() << std::endl;
                return nullptr;
              } 
              std::cout << "invalid base in FieldExpression : " << item_name << std::endl;
              return nullptr;
            }
          }
        }
      } else if (auto* borrow = dynamic_cast<BorrowExpressionNode*>(index_expr->base.get())) {
        auto* path = dynamic_cast<PathExpressionNode*>(borrow->expression.get());
        if (path) {
          if (auto* symbol = currentScope->lookupVar(path->toString())) {
            bool if_mut = symbol->isMutable;
            std::cout << "path in indexpression : " << path->toString() << std::endl;
            if (auto* array = dynamic_cast<ArrayTypeNode*>(symbol->type)) {
              return new ReferenceTypeNode(array->type.get(), if_mut, 0, 0);
            } else if (auto* ref = dynamic_cast<ReferenceTypeNode*>(symbol->type)) {
              if (auto* array = dynamic_cast<ArrayTypeNode*>(ref->type.get())) {
                return new ReferenceTypeNode(array->type.get(), if_mut, 0, 0);
              }
            }
            return new ReferenceTypeNode(symbol->type, if_mut, 0, 0);
          } else {
            std::cout << "path not found in indexexpression: " << path->toString() << std::endl;
            return nullptr;
          }
        }        
      } else {
        std::cout << "unknown type in indexexpression" << std::endl;
      }
    }
    if (auto* logic_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr)) {
      std::cout << "getting type of ArithmeticOrLogicalExpressionNode" << std::endl;
      auto* expr1 = logic_expr->expression1.get();
      auto* type = getExpressionType(expr1);
      if (!type) {
        std::cerr << "failed to get type of certain expression in arithmetic or logical expression" << std::endl;
        return nullptr;
      }
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
        else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
          return getExpressionType(node_ptr->expr.get());
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
        else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
          return getExpressionType(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type.get();
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get());
        }
      }, ewb->expr);
    } else if (auto* call = dynamic_cast<CallExpressionNode*>(expr)) {
      std::cout << "getting callexpression" << std::endl;
      if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
        std::string name = path->toString();
        std::cout << "func name in call expression: " << name << std::endl;
        auto* func_info = currentScope->lookupFunc(name);
        auto* func_type = currentScope->get_function_type(name);
        if (!func_type) {
          std::cout << "the function in call expression has no return type" << std::endl;
          return nullptr;
        }
        std::cout << "get type of callexpression : " << func_type->toString() << std::endl;
        if (func_type->toString() == "self" || func_type->toString() == "Self") {
          std::string s = name;
          std::size_t pos = s.rfind("::");
          if (pos != std::string::npos) {
            s = s.substr(0, pos);
          }
          return new TypePathNode(s);
        }
        return func_type;
      } else {
        std::cout << "fail to get type of callexpression" << std::endl;
        return nullptr;
      }
    } else if (auto* typecast = dynamic_cast<TypeCastExpressionNode*>(expr)) {
      std::cout << "getting type of typecast expression" << std::endl;
      return typecast->type.get();
    } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expr)) {
      std::cout << "getting type of break expression" << std::endl;
      return getExpressionType(bre->expr.get());
    } else if (auto* index = dynamic_cast<IndexExpressionNode*>(expr)) {
      std::cout << "getting type of index expression" << std::endl;
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
    } else if (auto* neg = dynamic_cast<NegationExpressionNode*>(expr)) {
      std::cout << "getting type of negation expression" << std::endl;
      return getExpressionType(neg->expression.get());
    } else if (auto* comp = dynamic_cast<ComparisonExpressionNode*>(expr)) {
      std::cout << "getting type of comparison expression" << std::endl;
      return new TypePathNode("bool");
    } else if (auto* lazy_bool = dynamic_cast<LazyBooleanExpressionNode*>(expr)) {
      std::cout << "getting type of lazy bool expression" << std::endl;
      return new TypePathNode("bool");
    } else if (auto* struct_expr = dynamic_cast<StructExpressionNode*>(expr)) {
      std::cout << "getting type of struct expression" << std::endl;
      std::string struct_name = struct_expr->pathin_expression->toString();
      return new TypePathNode(struct_name);
    } else if (auto* method_call = dynamic_cast<MethodCallExpressionNode*>(expr)) {
      std::cout << "getting type of method call expression" << std::endl;
      auto* type = getExpressionType(method_call->expression.get());
      if (auto* type_path = dynamic_cast<TypePathNode*>(type)) {
        std::string base = type_path->toString();
        std::cout << "base of methodcall expression : " << base << std::endl;
        std::string func_name = method_call->PathtoString();
        std::cout << "function of methodcall expression: " << func_name << std::endl;
        std::string func = base + "::" + func_name;
        auto* func_type = currentScope->get_function_type(func);
        if (!func_type) {
          std::cout << "function: " << func << " not found" << std::endl;
          return nullptr;
        }
        return func_type;
      } else if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
        std::cout << "getting type in reference type" << std::endl;
        auto* inner_type = ref->type.get();
        if (auto* type_path = dynamic_cast<TypePathNode*>(inner_type)) {
          std::string base = type_path->toString();
          std::cout << "base of methodcall expression : " << base << std::endl;
          std::string func_name = method_call->PathtoString();
          std::cout << "function of methodcall expression: " << func_name << std::endl;
          std::string func = base + "::" + func_name;
          auto* func_type = currentScope->get_function_type(func);
          if (!func_type) {
            std::cout << "function: " << func << " not found" << std::endl;
            return nullptr;
          }
          return func_type;
        }
      } else if (auto* array = dynamic_cast<ArrayTypeNode*>(type)) {
        if (method_call->PathtoString() == "len") {
          return new TypePathNode("usize");
        }
      }
    }
    // TODO: 其他表达式类型...
    std::cout << "other type of expression : " << typeid(*expr).name() << std::endl;
    return nullptr;
  }

  TypeNode* getExpressionTypeInLet(ExpressionNode* expr) {
    if (auto* block = dynamic_cast<BlockExpressionNode*>(expr)) {
      enterScope();
      for (int i = 0; i < block->statement.size(); i++) {
        if (block->statement[i]->let_statement) {
          try {
            std::cout << "try to declare var : " << block->statement[i]->let_statement->pattern->toString()  << " in scope :" << currentScope->id << "whose if_mut is " << block->statement[i]->let_statement->get_if_mutable() << std::endl;
            declareVariable(block->statement[i]->let_statement->pattern->toString(), block->statement[i]->let_statement->type.get(), block->statement[i]->let_statement->get_if_mutable());
          } catch (const std::exception& e) {
            std::cerr << "[Declare Error in LetStatement] : " << e.what() << std::endl;
          }
        }
        auto* stat = block->statement[i].get();
        if (auto* expr = stat->expr_statement.get()) {
          auto* expression = expr->expression.get();
          if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expression)) {
            std::cout << "getting type of if expression in block expression" << std::endl;
            if (getExpressionType(if_expr)) {
              auto* res = getExpressionType(if_expr);
              exitScope();
              return res;
            }
          } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expression)) {
            std::cout << "getting type of break expression in block expression" << std::endl;
            auto* res = getExpressionType(bre->expr.get());
            exitScope();
            return res;
          }
        }
      }
      if (block->expression_without_block) {
        std::cout << "having ewb in block" << std::endl;
        auto* res = getExpressionType(block->expression_without_block.get());
        exitScope();
        return res;
      }
      exitScope();
    }
    if (auto* indexExpr = dynamic_cast<IndexExpressionNode*>(expr)) {
      TypeNode* arrayType = getExpressionType(indexExpr->base.get());
      if (auto* arr = dynamic_cast<ArrayTypeNode*>(arrayType)) {
        return arr->type.get();
      }
    }
    if (auto* returnExpr = dynamic_cast<ReturnExpressionNode*>(expr)) {
      return getExpressionType(returnExpr->expression.get());
    }
    if (auto* DerefExpr = dynamic_cast<DereferenceExpressionNode*>(expr)) {
      std::cout << "getting type of dereference expression" << std::endl;
      if (auto* ref = dynamic_cast<ReferenceTypeNode*>(getExpressionType(DerefExpr->expression.get()))) {
        return ref->type.get();
      } 
    }
    if (auto* loop = dynamic_cast<InfiniteLoopExpressionNode*>(expr)) {
      std::cout << "getting type of loop expression" << std::endl;
      return get_type_in_loop(loop);
    }
    if (auto* field_expr = dynamic_cast<FieldExpressionNode*>(expr)) {
      std::cout << "getting type of field expression" << std::endl;
      if (auto* path_expr = dynamic_cast<PathExpressionNode*>(field_expr->expression.get())) {
        std::cout << "path_expr: " << path_expr->toString() << std::endl;
        if (path_expr->toString() == "self" || path_expr->toString() == "Self" 
            || getExpressionType(path_expr)->toString() == "self"
            || getExpressionType(path_expr)->toString() == "Self") {
          if (currentScope->possible_self == "") {
            std::cout << "invalid self" << std::endl;
            return nullptr;
          } else {
            std::cout << "getting valid self : " << currentScope->possible_self << std::endl;
            if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
              std::string item_name = field_expr->identifier.id;
              std::cout << "finding " << item_name << " in struct" << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "item in struct " << structInfo->name << " : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  return t;
                }
              }
              std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl; 
              return nullptr;
            } else {
              std::cout << "invalid base in FieldExpression : " << currentScope->possible_self << std::endl;
              return nullptr;
            }
          }
        } else {
          std::string item_name = field_expr->identifier.id;
          std::cout << "finding struct : " << path_expr->toString() << std::endl;
          if (auto* structInfo = currentScope->lookupStruct(path_expr->toString())) {
            std::cout << "finding item in struct : " << item_name << std::endl;
            for (int i = 0; i < structInfo->fields.size(); i++) {
              std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
              if (structInfo->fields[i].name == item_name) {
                auto* t = structInfo->fields[i].type;
                return t;
              }
            }
            std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl;
            return nullptr;
          } else {
            auto* type = getExpressionType(path_expr);
            std::string typeStr;
            if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
              typeStr = ref->type->toString();
            } else {
              typeStr = type->toString();
            }
            typeStr.erase(0, typeStr.find_first_not_of('&'));
            if (auto* structInfo = currentScope->lookupStruct(typeStr)) {
              std::cout << "finding item in struct : " << item_name << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  return t;
                }
              }
              std::cout << "unknown item : " << item_name << " in struct : " << getExpressionType(path_expr)->toString() << std::endl;
              return nullptr;
            } 
            std::cout << "invalid base in FieldExpression : " << item_name << std::endl;
            return nullptr;
          }
        }
      } else if (auto* index_expr = dynamic_cast<IndexExpressionNode*>(field_expr->expression.get())) {
        auto* index_type = getExpressionType(index_expr);
        if (auto* path = dynamic_cast<TypePathNode*>(index_type)) {
          std::string item_name = path->toString();
          std::cout << "struct in fieldexpression : " << item_name << std::endl;
          if (auto* structInfo = currentScope->lookupStruct(item_name)) {
            std::cout << "found strcut : " << item_name << " whose field size is " << structInfo->fields.size() << std::endl;
            std::cout << "finding item: " << field_expr->identifier.id << " in struct: " << item_name << std::endl;
            for (int i = 0; i < structInfo->fields.size(); i++) {
              std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
              if (structInfo->fields[i].name == field_expr->identifier.id) {
                auto* t = structInfo->fields[i].type;
                return t;
              }
            }
            std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl;
            return nullptr;
          } else {
            std::cout << "strcut : " << item_name << " not found" << std::endl;
            return nullptr;
          }
        }
      } else if (auto* inner_field = dynamic_cast<FieldExpressionNode*>(field_expr->expression.get())) {
        auto* type = getExpressionType(inner_field);
        if (auto* path_type = dynamic_cast<TypePathNode*>(type)) {
          std::string path = path_type->toString();
          std::cout << "path of innerfield in field expression : " << path << std::endl;
          auto* info = currentScope->lookupStruct(path);
          if (!info) {
            std::cout << "error in finding struct : " << path << std::endl;
          }
          std::string item_name = field_expr->identifier.id;
          std::cout << "finding item in struct : " << item_name << std::endl;
          for (int i = 0; i < info->fields.size(); i++) {
            std::cout << "declared item in field : " << info->fields[i].name << std::endl;
            if (info->fields[i].name == item_name) {
              auto* t = info->fields[i].type;
              return t;
            }
          }
          std::cout << "unknown item : " << item_name << " in struct : " << path << std::endl;
          return nullptr;
        }
      }
    }
    if (auto* if_expr = dynamic_cast<IfExpressionNode*>(expr)) {
      std::cout << "getting type of if expression" << std::endl;
      return get_type_in_if_in_let(if_expr);
    }
    if (auto* borrowExpr = dynamic_cast<BorrowExpressionNode*>(expr)) {
      std::cout << "getting type of borrow expression node" << std::endl;
      TypeNode* type = getExpressionType(borrowExpr->expression.get());
      return new ReferenceTypeNode(type, borrowExpr->if_mut, 0, 0);;
    }
    if (auto* pathExpr = dynamic_cast<PathExpressionNode*>(expr)) {
      std::string path = pathExpr->toString();
      std::cout << "path in getting expression type : " << path << std::endl;
      std::string path_pattern = "IdentifierPattern(" + path + ")";
      if (!currentScope->lookupVar(path_pattern) && !currentScope->lookupVar(path)) {
        std::cout << "Variable not found: " << path << " in scope : " << currentScope->id << std::endl;
        std::cout << "var_table size: " << currentScope->var_table.size() << std::endl;
        return nullptr;
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
        std::string s = t->toString();
        size_t pos = s.rfind("::");
        if (pos != std::string::npos) {
          s.erase(pos);
          t = new TypePathNode(s);
        }
        if (auto* inner_array = dynamic_cast<ArrayTypeNode*>(t)) {
          for (int i = 1; i < arrayExpr->expressions.size(); i++) {
            auto* temp_array = dynamic_cast<ArrayTypeNode*>(getExpressionType(arrayExpr->expressions[i].get()));
            if (!temp_array) {
              std::cout << "expected arraytype" << std::endl;
              return nullptr;
            }
            if (!check_arrayType(temp_array, inner_array, currentScope)) {
              std::cout << "arraytype mismatch in rhs of array assignment" << std::endl;
              return nullptr;
            }
          }
        }
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
              std::cout << "getting valid self : " << currentScope->possible_self << std::endl;
              if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
                std::cout << "getting valid struct of self" << std::endl;
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
          std::cout << "path_expr: " << path_expr->toString() << std::endl;
          if (path_expr->toString() == "self" || path_expr->toString() == "Self") {
            if (currentScope->possible_self == "") {
              std::cout << "invalid self" << std::endl;
              return nullptr;
            } else {
              std::cout << "getting valid self : " << currentScope->possible_self << std::endl;
              if (auto* structInfo = currentScope->lookupStruct(currentScope->possible_self)) {
                std::string item_name = field_expr->identifier.id;
                std::cout << "finding item in struct : " << item_name << std::endl;
                for (int i = 0; i < structInfo->fields.size(); i++) {
                  std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                  if (structInfo->fields[i].name == item_name) {
                    auto* t = structInfo->fields[i].type;
                    if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                      return array->type.get();
                    }
                  }
                }
                std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl; 
                return nullptr;
              } else {
                std::cout << "invalid base in FieldExpression : " << currentScope->possible_self << std::endl;
                return nullptr;
              }
            }
          } else {
            std::string item_name = field_expr->identifier.id;
            std::cout << "finding struct : " << path_expr->toString() << std::endl;
            if (auto* structInfo = currentScope->lookupStruct(path_expr->toString())) {
              std::cout << "finding item in struct : " << item_name << std::endl;
              for (int i = 0; i < structInfo->fields.size(); i++) {
                std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                if (structInfo->fields[i].name == item_name) {
                  auto* t = structInfo->fields[i].type;
                  if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                    return array->type.get();
                  }
                }
              }
              std::cout << "unknown item : " << item_name << " in struct : " << path_expr->toString() << std::endl;
              return nullptr;
            } else {
              std::string typeStr = getExpressionType(path_expr)->toString();
              typeStr.erase(0, typeStr.find_first_not_of('&'));
              if (auto* structInfo = currentScope->lookupStruct(typeStr)) {
                std::cout << "finding item in struct : " << item_name << std::endl;
                for (int i = 0; i < structInfo->fields.size(); i++) {
                  std::cout << "declared item in field : " << structInfo->fields[i].name << std::endl;
                  if (structInfo->fields[i].name == item_name) {
                    auto* t = structInfo->fields[i].type;
                    if (auto* array = dynamic_cast<ArrayTypeNode*>(t)) {
                      return array->type.get();
                    }
                  }
                }
                std::cout << "unknown item : " << item_name << " in struct : " << getExpressionType(path_expr)->toString() << std::endl;
                return nullptr;
              } 
              std::cout << "invalid base in FieldExpression : " << item_name << std::endl;
              return nullptr;
            }
          }
        }
      } else if (auto* borrow = dynamic_cast<BorrowExpressionNode*>(index_expr->base.get())) {
        auto* path = dynamic_cast<PathExpressionNode*>(borrow->expression.get());
        if (path) {
          if (auto* symbol = currentScope->lookupVar(path->toString())) {
            bool if_mut = symbol->isMutable;
            std::cout << "path in indexpression : " << path->toString() << std::endl;
            if (auto* array = dynamic_cast<ArrayTypeNode*>(symbol->type)) {
              return new ReferenceTypeNode(array->type.get(), if_mut, 0, 0);
            } else if (auto* ref = dynamic_cast<ReferenceTypeNode*>(symbol->type)) {
              if (auto* array = dynamic_cast<ArrayTypeNode*>(ref->type.get())) {
                return new ReferenceTypeNode(array->type.get(), if_mut, 0, 0);
              }
            }
            return new ReferenceTypeNode(symbol->type, if_mut, 0, 0);
          } else {
            std::cout << "path not found in indexexpression: " << path->toString() << std::endl;
            return nullptr;
          }
        }        
      } else {
        std::cout << "unknown type in indexexpression" << std::endl;
      }
    }
    if (auto* logic_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr)) {
      std::cout << "getting type of ArithmeticOrLogicalExpressionNode" << std::endl;
      auto* expr1 = logic_expr->expression1.get();
      auto* type = getExpressionType(expr1);
      if (!type) {
        std::cerr << "failed to get type of certain expression in arithmetic or logical expression" << std::endl;
        return nullptr;
      }
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
        else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
          return getExpressionType(node_ptr->expr.get());
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
        else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
          return getExpressionType(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return node_ptr->type.get();
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return getExpressionType(node_ptr->expression.get());
        }
      }, ewb->expr);
    } else if (auto* call = dynamic_cast<CallExpressionNode*>(expr)) {
      std::cout << "getting callexpression" << std::endl;
      if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
        std::string name = path->toString();
        std::cout << "func name in call expression: " << name << std::endl;
        auto* func_info = currentScope->lookupFunc(name);
        auto* func_type = currentScope->get_function_type(name);
        if (!func_type) {
          std::cout << "the function in call expression has no return type" << std::endl;
          return nullptr;
        }
        std::cout << "get type of callexpression : " << func_type->toString() << std::endl;
        if (func_type->toString() == "self" || func_type->toString() == "Self") {
          std::string s = name;
          std::size_t pos = s.rfind("::");
          if (pos != std::string::npos) {
            s = s.substr(0, pos);
          }
          return new TypePathNode(s);
        }
        return func_type;
      } else {
        std::cout << "fail to get type of callexpression" << std::endl;
        return nullptr;
      }
    } else if (auto* typecast = dynamic_cast<TypeCastExpressionNode*>(expr)) {
      std::cout << "getting type of typecast expression" << std::endl;
      return typecast->type.get();
    } else if (auto* bre = dynamic_cast<BreakExpressionNode*>(expr)) {
      std::cout << "getting type of break expression" << std::endl;
      return getExpressionType(bre->expr.get());
    } else if (auto* index = dynamic_cast<IndexExpressionNode*>(expr)) {
      std::cout << "getting type of index expression" << std::endl;
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
    } else if (auto* neg = dynamic_cast<NegationExpressionNode*>(expr)) {
      std::cout << "getting type of negation expression" << std::endl;
      return getExpressionType(neg->expression.get());
    } else if (auto* comp = dynamic_cast<ComparisonExpressionNode*>(expr)) {
      std::cout << "getting type of comparison expression" << std::endl;
      return new TypePathNode("bool");
    } else if (auto* lazy_bool = dynamic_cast<LazyBooleanExpressionNode*>(expr)) {
      std::cout << "getting type of lazy bool expression" << std::endl;
      return new TypePathNode("bool");
    } else if (auto* struct_expr = dynamic_cast<StructExpressionNode*>(expr)) {
      std::cout << "getting type of struct expression" << std::endl;
      std::string struct_name = struct_expr->pathin_expression->toString();
      return new TypePathNode(struct_name);
    } else if (auto* method_call = dynamic_cast<MethodCallExpressionNode*>(expr)) {
      std::cout << "getting type of method call expression" << std::endl;
      auto* type = getExpressionType(method_call->expression.get());
      if (auto* type_path = dynamic_cast<TypePathNode*>(type)) {
        std::string base = type_path->toString();
        std::cout << "base of methodcall expression : " << base << std::endl;
        std::string func_name = method_call->PathtoString();
        std::cout << "function of methodcall expression: " << func_name << std::endl;
        std::string func = base + "::" + func_name;
        auto* func_type = currentScope->get_function_type(func);
        if (!func_type) {
          std::cout << "function: " << func << " not found" << std::endl;
          return nullptr;
        }
        return func_type;
      } else if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
        std::cout << "getting type in reference type" << std::endl;
        auto* inner_type = ref->type.get();
        if (auto* type_path = dynamic_cast<TypePathNode*>(inner_type)) {
          std::string base = type_path->toString();
          std::cout << "base of methodcall expression : " << base << std::endl;
          std::string func_name = method_call->PathtoString();
          std::cout << "function of methodcall expression: " << func_name << std::endl;
          std::string func = base + "::" + func_name;
          auto* func_type = currentScope->get_function_type(func);
          if (!func_type) {
            std::cout << "function: " << func << " not found" << std::endl;
            return nullptr;
          }
          return func_type;
        }
      } else if (auto* array = dynamic_cast<ArrayTypeNode*>(type)) {
        if (method_call->PathtoString() == "len") {
          return new TypePathNode("usize");
        }
      }
    }
    // TODO: 其他表达式类型...
    std::cout << "other type of expression : " << typeid(*expr).name() << std::endl;
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
    std::cout << "func: check_arrayType" << std::endl;
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
    if (!expr) {
      std::cout << "error: no letstatement" << std::endl;
      return false;
    }
    if (expr->let_statement == nullptr) return true;
    std::cout << "checking LetStatement with pattern : " << expr->let_statement->pattern->toString() << std::endl;
    LetStatement& letStatement = *(expr->let_statement);

    std::cout << "checking expression in letstatement" << std::endl;
    if (!check_expression(letStatement.expression.get())) {
      std::cerr << "error in expression in letstatement" << std::endl;
      return false;
    }
    std::cout << "finish checking expression in letstatement with pattern : " << expr->let_statement->pattern->toString() << std::endl;

    if (auto* if_expr = dynamic_cast<IfExpressionNode*>(letStatement.expression.get())) {
      std::cout << "checking return type of if expression in letstatement" << std::endl;
      auto types = get_return_type_in_if_in_let(if_expr);
      if (types.size() != 1) {
        if (!(types.size() == 2 && types.count("NeverType"))) {
          std::cout << "size of types: " << types.size() << std::endl;
          std::cout << "return-type mismatch in if expression" << std::endl;
          return false;
        }
      }
    }

    if (!getExpressionType(letStatement.expression.get())) {
      std::cout << "failed to get type of expression in letstatement" << std::endl;
      return false;
    }

    if (letStatement.type->toString() != getExpressionTypeInLet(letStatement.expression.get())->toString()) {
      std::cout << "declared type : " << letStatement.type->toString() << std::endl;
      std::cout << "type of rhs : " << getExpressionTypeInLet(letStatement.expression.get())->toString() << std::endl;
      std::string s1 = letStatement.type->toString();
      std::string s2 = getExpressionTypeInLet(letStatement.expression.get())->toString();
      size_t start = s1.find_first_not_of('[');
      size_t end = s1.find_last_not_of(']');
      if (start != std::string::npos && end != std::string::npos) {
        s1 = s1.substr(start, end - start + 1);
      } else {
        s1 = "";
      }
      start = s2.find_first_not_of('[');
      end = s2.find_last_not_of(']');
      if (start != std::string::npos && end != std::string::npos) {
        s2 = s2.substr(start, end - start + 1);
      } else {
        s2 = "";
      }
      if (!((s1 == "usize" || s1 == "u32") && s2 == "i32")) {
        std::cerr << "type mismatch in letstatement" << std::endl;
        std::cout << "s1: " << s1 << std::endl;
        std::cout << "s2: " << s2 << std::endl;
        return false;
      }
    }

    if (auto* possible_underscore = dynamic_cast<UnderscoreExpressionNode*>(expr->let_statement->expression.get())) {
      std::cerr << "underscore expression is not allowed in RHS" << std::endl;
      throw std::runtime_error("underscore expression is not allowed in RHS");
    }

    try {
      std::cout << "try to declare var : " << letStatement.pattern->toString()  << " in scope :" << currentScope->id << "whose if_mut is " << letStatement.get_if_mutable() << std::endl;
      declareVariable(letStatement.pattern->toString(), letStatement.type.get(), letStatement.get_if_mutable());
    } catch (const std::exception& e) {
      std::cerr << "[Declare Error in LetStatement] : " << e.what() << std::endl;
    }
    //Array:检查类型和数量
    if (auto *d = dynamic_cast<ArrayTypeNode*>(letStatement.type.get())) {
      std::cout << "checking array type in letstatement" << std::endl;
      //检查数量
      auto *rhs = dynamic_cast<ArrayExpressionNode*>(letStatement.expression.get());
      auto *call_expr = dynamic_cast<CallExpressionNode*>(letStatement.expression.get());
      auto *path_expr = dynamic_cast<PathExpressionNode*>(letStatement.expression.get());
      auto *index_expr = dynamic_cast<IndexExpressionNode*>(letStatement.expression.get());
      auto *method_call = dynamic_cast<MethodCallExpressionNode*>(letStatement.expression.get());
      auto *field = dynamic_cast<FieldExpressionNode*>(letStatement.expression.get());
      auto* deref = dynamic_cast<DereferenceExpressionNode*>(letStatement.expression.get());
      if (!rhs && !call_expr && !path_expr && !index_expr && !method_call && !field && !deref) {
        std::cout << "Expected array expression, pathexpression, indexexpression, derefexpr, field expression or call expression in array assignment" << std::endl;
        std::cout << "expression type in letstatement : " << typeid(*letStatement.expression.get()).name() << std::endl;
        return false;
      }
      if (method_call) {
        return true;
      }
      if (deref) {
        if (auto* path = dynamic_cast<PathExpressionNode*>(deref->expression.get())) {
          std::cout << "getting path: " << path << std::endl;
          auto* type = currentScope->lookupVar(path->toString())->type; 
          std::cout << "getting corresponding type: " << type->toString() << std::endl;
          if (!type) {
            std::cout << "variable not found: " << path->toString() << std::endl;
            return false;
          }
          if (auto* rhs_ref = dynamic_cast<ReferenceTypeNode*>(type)) {
            if (auto* rhs_array = dynamic_cast<ArrayTypeNode*>(rhs_ref->type.get())) {
              if (!check_arrayType(rhs_array, d, currentScope)) {
                std::cout << "array type mismatch in let statement" << std::endl;
                return false;
              }
            }
          } else {
            return false;
          }
        }
        return true;
      }
      if (call_expr) {
        return true;
      }
      if (index_expr) {
        if (auto* path = dynamic_cast<PathExpressionNode*>(index_expr->base.get())) {
          std::cout << "getting path: " << path << std::endl;
          auto* type = currentScope->lookupVar(path->toString())->type; 
          std::cout << "getting corresponding type: " << type->toString() << std::endl;
          if (!type) {
            std::cout << "variable not found: " << path->toString() << std::endl;
            return false;
          }
          if (auto* rhs_array = dynamic_cast<ArrayTypeNode*>(type)) {
            if (!dynamic_cast<ArrayTypeNode*>(rhs_array->type.get())) {
              std::cout << "array type mismatch in letstament" << std::endl;
              return false;
            }
            if (!check_arrayType(dynamic_cast<ArrayTypeNode*>(rhs_array->type.get()), d, currentScope)) {
              std::cout << "array type mismatch in letstament" << std::endl;
              return false;
            }
          } else {
            return false;
          }
        }
        return true;
      }
      if (field) {
        return true;
      }
      if (path_expr) {
        std::cout << "getting path: " << path_expr->toString() << std::endl;
        auto* symbol = currentScope->lookupVar(path_expr->toString());
        if (symbol) {
          std::cout << "getting symbol with type: " << symbol->type->toString() << std::endl;
          if (auto* array_type = dynamic_cast<ArrayTypeNode*>(symbol->type)) {
            int declaredLength = -1;
            if (auto *lenLit = dynamic_cast<LiteralExpressionNode*>(d->expression.get())) {
              if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                declaredLength = std::stoi(intLit->value);
              } else {
                std::cout << "wrong type of length in initializer" << std::endl;
                return false;
              }
            } else if (auto *lenVar = dynamic_cast<PathExpressionNode*>(d->expression.get())) {
              std::string path = lenVar->toString();
              auto* info = currentScope->lookupConst(path);
              if (info) {
                if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
                  if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                    declaredLength = std::stoi(intLit->value);
                  } else {
                    std::cout << "wrong type of length in initializer" << std::endl;
                    return false;
                  }
                }
              }              
            } else if (auto *lenVar = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(d->expression.get())) {
              //只处理了二元的情况
              std::string left;
              std::string right;
              if (auto* left_path = dynamic_cast<PathExpressionNode*>(lenVar->expression1.get())) {
                left = left_path->toString();
                std::cout << "left: " << left << std::endl;    
                auto* info = currentScope->lookupConst(left);
                if (info) {
                  if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
                    if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                      left = intLit->value;
                    } else {
                      std::cout << "wrong type of length in initializer" << std::endl;
                      return false;
                    }
                  }
                }               
              } else if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(lenVar->expression1.get())) {
                if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                  left = intLit->value;
                  std::cout << "left: " << left << std::endl;   
                } else {
                  std::cout << "wrong type of length in initializer" << std::endl;
                  return false;
                }
              }
              if (auto* right_path = dynamic_cast<PathExpressionNode*>(lenVar->expression2.get())) {
                right = right_path->toString();
                std::cout << "right: " << right << std::endl;                
              } else if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(lenVar->expression2.get())) {
                if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                  right = intLit->value;
                  std::cout << "right: " << right << std::endl;   
                } else {
                  std::cout << "wrong type of length in initializer" << std::endl;
                  return false;
                }
              }
              //ADD, MINUS, MUL, DIV, MOD, AND, OR, XOR, SHL, SHR
              switch(lenVar->type) {
                case(OperationType::ADD): declaredLength = std::stoi(left) + std::stoi(right); break;
                case(OperationType::MINUS): declaredLength = std::stoi(left) - std::stoi(right); break;
                case(OperationType::MUL): declaredLength = std::stoi(left) * std::stoi(right); break;
                case(OperationType::DIV): declaredLength = std::stoi(left) / std::stoi(right); break;
                case(OperationType::MOD): declaredLength = std::stoi(left) % std::stoi(right); break;
                case(OperationType::AND): declaredLength = std::stoi(left) & std::stoi(right); break;
                case(OperationType::OR): declaredLength = std::stoi(left) | std::stoi(right); break;
                case(OperationType::XOR): declaredLength = std::stoi(left) ^ std::stoi(right); break;
                case(OperationType::SHL): declaredLength = std::stoi(left) << std::stoi(right); break;
                case(OperationType::SHR): declaredLength = std::stoi(left) >> std::stoi(right); break;
                default: std::cout << "unknown type of operation" << std::endl; return false;
              };
            }
            std::cout << "getting declared length: " << declaredLength << std::endl;
            int itemLength = -1;
            if (auto *lenLit = dynamic_cast<LiteralExpressionNode*>(array_type->expression.get())) {
              if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                itemLength = std::stoi(intLit->value);
              } else {
                std::cout << "wrong type of length in initializer" << std::endl;
                return false;
              }
            } else if (auto* lenVar = dynamic_cast<PathExpressionNode*>(array_type->expression.get())) {
              std::cout << "path of length in array type of rhs in letstatement: " << lenVar->toString() << std::endl;
              auto* info = currentScope->lookupConst(lenVar->toString());
              if (info) {
                if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
                  if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                    itemLength = std::stoi(intLit->value);
                  } else {
                    std::cout << "wrong type of length in initializer" << std::endl;
                    return false;
                  }
                }
              }
            } else if (auto *lenVar = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(array_type->expression.get())) {
              //只处理了二元的情况
              std::string left;
              std::string right;
              if (auto* left_path = dynamic_cast<PathExpressionNode*>(lenVar->expression1.get())) {
                left = left_path->toString();
                std::cout << "left: " << left << std::endl;
                auto* info = currentScope->lookupConst(left);
                if (info) {
                  if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(info->expr)) {
                    if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                      left = intLit->value;
                    } else {
                      std::cout << "wrong type of length in initializer" << std::endl;
                      return false;
                    }
                  }
                }      
              } else if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(lenVar->expression1.get())) {
                if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                  left = intLit->value;
                  std::cout << "left: " << left << std::endl;   
                } else {
                  std::cout << "wrong type of length in initializer" << std::endl;
                  return false;
                }
              }
              if (auto* right_path = dynamic_cast<PathExpressionNode*>(lenVar->expression2.get())) {
                right = right_path->toString();
                std::cout << "right: " << right << std::endl;                
              } else if (auto* lenLit = dynamic_cast<LiteralExpressionNode*>(lenVar->expression2.get())) {
                if (auto& intLit = std::get<std::unique_ptr<integer_literal>>(lenLit->literal)) {
                  right = intLit->value;
                  std::cout << "right: " << right << std::endl;   
                } else {
                  std::cout << "wrong type of length in initializer" << std::endl;
                  return false;
                }
              }
              //ADD, MINUS, MUL, DIV, MOD, AND, OR, XOR, SHL, SHR
              switch(lenVar->type) {
                case(OperationType::ADD): itemLength = std::stoi(left) + std::stoi(right); break;
                case(OperationType::MINUS): itemLength = std::stoi(left) - std::stoi(right); break;
                case(OperationType::MUL): itemLength = std::stoi(left) * std::stoi(right); break;
                case(OperationType::DIV): itemLength = std::stoi(left) / std::stoi(right); break;
                case(OperationType::MOD): itemLength = std::stoi(left) % std::stoi(right); break;
                case(OperationType::AND): itemLength = std::stoi(left) & std::stoi(right); break;
                case(OperationType::OR): itemLength = std::stoi(left) | std::stoi(right); break;
                case(OperationType::XOR): itemLength = std::stoi(left) ^ std::stoi(right); break;
                case(OperationType::SHL): itemLength = std::stoi(left) << std::stoi(right); break;
                case(OperationType::SHR): itemLength = std::stoi(left) >> std::stoi(right); break;
                default: std::cout << "unknown type of operation" << std::endl; return false;
              };
            }
            std::cout << "getting itemlength: " << itemLength << std::endl;
            if (itemLength != declaredLength && declaredLength != 999) {
              std::cout << "itemLength: " << itemLength << std::endl;
              std::cout << "declaredLength: " << declaredLength << std::endl;
              std::cout << "length mismatch in array assignment in letstatement" << std::endl;
              return false;
            }
          } else {
            std::cout << "type mismatch in letstatement, left: arraytype" << std::endl;
            return false;
          }
        }
        else {
          std::cout << "var not found :" << path_expr->toString() << std::endl;
          return false;
        }
      }
      TypeNode* t;
      if (rhs) t = getExpressionType(rhs);
      else if (path_expr) t = getExpressionType(path_expr);
      else return true;
      bool check_array = check_arrayType(d, dynamic_cast<ArrayTypeNode*>(t), currentScope);
      if (!check_array) return false;
      std::cout << "finish function check_arrayType" << std::endl;
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
      if (rhs) {
        if (rhs->type == ArrayExpressionType::LITERAL) {
          std::cout << "get literal array expression" << std::endl;
          itemLength = rhs->expressions.size();
          std::string type = getExpressionType(rhs->expressions[0].get())->toString();
          size_t pos = type.rfind("::");
          if (pos != std::string::npos) {
            type.erase(pos);
          }
          for (int i = 1; i < itemLength; i++) {
            std::string temp_type = getExpressionType(rhs->expressions[i].get())->toString();
            size_t pos = temp_type.rfind("::");
            if (pos != std::string::npos) {
              temp_type.erase(pos);
            }
            if (temp_type != type) {
              std::cout << "type of expression[0] in array : " << type << std::endl;
              std::cout << "type of expression[" << i << "] in array : " << temp_type << std::endl;
              std::cerr << "type mismatch in array" << std::endl;
              return false;
            }
          }
        } else {
          std::cout << "getting repeat array expression" << std::endl;
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
      }
      std::cout << "declaredLength : " << declaredLength << std::endl;
      std::cout << "itemLength : " << itemLength << std::endl;
      if (declaredLength != -1 && declaredLength != itemLength && itemLength != -1) {
        std::cout << "Array length mismatch: declared length = " << std::to_string(declaredLength) 
                  << ", but initializer has " << itemLength << std::endl;
        return false;
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
    for (int i = 0; i < res.size(); i++) {
      if (auto* lit = dynamic_cast<LiteralExpressionNode*>(res[i])) {
        std::cout << "the " << i << "th element in logic_expr: " << lit->toString() << std::endl;
      } else if (auto* path = dynamic_cast<PathExpressionNode*>(res[i])) {
        std::cout << "the " << i << "th element in logic_expr: " << path->toString() << std::endl;
      }
    }
    return res;
  }

  bool has_negative_in_logic(const ArithmeticOrLogicalExpressionNode* expr) {
    bool res = false;
    auto* expr1 = expr->expression1.get();
    auto* expr2 = expr->expression2.get();
    if (auto* arith_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr1)) {
      res = res && has_negative_in_logic(arith_expr);
    }
    if (auto* neg_expr = dynamic_cast<NegationExpressionNode*>(expr1)) {
      return true;
    }
    if (auto* arith_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr2)) {
      res = res && has_negative_in_logic(arith_expr);
    }
    if (auto* neg_expr = dynamic_cast<NegationExpressionNode*>(expr2)) {
      return true;
    }
    return res;
  }

  std::vector<std::string> get_logic_expr_types(const ArithmeticOrLogicalExpressionNode* expr) {
    std::cout << "getting types in logic_expr" << std::endl;
    std::vector<TypeNode*> res;
    std::vector<ExpressionNode*> ex;
    auto* expr1 = expr->expression1.get();
    auto* expr2 = expr->expression2.get();
    if (auto* arith_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr1)) {
      std::vector<ExpressionNode*> vec1 = get_item_in_logic_expr(arith_expr);
      ex.insert(ex.end(), vec1.begin(), vec1.end());
    } else {
      ex.push_back(expr1);
    }
    if (auto* arith_expr = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr2)) {
      std::vector<ExpressionNode*> vec2 = get_item_in_logic_expr(arith_expr);
      ex.insert(ex.end(), vec2.begin(), vec2.end());
    } else {
      ex.push_back(expr2);
    }
    for (int i = 0; i < ex.size(); i++) {
      if (auto* lit = dynamic_cast<LiteralExpressionNode*>(ex[i])) {
        std::cout << "the " << i << "th element in logic_expr: " << lit->toString() << std::endl;
      } else if (auto* path = dynamic_cast<PathExpressionNode*>(ex[i])) {
        std::cout << "the " << i << "th element in logic_expr: " << path->toString() << std::endl;
      }
    }
    bool has_usize = false;
    for (int i = 0; i < ex.size(); i++) {
      auto* t = getExpressionType(ex[i]);
      if (!t) {
        res.push_back(nullptr);
      } else {
        res.push_back(t);
        if (t->toString() == "usize" || t->toString() == "u32") has_usize = true;
        std::cout << "the " << res.size() - 1 << "th type in logic expr: " << res[res.size() - 1]->toString() << std::endl;
      }
    }
    std::set<int> erase_id;
    for (int i = 0; i < ex.size(); i++) {
      auto* t = getExpressionType(ex[i]);
      if (!t) {
        std::cout << "failing get type of " << i << "th element in logic items" << std::endl;
        throw std::runtime_error("failing getting type of element in arithmetic or logical expression");
      }
      if (t->toString() == "i32") {
        if (auto* lit = dynamic_cast<LiteralExpressionNode*>(ex[i])) {
          if (lit->toString()[0] != '-') {
            if (has_usize) {
              std::cout << "erasing the " << i << "th element in logic_expr types" << std::endl;
              erase_id.insert(i);
            }
          }
        }
      }
    }
    std::cout << "finish getting logic_expr_types" << std::endl;
    std::cout << "size : " << res.size() << std::endl;
    std::vector<std::string> str_res;
    for (int i = 0; i < res.size(); i++) {
      if (erase_id.find(i) == erase_id.end()) str_res.push_back(res[i]->toString());
    }
    return str_res;
  }

  bool check_conditions(const Conditions* condition) {
    if (std::holds_alternative<LetChain>(condition->condition)) {
      return true;
    }
   
    auto& expr_ptr = std::get<std::unique_ptr<ExpressionNode>>(condition->condition);
    std::cout << "checking expression in conditions" << std::endl;
    return check_expression(expr_ptr.get());
  }

  bool has_neg(const ArithmeticOrLogicalExpressionNode* expr) {
    if (auto* inner = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr->expression1.get())) {
      if (has_neg(inner)) return true;
    } else if (auto* inner = dynamic_cast<NegationExpressionNode*>(expr->expression1.get())) {
      return true;
    }
    if (auto* inner = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr->expression2.get())) {
      if (has_neg(inner)) return true;
    } else if (auto* inner = dynamic_cast<NegationExpressionNode*>(expr->expression2.get())) {
      return true;
    }
    return false;
  }

  bool check_expression(const ExpressionNode* expr) {
    if (auto *d = dynamic_cast<const ArithmeticOrLogicalExpressionNode*>(expr)) {
      std::cout << "checking ArithmeticOrLogicalExpressionNode" << std::endl;
      std::vector<std::string> types;
      try {
        types = get_logic_expr_types(d);
      } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return false;
      }
      std::unordered_set<std::string> t;
      for (int i = 0; i < types.size(); i++) {
        std::cout << "the " << i << "th type in logic expression: " << std::endl;
        std::cout << types[i] << std::endl;
        t.insert(types[i]);
      }
      if (t.size() != 1) {
        if (t.size() == 2 && (t.count("usize") || t.count("u32")) && t.count("i32")) {
          if (d->type == OperationType::SHL || d->type == OperationType::SHR) {
            return true;
          }
        }
        std::cout << "type mismatch in Arithmetic or Logical expression" << std::endl;
        return false;
      }
    } else if (auto *lazybool = dynamic_cast<const LazyBooleanExpressionNode*>(expr)) {
      return check_expression(lazybool->expression1.get()) && check_expression(lazybool->expression2.get());
    } else if (auto *ewb = dynamic_cast<const ExpressionWithoutBlockNode*>(expr)) {
      return std::visit([this](auto& node_ptr) -> bool {
        using T = std::decay_t<decltype(node_ptr)>;

        if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
          return check_expression(node_ptr->expr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<OperatorExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
          return check_expression(node_ptr->expression.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<TupleExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<TupleIndexingExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<MethodCallExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<RangeExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<UnderscoreExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
          return check_expression(node_ptr.get());
        }
        else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
          return check_expression(node_ptr->expression.get());
        }
      }, ewb->expr);
    } else if (auto *d = dynamic_cast<const PredicateLoopExpressionNode*>(expr)) {
      std::cout << "checking predicate loop expression" << std::endl;
      if (!d->conditions || !check_conditions(d->conditions.get()) || !d->conditions->check()) {
        std::cerr << "error in conditions" << std::endl;
        return false;
      }
      enterScope();
      currentScope->if_cycle = true;
      bool res = check_BlockExpression_without_changing_scope(d->block_expression.get());
      exitScope();
      return res;
    } else if (auto *d = dynamic_cast<const IfExpressionNode*>(expr)) {
      std::cout << "checking if expression" << std::endl;
      if (!check_conditions(d->conditions.get()) || !d->conditions->check()) {
        std::cerr << "error in conditions" << std::endl;
        return false;
      }
      if (d->block_expression) {
        if (!check_BlockExpression(d->block_expression.get())) return false;
      }
      if (d->else_block) {
        if (!check_BlockExpression(d->else_block.get())) return false;
      }
      if (d->else_if) {
        std::cout << "checking else_if" << std::endl;
        if (!check_expression(d->else_if.get())) return false;
      }
      return true;
    } else if (auto *d = dynamic_cast<const ComparisonExpressionNode*>(expr)) {
      std::cout << "checking ComparisonExpressionNode" << std::endl;
      if (!check_ComparisonExpression(d)) return false;
    } else if (auto *d = dynamic_cast<const LazyBooleanExpressionNode*>(expr)) {
      std::cout << "checking lazy bool expression" << std::endl;
      if (!getExpressionType(d->expression1.get()) || getExpressionType(d->expression2.get())) {
        std::cout << "fail to get type in expression" << std::endl;
        return false;
      }
      if (getExpressionType(d->expression1.get())->toString() != getExpressionType(d->expression2.get())->toString()) {
        std::cout << "type of expression1 : " << getExpressionType(d->expression1.get())->toString() << std::endl;
        std::cout << "type of expression2 : " << getExpressionType(d->expression2.get())->toString() << std::endl;
        return false;
      }
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
      if (!check_expression(d->expression1.get()) || !check_expression(d->expression2.get())) {
        std::cout << "error in expression of assignment expression" << std::endl;
        return false;
      }
      auto* t1 = getExpressionType(d->expression1.get());
      auto* t2 = getExpressionType(d->expression2.get());
      std::string type1 = t1->toString();
      std::string type2 = t2->toString();
      if (auto* ref = dynamic_cast<ReferenceTypeNode*>(t1)) {
        type1 = ref->type->toString();
      }
      if (auto* ref = dynamic_cast<ReferenceTypeNode*>(t2)) {
        type2 = ref->type->toString();
      }
      if (type1 == "i32" && type2 == "usize") type2 = "i32";
      if ((type1 == "usize" || type1 == "u32") && type2 == "i32") {
        std::cout << "checking if the second expression in assignment >0" << std::endl;
        if (auto* lit = dynamic_cast<LiteralExpressionNode*>(d->expression2.get())) {
          if (std::holds_alternative<std::unique_ptr<integer_literal>>(lit->literal)) {
            if (lit->toString()[0] != '-') {
              type2 = type1;
            }
          }
        }
      }
      if (type1 != type2) {
        std::cout << "type mismatch in assignment : " << std::endl;
        std::cout << "type1 : " << type1 << std::endl;
        std::cout << "type2 : " << type2 << std::endl;
        return false;
      }
      if (auto *expr1 = dynamic_cast<const IndexExpressionNode*>(d->expression1.get())) {
        std::cout << "the first expression of assignment is index expression" << std::endl;
        if (getExpressionType(expr1->index.get())->toString() == "bool") {
          std::cout << "index cannot be a boolean type" << std::endl;
          return false;
         }
        //检查ArrayPattern是否mutable
        if (auto *path_expression = dynamic_cast<PathExpressionNode*>(expr1->base.get())) {
          std::cout << "the base of index expression is pathexpression" << std::endl;
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
        } else if (auto *method_call = dynamic_cast<MethodCallExpressionNode*>(expr1->base.get())) {
          std::cout << "the base of index expression is method call expression" << std::endl;
          return true;
        } else {
          std::cout << "unknown type of expression of base of indexexpression : " << typeid(expr1->base.get()).name() << std::endl;
          return true;
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
          return false;
        }
        bool var_if_mut = symbol->isMutable;
        if (var == "self" || var == "Self") var_if_mut = true;
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
      std::cout << "finish checking methodcall expression" << std::endl;
    } else if (auto *inf = dynamic_cast<const InfiniteLoopExpressionNode*>(expr)) {
      std::cout << "checking infinite loop expression" << std::endl;
      enterScope();
      currentScope->if_cycle = true;
      bool res = check_BlockExpression_without_changing_scope(inf->block_expression.get());
      exitScope();
      std::cout << "finish checking infinite loop expression" << std::endl;
      return res;
    } else if (auto * break_expr = dynamic_cast<const BreakExpressionNode*>(expr)) {
      if (!currentScope->if_cycle == true) {
        std::cout << "break not in loop" << std::endl;
        return false;
      }
      return true;
    } else if (auto* block = dynamic_cast<const BlockExpressionNode*>(expr)) {
      std::cout << "checking block expression" << std::endl;
      enterScope();
      bool res = check_BlockExpression_without_changing_scope(block);
      exitScope();
      std::cout << "finish checking block expression" << std::endl;
      return res;
    } else if (auto* block = dynamic_cast<const GroupedExpressionNode*>(expr)) {
      std::cout << "checking grouped expression" << std::endl;
      bool res = check_expression(block->expression.get());
      return res;
    } else if (auto* return_expr = dynamic_cast<const ReturnExpressionNode*>(expr)) {
      std::cout << "checking return expression" << std::endl;
      bool res = check_expression(return_expr->expression.get());
      return res;
    } else if (auto* index_expr = dynamic_cast<const IndexExpressionNode*>(expr)) {
      std::cout << "checking index expression" << std::endl;
      std::string type = getExpressionType(index_expr->index.get())->toString();
      if (type == "bool" || type == "str" || type == "&str") {
        std::cout << "invalid type of index in index expression : " << type << std::endl;
        return false;
      }
      return true;
    } else if (auto* struct_expr = dynamic_cast<const StructExpressionNode*>(expr)) {
      std::cout << "checking struct expression" << std::endl;
      std::string struct_name = struct_expr->pathin_expression->toString();
      std::cout << "name of the struct : " << struct_name << std::endl;
      auto* struct_info = currentScope->lookupStruct(struct_name);
      int declared_item_num = struct_info->fields.size();
      int actual_item_num = struct_expr->struct_expr_fields->struct_expr_fields.size();
      std::cout << "declared item num : " << declared_item_num << std::endl;
      std::cout << "actual item num : " << actual_item_num << std::endl;
      if (declared_item_num != actual_item_num) {
        return false;
      }
      for (int i = 0; i < declared_item_num; i++) {
        auto* type1 = struct_info->fields[i].type;
        auto* type2 = getExpressionType(struct_expr->struct_expr_fields->struct_expr_fields[i]->expression.get());
        if (!is_type_equal(type1, type2)) {
          std::cout << "type of def : " << type1->toString() << std::endl;
          std::cout << "type of declare : " << type2->toString() << std::endl;
          if (!(type1->toString() == "usize" && type2->toString() == "i32")) {
            std::cout << "type mismatch in struct expression with id: " << i << std::endl;
            return false;
          } else {
            auto* lit = dynamic_cast<LiteralExpressionNode*>(struct_expr->struct_expr_fields->struct_expr_fields[i]->expression.get());
            if (lit->toString()[0] == '-') {//声明usize但是定义是负数
              std::cout << "type mismatch in struct expression with id: " << i << std::endl;
              return false;
            }
          }
        }
      }
      std::cout << "finish checking struct expression" << std::endl;
      return true;
    } else if (auto* call_expr = dynamic_cast<const CallExpressionNode*>(expr)) {
      if (auto* temp = dynamic_cast<PathExpressionNode*>(call_expr->expression.get())) {
        std::string func_name = dynamic_cast<PathExpressionNode*>(call_expr->expression.get())->toString();
        if (func_name == "printInt" || func_name == "exit" || func_name == "println" || func_name == "printlnInt") {
          if (func_name == "exit") {
            if (!call_expr->call_params || call_expr->call_params->expressions.size() != 1) {
              return false;
            }
            std::string t = getExpressionType(call_expr->call_params->expressions[0].get())->toString();
            std::cout << "type of param in exit expression : " << t << std::endl;
            if (t != "i32") {
              std::cerr << "param except i32 not allowed in 'exit' function" << std::endl;
              return false;
            }
          }
          std::cout << "finish checking expression" << std::endl;
          return true;
        }
        size_t pos = func_name.rfind("::");
        std::string possible_self;
        std::string function;
        if (pos != std::string::npos) {
          possible_self = func_name.substr(0, pos);
          function = func_name.substr(pos + 2);
        } 
        if (possible_self == "Self" || possible_self == "self") {
          possible_self = currentScope->possible_self;
          func_name = possible_self + "::" + function;
          std::cout << "function to find: " << func_name << std::endl;
        }
        if (!currentScope->is_forward_declared(func_name)) {
          std::cout << "function: " << func_name << " not found" << std::endl;
          return false;
        }
        if (call_expr->call_params) {
          for (int i = 0; i < call_expr->call_params->expressions.size(); i++) {
            if (auto* lit_expr = dynamic_cast<LiteralExpressionNode*>(call_expr->call_params->expressions[i].get())) {
              if (std::holds_alternative<std::unique_ptr<integer_literal>>(lit_expr->literal)) {
                std::string num = lit_expr->toString();
                std::cout << "integer literal in function param: " << num << std::endl;
                if (isOverflow(num)) {
                  std::cout << "overflow of integer_literal in call expression" << std::endl;
                  return false;
                }
              }
            }
          }
          auto* func_param = currentScope->find_func_param(func_name);
          if (!func_param) {
            std::cout << "function : " << func_name << " not found" << std::endl;
          }
          std::cout << "size of function param: " << func_param->function_params.size() << std::endl;
          for (int i = 0; i < call_expr->call_params->expressions.size(); i++) {
            std::string type1 = getExpressionType(call_expr->call_params->expressions[i].get())->toString();
            std::cout << "type of call param : " << type1 << std::endl;
            std::string type2 = getFunctionParamTypeString(func_param->function_params[i].get());
            std::cout << "type of function param : " << type2 << std::endl;
            while (!type1.empty() && type1.front() == '&') type1.erase(type1.begin());
            if (type1.rfind("mut", 0) == 0) type1.erase(0, 3);
            while (!type2.empty() && type2.front() == '&') type2.erase(type2.begin());
            if (type2.rfind("mut", 0) == 0) type2.erase(0, 3);
            if (type1 == "i32" && (type2 == "usize" || type2 == "u32")) {
              if (auto* lit = dynamic_cast<LiteralExpressionNode*>(call_expr->call_params->expressions[i].get())) {
                if (lit->toString()[0] != '-') {
                  type1 = type2;
                } 
              } else if (auto* expr_node = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(call_expr->call_params->expressions[i].get())) {
                if (!has_neg(expr_node)) {
                  type1 = type2;
                }
              }
            }
            if (type1 == "u32" || type1 == "usize") {
              if (type2 == "i32") return true;
            }
            if (type1 != type2) {
              std::cout << "type mismatch in call expression" << std::endl;
              return false;
            }
          }
        }
      }
    } else if (auto* path_expr = dynamic_cast<const PathExpressionNode*>(expr)) {
      std::string path = path_expr->toString();
      if (!currentScope->lookupVar(path)) {
        std::cout << "var: " << path << " not found" << std::endl;
        return false;
      }
    }
    std::cout << "finish checking expression" << std::endl;
    return true;
  }

  bool forward_declare(const ItemNode* expr) {
    //插入getInt()
    //printInt()是一个单独的expression，这里不加没关系
    
    if (auto* func = dynamic_cast<const FunctionNode*>(expr)) {
      if (currentScope->get_function_type(func->identifier) != nullptr) {
        std::cout << "redefinition of function : " << func->identifier << std::endl;
        return false;
      }
      currentScope->symbol_table.functions.insert({func->identifier, func->function_parameter.get()});
      if (func->return_type) currentScope->symbol_table.function_types.insert({func->identifier, func->return_type->type.get()});
      std::cout << "inserting function : " << func->identifier << std::endl;
    } else if (auto* structstruct = dynamic_cast<const StructStructNode*>(expr)) {
      if (!check_Item(expr)) return false;
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
          if (node->return_type) {
            std::cout << "inserting function return type of function: " << func << std::endl;
            currentScope->symbol_table.function_types[func] = node->return_type->type.get();
          }
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
    return true;
  }
 
  bool check() {
    std::string func_name = "getInt";
    TypeNode* return_type = new TypePathNode("i32");
    std::vector<std::unique_ptr<FunctionParam>> fp;
    auto* parameter = new FunctionParameter(2, std::move(fp));
    auto func_info = FunctionSymbol{func_name, parameter, return_type, std::nullopt};
    currentScope->insertFunc(func_name, func_info);
    currentScope->symbol_table.function_types.insert({func_name, return_type});
    currentScope->symbol_table.functions.insert({func_name, parameter});
    func_name = "i32::to_string";
    return_type = new TypePathNode("String");
    std::vector<std::unique_ptr<FunctionParam>> fp2;
    parameter = new FunctionParameter(2, std::move(fp2));
    func_info = FunctionSymbol{func_name, parameter, return_type, std::nullopt};
    currentScope->insertFunc(func_name, func_info);
    currentScope->symbol_table.function_types.insert({func_name, return_type});
    currentScope->symbol_table.functions.insert({func_name, parameter});
    for (int i = 0; i < ast.size(); i++) {
      if (auto item = dynamic_cast<ItemNode*>(ast[i].get())) {
        std::cout << "forward declaring item which is the " << i << "th node in ast" << std::endl;
        if (!forward_declare(item)) {
          std::cerr << "error in forward declaring" << std::endl;
          return false;
        };
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
        if (auto *structstruct = dynamic_cast<StructStructNode*>(d)) continue;
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