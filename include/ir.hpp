#ifndef IR_HPP
#define IR_HPP

#include <string>
#include <sstream>
#include <memory>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <cassert>
#include <cctype>
#include "parser.hpp"

// LLVM IR 对全局符号名的“裸标识符”限制比较严格；包含 ':' 等字符时需要使用带引号的形式：@"Foo::bar"。
static bool isValidLLVMGlobalBareIdent(const std::string& s) {
  if (s.empty()) return false;
  auto isFirst = [](unsigned char c) {
    return std::isalpha(c) || c == '_' || c == '.' || c == '$';
  };
  auto isRest = [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '.' || c == '$';
  };
  if (!isFirst(static_cast<unsigned char>(s[0]))) return false;
  for (size_t i = 1; i < s.size(); ++i) {
    if (!isRest(static_cast<unsigned char>(s[i]))) return false;
  }
  return true;
}

static std::string llvmGlobalRef(const std::string& name) {
  if (isValidLLVMGlobalBareIdent(name)) return "@" + name;
  return "@\"" + name + "\"";
}

static std::string stripTrailingStars(std::string t) {
  while (!t.empty() && t.back() == '*') t.pop_back();
  return t;
}

static std::string mangleFuncName(std::string name) {
  size_t pos = 0;
  while ((pos = name.find("::", pos)) != std::string::npos) {
    name.replace(pos, 2, "_");
  }
  return name;
}

// 用于 impl 类型名前缀：尽量从 parser.hpp 的 TypeNode::toString() 里拿到“干净”的类型名。
// 目前只处理最常见的 &T / &mutT 形式。
static std::string sanitizeImplTypePrefix(std::string t) {
  if (!t.empty() && t.front() == '&') {
    t.erase(t.begin());
    if (t.rfind("mut", 0) == 0) {
      t.erase(0, 3);
    }
  }
  return t;
}

class IRGenerator {
  public:
   IRGenerator();
   ~IRGenerator();

   std::string generate(const std::vector<std::unique_ptr<ASTNode>>& ast);
   std::string getCurrentIR();

   private:
   std::stringstream irStream;
   int tempCounter;
   int labelCounter;
   std::unordered_map<std::string, std::string> symbolTable; // 保留全局
   std::unordered_map<std::string, std::string> functionTable;
   std::unordered_map<std::string, std::vector<std::string>> paramTypesTable; // function name -> list of param types
   std::unordered_map<std::string, std::string> constantTable;
   std::unordered_map<std::string, std::string> typeTable; // struct name -> LLVM type string
   std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> structFields; // struct name -> list of (field_name, type)
   std::unordered_map<std::string, std::string> varTypes; // 保留全局

   // Scope management
   std::vector<std::unordered_map<std::string, std::string>> symbolScopes;
   std::vector<std::unordered_map<std::string, std::string>> varTypeScopes;
   std::vector<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> fieldScopes;
   std::vector<std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> fieldTypeScopes;
   std::vector<std::unordered_map<std::string, bool>> isLetDefinedScopes;
   std::vector<std::unordered_map<std::string, std::string>> typeNameScopes;

   // Pre-allocated addresses for let statements in loops
   std::unordered_map<std::string, std::string> loopPreAlloc;
   std::string currentRetType;
   bool inFunctionBody = false;
   std::string returnVar;
   std::string returnLabel;
   std::string currentLoopLabel;
   std::string currentBreakLabel;

  void preScan(const std::vector<std::unique_ptr<ASTNode>>& ast);
  void generateStructTypes();

  std::string toIRType(TypeNode* type);
  std::string toIRType(const std::string& typeName);
  std::string expandStructType(const std::string& typeName);
  std::vector<std::string> expandParamTypes(const std::string& paramType);

  std::string visit(ExpressionNode* node);
  std::string visit(LiteralExpressionNode* node);
  std::string visit(PathExpressionNode* node);
  std::string visit(CallExpressionNode* node);
  std::string visit(MethodCallExpressionNode* node);
  std::string visit(IndexExpressionNode* node);
  std::string visit(ArrayExpressionNode* node);
  std::string visit(ArithmeticOrLogicalExpressionNode* node);
  std::string visit(ComparisonExpressionNode* node);
  std::string visit(LazyBooleanExpressionNode* node);
  std::string visit(TypeCastExpressionNode* node);
  std::string visit(AssignmentExpressionNode* node);
  std::string visit(CompoundAssignmentExpressionNode* node);
  std::string visit(StructExpressionNode* node);
  std::string visit(DereferenceExpressionNode* node);
  std::string visit(NegationExpressionNode* node);
  std::string visit(BorrowExpressionNode* node);
  std::string visit(BlockExpressionNode* node);
  std::string visit(IfExpressionNode* node);
  std::string visit(PredicateLoopExpressionNode* node);
  std::string visit(ReturnExpressionNode* node);
  std::string visit(ContinueExpressionNode* node);
  std::string visit(BreakExpressionNode* node);
  std::string visit(FieldExpressionNode* node);
  std::string visit(GroupedExpressionNode* node);
  std::string visit(ExpressionWithoutBlockNode* node);
  std::string visit(OperatorExpressionNode* node);

  std::string visit_in_rhs(ExpressionNode* node);
  std::string visit_in_rhs(IfExpressionNode* node);
  std::string visit_in_rhs(ArithmeticOrLogicalExpressionNode* node);
  std::string visit_in_rhs(GroupedExpressionNode* node);

  void visit(ItemNode* node);
  void visit(FunctionNode* node);
  void visit(StructStructNode* node);
  void visit(TupleStructNode* node);
  void visit(EnumerationNode* node);
  void visit(ConstantItemNode* node);
  void visit(InherentImplNode* node);
  void visit(TraitImplNode* node);

  void visit(StatementNode* node);
  void visit(LetStatement* node);
  void visit(ExpressionStatement* node);

  std::string createTemp();
  std::string createLabel();
  std::string emitAlloca(const std::string& type, const std::string& name = "");
  std::string emitLoad(const std::string& type, const std::string& ptr);
  std::string emitStore(const std::string& value, const std::string& ptr);
  std::string emitBinaryOp(const std::string& op, const std::string& lhs, const std::string& rhs, const std::string& type);

  std::string getLhsAddress(ExpressionNode* lhs);
  std::string getLhsType(ExpressionNode* lhs);
  std::string getLhsTypeWithStar(ExpressionNode* lhs);

  bool isLetDefined(const std::string& name);

  std::optional<int> evaluateConstant(ExpressionNode* expr);
  std::string getElementType(const std::string& typeStr);

  bool hasReturn(BlockExpressionNode* block);
  bool hasReturnInExpression(ExpressionNode* expr);
  bool willReturn(BlockExpressionNode* block);
  bool willReturnInExpression(ExpressionNode* expr);

  void error(const std::string& msg);

  void enterScope();
  void exitScope();
  std::string lookupSymbol(const std::string& name);
  std::string lookupVarType(const std::string& name);
  std::string getTypeName(const std::string& name);
};

IRGenerator::IRGenerator() : tempCounter(0), labelCounter(0) {
  irStream << "; ModuleID = 'generated.ll'\n";
  irStream << "source_filename = \"generated.ll\"\n";
  irStream << "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128\"\n";
  irStream << "target triple = \"x86_64-pc-linux-gnu\"\n\n";
  // Initialize global scope
  symbolScopes.push_back({});
  varTypeScopes.push_back({});
  fieldScopes.push_back({});
  fieldTypeScopes.push_back({});
  isLetDefinedScopes.push_back({});
  typeNameScopes.push_back({});
}

IRGenerator::~IRGenerator() = default;

std::string IRGenerator::generate(const std::vector<std::unique_ptr<ASTNode>>& ast) {
  // 第一步：预扫描全局声明
  preScan(ast);

  // 添加内置函数信息到table
  functionTable["print"] = "void";
  paramTypesTable["print"] = {"i8*"};
  functionTable["println"] = "void";
  paramTypesTable["println"] = {"i8*"};
  functionTable["printInt"] = "void";
  paramTypesTable["printInt"] = {"i32"};
  functionTable["printlnInt"] = "void";
  paramTypesTable["printlnInt"] = {"i32"};
  functionTable["getString"] = "i8*";
  paramTypesTable["getString"] = {};
  functionTable["getInt"] = "i32";
  paramTypesTable["getInt"] = {};
  functionTable["builtin_memset"] = "i8*";
  paramTypesTable["builtin_memset"] = {"i8*", "i32", "i32"};
  functionTable["builtin_memcpy"] = "i8*";
  paramTypesTable["builtin_memcpy"] = {"i8*", "i8*", "i32"};
  functionTable["exit"] = "void";
  paramTypesTable["exit"] = {"i32"};

  //irStream << "; begin generating IR of struct\n";
  // 第二步：先生成所有 struct 的 IR
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (dynamic_cast<StructStructNode*>(item) || dynamic_cast<TupleStructNode*>(item)) {
        visit(item);
      }
    }
  }

  //irStream << "; finish generating IR of struct\n";

  // 添加内置函数定义
  irStream << "@.str = private unnamed_addr constant [3 x i8] c\"%s\\00\", align 1\n";
  irStream << "@.str.1 = private unnamed_addr constant [4 x i8] c\"%s\\0A\\00\", align 1\n";
  irStream << "@.str.2 = private unnamed_addr constant [3 x i8] c\"%d\\00\", align 1\n";
  irStream << "@.str.3 = private unnamed_addr constant [4 x i8] c\"%d\\0A\\00\", align 1\n";
  irStream << "\n";
  irStream << "define dso_local void @print(i8* noundef %0) {\n";
  irStream << "  %2 = alloca i8*, align 8\n";
  irStream << "  store i8* %0, i8** %2, align 8\n";
  irStream << "  %3 = load i8*, i8** %2, align 8\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i8* noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local void @println(i8* noundef %0) {\n";
  irStream << "  %2 = alloca i8*, align 8\n";
  irStream << "  store i8* %0, i8** %2, align 8\n";
  irStream << "  %3 = load i8*, i8** %2, align 8\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0), i8* noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local void @printInt(i32 noundef %0) {\n";
  irStream << "  %2 = alloca i32, align 4\n";
  irStream << "  store i32 %0, i32* %2, align 4\n";
  irStream << "  %3 = load i32, i32* %2, align 4\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i64 0, i64 0), i32 noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local void @printlnInt(i32 noundef %0) {\n";
  irStream << "  %2 = alloca i32, align 4\n";
  irStream << "  store i32 %0, i32* %2, align 4\n";
  irStream << "  %3 = load i32, i32* %2, align 4\n";
  irStream << "  %4 = call i32 (i8*, ...) @printf(i8* noundef getelementptr inbounds ([4 x i8], [4 x i8]* @.str.3, i64 0, i64 0), i32 noundef %3)\n";
  irStream << "  ret void\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i8* @getString() {\n";
  irStream << "  %1 = alloca i8*, align 8\n";
  irStream << "  %2 = call i8* @malloc(i32 noundef 256)\n";
  irStream << "  store i8* %2, i8** %1, align 8\n";
  irStream << "  %3 = load i8*, i8** %1, align 8\n";
  irStream << "  %4 = call i32 (i8*, ...) @scanf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str, i64 0, i64 0), i8* noundef %3)\n";
  irStream << "  %5 = load i8*, i8** %1, align 8\n";
  irStream << "  ret i8* %5\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i32 @getInt() {\n";
  irStream << "  %1 = alloca i32, align 4\n";
  irStream << "  %2 = call i32 (i8*, ...) @scanf(i8* noundef getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i64 0, i64 0), i32* noundef %1)\n";
  irStream << "  %3 = load i32, i32* %1, align 4\n";
  irStream << "  ret i32 %3\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i8* @builtin_memset(i8* noundef %0, i32 noundef %1, i32 noundef %2) {\n";
  irStream << "  %4 = alloca i8*, align 8\n";
  irStream << "  %5 = alloca i32, align 4\n";
  irStream << "  %6 = alloca i32, align 4\n";
  irStream << "  store i8* %0, i8** %4, align 8\n";
  irStream << "  store i32 %1, i32* %5, align 4\n";
  irStream << "  store i32 %2, i32* %6, align 4\n";
  irStream << "  %7 = load i8*, i8** %4, align 8\n";
  irStream << "  %8 = load i32, i32* %5, align 4\n";
  irStream << "  %9 = load i32, i32* %6, align 4\n";
  irStream << "  %10 = call i8* @memset(i8* noundef %7, i32 noundef %8, i32 noundef %9)\n";
  irStream << "  ret i8* %10\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "define dso_local i8* @builtin_memcpy(i8* noundef %0, i8* noundef %1, i32 noundef %2) {\n";
  irStream << "  %4 = alloca i8*, align 8\n";
  irStream << "  %5 = alloca i8*, align 8\n";
  irStream << "  %6 = alloca i32, align 4\n";
  irStream << "  store i8* %0, i8** %4, align 8\n";
  irStream << "  store i8* %1, i8** %5, align 8\n";
  irStream << "  store i32 %2, i32* %6, align 4\n";
  irStream << "  %7 = load i8*, i8** %4, align 8\n";
  irStream << "  %8 = load i8*, i8** %5, align 8\n";
  irStream << "  %9 = load i32, i32* %6, align 4\n";
  irStream << "  %10 = call i8* @memcpy(i8* noundef %7, i8* noundef %8, i32 noundef %9)\n";
  irStream << "  ret i8* %10\n";
  irStream << "}\n";
  irStream << "\n";
  irStream << "declare i32 @scanf(i8*, ...)\n";
  irStream << "declare i32 @printf(i8*, ...)\n";
  irStream << "declare i8* @malloc(i32 noundef)\n";
  irStream << "declare i8* @memset(i8* noundef, i32 noundef, i32 noundef)\n";
  irStream << "declare i8* @memcpy(i8* noundef, i8* noundef, i32 noundef)\n";
  irStream << "declare void @exit(i32 noundef)\n\n";

  // 第三步：生成其他代码（函数等）
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (!dynamic_cast<StructStructNode*>(item) && !dynamic_cast<TupleStructNode*>(item) && !dynamic_cast<ConstantItemNode*>(item)) {
        visit(item);
      }
    } else if (auto* stmt = dynamic_cast<StatementNode*>(node.get())) {
      visit(stmt);
    } else if (auto* expr = dynamic_cast<ExpressionNode*>(node.get())) {
      visit(expr);
    } else {
      error("Unknown AST node type");
    }
  }
  return irStream.str();
}

std::string IRGenerator::toIRType(TypeNode* type) {
  //irStream << "; getting irtype of typenode: " << type->toString() << '\n';
  if (!type) return "void";

  switch (type->node_type) {
    case TypeType::TypePath_node: {
      auto* typePathNode = dynamic_cast<TypePathNode*>(type);
      if (typePathNode && typePathNode->type_path) {
        std::string path = typePathNode->type_path->toString();
        if (typeTable.find(path) != typeTable.end()) {
          return typeTable[path];
        }
        if (path == "i32") return "i32";
        if (path == "i64") return "i64";
        if (path == "u32") return "i32";
        if (path == "bool") return "i1";
      }
      break;
    }
    case TypeType::ReferenceType_node: {
      //irStream << "; the type is reference type\n";
      auto* refType = dynamic_cast<ReferenceTypeNode*>(type);
      if (refType) {
        return "ptr";
      }
      break;
    }
    case TypeType::ArrayType_node: {
      //irStream << "; the type is array type\n";
      auto* arrayType = dynamic_cast<ArrayTypeNode*>(type);
      if (arrayType) {
          std::string innerType = toIRType(arrayType->type.get());
          //irStream << "; the inner type of array type: " << innerType << '\n';
          auto numOpt = evaluateConstant(arrayType->expression.get());
          if (numOpt) {
            return "[" + std::to_string(*numOpt) + " x " + innerType + "]";
          } else {
            error("Array size must be constant expression");
          }
      }
      break;
    }
  }
  return "i32";
}

std::string IRGenerator::toIRType(const std::string& typeName) {
  if (typeName == "i32") return "i32";
  if (typeName == "i64") return "i64";
  if (typeName == "bool") return "i1";
  return "i32";
}

std::string IRGenerator::expandStructType(const std::string& typeName) {
  if (typeName.back() == '*') {
    // Pointer type, expand the pointed type and add *
    std::string pointed = typeName.substr(0, typeName.size() - 1);
    std::string expanded = expandStructType(pointed);
    return expanded + "*";
  } else if (typeName[0] == '[') {
    // Array type [N x T]
    size_t xPos = typeName.find(" x ");
    if (xPos != std::string::npos) {
      std::string nStr = typeName.substr(1, xPos - 1);
      std::string tStr = typeName.substr(xPos + 3, typeName.size() - xPos - 4); // remove ]
      std::string expandedT = expandStructType(tStr); // recursive
      std::string res = "[" + nStr + " x " + expandedT + "]";
      return res;
    }
  } else if (typeName[0] == '%') {
    // Struct type
    std::string name = typeName.substr(1);
    auto it = structFields.find(name);
    if (it != structFields.end()) {
      std::string res = "{";
      for (size_t i = 0; i < it->second.size(); ++i) {
        std::string fieldType = it->second[i].second;
        fieldType = expandStructType(fieldType); // recursive
        res += fieldType;
        if (i + 1 < it->second.size()) res += ", ";
      }
      res += "}";
      return res;
    }
  }
  return typeName;
}

std::vector<std::string> IRGenerator::expandParamTypes(const std::string& paramType) {
  if (paramType[0] == '%') {
    std::string name = paramType.substr(1);
    auto it = structFields.find(name);
    if (it != structFields.end()) {
      std::vector<std::string> res;
      for (auto& p : it->second) {
        res.push_back(p.second);
      }
      return res;
    }
  }
  return {paramType};
}

std::string IRGenerator::visit(ExpressionNode* node) {
  if (auto* lit = dynamic_cast<LiteralExpressionNode*>(node)) {
    return visit(lit);
  } else if (auto* path = dynamic_cast<PathExpressionNode*>(node)) {
    return visit(path);
  } else if (auto* call = dynamic_cast<CallExpressionNode*>(node)) {
    return visit(call);
  } else if (auto* arith = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(node)) {
    return visit(arith);
  } else if (auto* comp = dynamic_cast<ComparisonExpressionNode*>(node)) {
    return visit(comp);
  } else if (auto* assign = dynamic_cast<AssignmentExpressionNode*>(node)) {
    return visit(assign);
  } else if (auto* compAssign = dynamic_cast<CompoundAssignmentExpressionNode*>(node)) {
    return visit(compAssign);
  } else if (auto* struct_ = dynamic_cast<StructExpressionNode*>(node)) {
    return visit(struct_);
  } else if (auto* deref = dynamic_cast<DereferenceExpressionNode*>(node)) {
    return visit(deref);
  } else if (auto* block = dynamic_cast<BlockExpressionNode*>(node)) {
    return visit(block);
  } else if (auto* if_ = dynamic_cast<IfExpressionNode*>(node)) {
    return visit(if_);
  } else if (auto* ret = dynamic_cast<ReturnExpressionNode*>(node)) {
    return visit(ret);
  } else if (auto* group = dynamic_cast<GroupedExpressionNode*>(node)) {
    return visit(group);
  } else if (auto* field = dynamic_cast<FieldExpressionNode*>(node)) {
    return visit(field);
  } else if (auto* lazy_bool = dynamic_cast<LazyBooleanExpressionNode*>(node)) {
    return visit(lazy_bool);
  } else if (auto* method_call = dynamic_cast<MethodCallExpressionNode*>(node)) {
    return visit(method_call);
  } else if (auto* index = dynamic_cast<IndexExpressionNode*>(node)) {
    return visit(index);
  } else if (auto* loop = dynamic_cast<PredicateLoopExpressionNode*>(node)) {
    return visit(loop);
  } else if (auto* cont = dynamic_cast<ContinueExpressionNode*>(node)) {
    return visit(cont);
  } else if (auto* break_ = dynamic_cast<BreakExpressionNode*>(node)) {
    return visit(break_);
  } else if (auto* opExpr = dynamic_cast<OperatorExpressionNode*>(node)) {
    return visit(opExpr);
  } else if (auto* type_cast = dynamic_cast<TypeCastExpressionNode*>(node)) {
    return visit(type_cast);
  } else if (auto* array = dynamic_cast<ArrayExpressionNode*>(node)) {
    return visit(array);
  } else if (auto* borrow = dynamic_cast<BorrowExpressionNode*>(node)) {
    return visit(borrow);
  } else if (auto* neg = dynamic_cast<NegationExpressionNode*>(node)) {
    return visit(neg);
  }
  error(std::string("Unsupported expression type: ") + typeid(*node).name());
  return "";
}

std::string IRGenerator::visit(LiteralExpressionNode* node) {
  if (std::holds_alternative<std::unique_ptr<string_literal>>(node->literal)) {
    auto& strLit = std::get<std::unique_ptr<string_literal>>(node->literal);
    std::string strValue = strLit->value;
    // 转义字符串，添加 null terminator
    std::string escaped = strValue + "\\00";
    int len = strValue.size() + 1;
    std::string globalName = ".str." + std::to_string(tempCounter++);
    irStream << "@" << globalName << " = private unnamed_addr constant [" << len << " x i8] c\"" << escaped << "\", align 1\n";
    std::string temp = createTemp();
    irStream << "  %" << temp << " = getelementptr inbounds [" << len << " x i8], [" << len << " x i8]* @" << globalName << ", i32 0, i32 0\n";
    return "%" + temp;
  } else if (std::holds_alternative<std::unique_ptr<bool>>(node->literal)) {
    auto& boolLit = std::get<std::unique_ptr<bool>>(node->literal);
    std::string temp = createTemp();
    std::string value = *boolLit ? "1" : "0";
    irStream << "  %" << temp << " = add i1 0, " << value << "\n";
    return "%" + temp;
  } else {
    // 整数或其他
    std::string temp = createTemp();
    std::string type = "i32";
    std::string value = node->toString();
    if (std::holds_alternative<std::unique_ptr<integer_literal>>(node->literal)) {
      //irStream << "; visiting integer literal: " << value << '\n';
      value = std::get<std::unique_ptr<integer_literal>>(node->literal)->value;
      try {
        long long val = std::stoll(value);
        if (val > 2147483647LL || val < -2147483648LL) {
          type = "i64";
        }
      } catch (...) {
        // keep i32
      }
    }
    irStream << "  %" << temp << " = add " << type << " 0, " << value << "\n";
    return "%" + temp;
  }
}

std::string IRGenerator::visit(PathExpressionNode* node) {
    std::string name = node->toString();
    irStream << "; visiting pathexpression with path: " << name << '\n';
    if (constantTable.find(name) != constantTable.end()) {
      // 常量，直接返回值
      return constantTable[name];
    } else {
      std::string temp = lookupSymbol(name);
      if (!temp.empty()) {
        std::string type = lookupVarType(name);
        //irStream << "; type of " << name << " : " << type << '\n';
        if (type.empty()) type = "i32";
        if (temp == name) {
          // 直接是 register
          return "%" + temp;
        } else {
          // temp is a stored pointer, load value
          if (type.find('%') == 0 && type.back() == '*') {
            // 结构体指针，load 指针
            return "%" + temp;
          } else if (type.find('*') != std::string::npos) {
            // 其他指针，直接返回指针
            return "%" + temp;
          } else if (type[0] == '%') {
            // struct value, temp is value register, return directly
            return "%" + temp;
          } else if (type[0] == '{') {
            // struct value expanded, load
            std::string loadTemp = createTemp();
            irStream << "  %" << loadTemp << " = load " << expandStructType(type) << ", " << expandStructType(type) << "* %" << temp << "\n";
            return "%" + loadTemp;
          } else if (type[0] == '[' && type.back() == '*') {
            // array pointer, return pointer
            return "%" + temp;
          } else {
            // 基本类型，load
            std::string loadTemp = createTemp();
            irStream << "  %" << loadTemp << " = load " << expandStructType(type) << ", " << expandStructType(type) << "* %" << temp << "\n";
            return "%" + loadTemp;
          }
        }
      }
    }
    error("Undefined variable: " + name);
    return "";
  }

std::string IRGenerator::visit(CallExpressionNode* node) {
    std::string funcName;
    if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression.get())) {
        funcName = path->toString();
    } else {
        error("Unsupported function expression in call");
        return "";
    }
    funcName = mangleFuncName(funcName);
    // Special handling for exit
    if (funcName == "exit") {
        std::string arg = visit(node->call_params->expressions[0].get());
        irStream << "  ret i32 " << arg << "\n";
        return "";
    }
    std::string args = "";
    if (node->call_params) {
        auto it = paramTypesTable.find(funcName);
        if (it != paramTypesTable.end()) {
            const auto& paramTypes = it->second;
            size_t argIndex = 0;
            for (size_t i = 0; i < node->call_params->expressions.size(); ++i) {
                std::string argValue = visit_in_rhs(node->call_params->expressions[i].get());
                std::string argType = (i < paramTypes.size()) ? paramTypes[i] : "i32";
                // 参数类型检查：如果实参类型比声明类型多一层指针，则需要 load 之后再传入
                // 这里的 actualType 以 getLhsType/lookupVarType 的结果为准（此项目中局部变量通常记录为 T*）
                std::string actualType;
                if (auto* pathArg = dynamic_cast<PathExpressionNode*>(node->call_params->expressions[i].get())) {
                  actualType = lookupVarType(pathArg->toString());
                  //irStream << "; actual type is " << actualType << " after looking up var type\n";
                } else {
                  actualType = getLhsType(node->call_params->expressions[i].get());
                  //irStream << "; actual type is " << actualType << " after getting lhs type\n";
                }
                //irStream << "; argvalue in call expression of " << funcName << " is " << argValue << " with argtype " << argType << "\n";
                if (!actualType.empty() && actualType == argType + "*") {
                  if (auto* borrow = dynamic_cast<BorrowExpressionNode*>(node->call_params->expressions[i].get())) {
                    if (auto* path = dynamic_cast<PathExpressionNode*>(borrow->expression.get())) {
                      if (!isLetDefined(path->toString())) {
                        std::string param_temp = createTemp();
                        irStream << "  %" << param_temp << " = load " << expandStructType(argType)
                                 << ", " << expandStructType(actualType) << " " << argValue << "\n";
                        argValue = "%" + param_temp;
                      }
                    }
                  } else {
                    std::string param_temp = createTemp();
                    irStream << "  %" << param_temp << " = load " << expandStructType(argType)
                             << ", " << expandStructType(actualType) << " " << argValue << "\n";
                    argValue = "%" + param_temp;
                  }
                }
                if (argType[0] == '%' && argType.back() != '*') {
                    std::string name = argType.substr(1);
                    auto it2 = structFields.find(name);
                    if (it2 != structFields.end()) {
                        for (size_t j = 0; j < it2->second.size(); ++j) {
                            if (argIndex > 0) args += ", ";
                            std::string temp = createTemp();
                            irStream << "  %" << temp << " = extractvalue " << expandStructType(argType) << " " << argValue << ", " << j << "\n";
                            args += it2->second[j].second + " %" + temp;
                            argIndex++;
                        }
                    } else {
                        if (argIndex > 0) args += ", ";
                        args += expandStructType(argType) + " " + argValue;
                        argIndex++;
                    }
                } else {
                    if (argIndex > 0) args += ", ";
                    args += expandStructType(argType) + " " + argValue;
                    argIndex++;
                }
            }
        } else {
            // 默认 i32
            //irStream << "; function not found in paramTypesTable\n";
            for (size_t i = 0; i < node->call_params->expressions.size(); ++i) {
                if (i > 0) args += ", ";
                std::string argValue = visit_in_rhs(node->call_params->expressions[i].get());
                args += "i32 " + argValue;
            }
        }
    }
    std::string retType = functionTable[funcName];
    if (retType.empty()) retType = "i32"; // 默认
    if (retType == "void") {
        irStream << "  call void " << llvmGlobalRef(funcName) << "(" << args << ")\n";
        return "";
    } else {
        std::string temp = createTemp();
        irStream << "  %" << temp << " = call " << expandStructType(retType) << " " << llvmGlobalRef(funcName) << "(" << args << ")\n";
        return "%" + temp;
    }
}

std::string IRGenerator::visit(ArithmeticOrLogicalExpressionNode* node) {
  irStream << "; visiting arithmetic or logical expression\n";
  std::string lhs = visit(node->expression1.get());
  std::string lhsType = getLhsType(node->expression1.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression1.get())) {
    std::string lhs_path = path->toString();
    std::string lhs_type = lookupVarType(lhs_path);
    std::string lhs_addr = lookupSymbol(lhs_path);
    if (lhs_type == "i32*") {
      std::string lhs_temp = createTemp();
      irStream << "  %" << lhs_temp << " = load i32, i32* %" << lhs_addr << '\n';
      lhs = "%" + lhs_temp;
      lhsType = "i32";
    } else if (lhs_type == "i64*") {
      std::string lhs_temp = createTemp();
      irStream << "  %" << lhs_temp << " = load i64, i64* %" << lhs_addr << '\n';
      lhs = "%" + lhs_temp;
      lhsType = "i64";
    }
  }
  if (auto* type_cast = dynamic_cast<TypeCastExpressionNode*>(node->expression1.get())) {
    if (auto* path = dynamic_cast<PathExpressionNode*>(type_cast->expression.get())) {
      std::string lhs_path = path->toString();
      std::string lhs_type = lookupVarType(lhs_path);
      std::string lhs_addr = lookupSymbol(lhs_path);
      if (lhs_type == "i32*") {
        std::string lhs_temp = createTemp();
        irStream << "  %" << lhs_temp << " = load i32, i32* %" << lhs_addr << '\n';
        lhs = "%" + lhs_temp;
        lhsType = "i32";
      } else if (lhs_type == "i64*") {
        std::string lhs_temp = createTemp();
        irStream << "  %" << lhs_temp << " = load i64, i64* %" << lhs_addr << '\n';
        lhs = "%" + lhs_temp;
        lhsType = "i64";
        if (toIRType(type_cast->type.get()) == "i32") {
          lhsType = "i32";
          std::string temp = createTemp();
          irStream << "  %" << temp << " = trunc i64 " << lhs << " to i32\n";
          lhs = "%" + temp;
        }
      }
    }
  }
  std::string rhs = visit(node->expression2.get());
  std::string rhsType = getLhsType(node->expression2.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression2.get())) {
    std::string rhs_path = path->toString();
    std::string rhs_type = lookupVarType(rhs_path);
    std::string rhs_addr = lookupSymbol(rhs_path);
    if (rhs_type == "i32*") {
      std::string rhs_temp = createTemp();
      irStream << "  %" << rhs_temp << " = load i32, i32* %" << rhs_addr << '\n';
      rhs = "%" + rhs_temp;
      rhsType = "i32";
    } else if (rhs_type == "i64*") {
      std::string rhs_temp = createTemp();
      irStream << "  %" << rhs_temp << " = load i64, i64* %" << rhs_addr << '\n';
      rhs = "%" + rhs_temp;
      rhsType = "i64";
    }
  }
  std::string resultType = (lhsType == "i64" || rhsType == "i64") ? "i64" : "i32";
  // Convert operands to resultType if necessary
  if (resultType == "i64") {
    if (lhsType == "i32") {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = sext i32 " << lhs << " to i64\n";
      lhs = "%" + temp;
    }
    if (rhsType == "i32") {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = sext i32 " << rhs << " to i64\n";
      rhs = "%" + temp;
    }
  }
  irStream << "; lhsType: " << lhsType << ", rhsType: " << rhsType << ", resultType: " << resultType << "\n";
  std::string temp = createTemp();
  std::string op;
  switch (node->type) {
    case ADD: op = "add"; break;
    case MINUS: op = "sub"; break;
    case MUL: op = "mul"; break;
    case DIV: op = "sdiv"; break;
    case MOD: op = "srem"; break;
    case AND: op = "and"; break;
    case OR: op = "or"; break;
    case XOR: op = "xor"; break;
    case SHL: op = "shl"; break;
    case SHR: op = "ashr"; break;
    default: op = "add";
  }
  irStream << "  %" << temp << " = " << op << " " << resultType << " " << lhs << ", " << rhs << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(ComparisonExpressionNode* node) {
  irStream << "; visit ComparisonExpressionNode\n";
  std::string lhs = visit_in_rhs(node->expression1.get());
  std::string lhsType = getLhsType(node->expression1.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression1.get())) {
    std::string lhs_path = path->toString();
    //irStream << "; lhs_path in comparison expression: " << lhs_path << "\n";
    std::string lhs_type = lookupVarType(lhs_path);
    std::string lhs_addr = lookupSymbol(lhs_path);
    if (lhs_type == "i32*") {
      std::string lhs_temp = createTemp();
      irStream << "  %" << lhs_temp << " = load i32, i32* %" << lhs_addr << '\n';
      lhs = "%" + lhs_temp;
      lhsType = "i32";
    } else if (lhs_type == "i64*") {
      std::string lhs_temp = createTemp();
      irStream << "  %" << lhs_temp << " = load i64, i64* %" << lhs_addr << '\n';
      lhs = "%" + lhs_temp;
      lhsType = "i64";
    }
  }
  std::string rhs = visit_in_rhs(node->expression2.get());
  std::string rhsType = getLhsType(node->expression2.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression2.get())) {
    std::string rhs_path = path->toString();
    irStream << "; rhs_path in comparison expression: " << rhs_path << "\n";
    std::string rhs_type = lookupVarType(rhs_path);
    std::string rhs_addr = lookupSymbol(rhs_path);
    if (rhs_type == "i32*") {
      std::string rhs_temp = createTemp();
      irStream << "  %" << rhs_temp << " = load i32, i32* %" << rhs_addr << '\n';
      rhs = "%" + rhs_temp;
      rhsType = "i32";
    } else if (rhs_type == "i64*") {
      std::string rhs_temp = createTemp();
      irStream << "  %" << rhs_temp << " = load i64, i64* %" << rhs_addr << '\n';
      rhs = "%" + rhs_temp;
      rhsType = "i64";
    }
  }
  std::string compareType = (lhsType == "i64" || rhsType == "i64") ? "i64" : "i32";
  // Convert operands to compareType if necessary
  if (compareType == "i64") {
    if (lhsType == "i32") {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = sext i32 " << lhs << " to i64\n";
      lhs = "%" + temp;
    }
    if (rhsType == "i32") {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = sext i32 " << rhs << " to i64\n";
      rhs = "%" + temp;
    }
  }
  std::string temp = createTemp();
  std::string op;
  switch (node->type) {
    case EQ: op = "icmp eq"; break;
    case NEQ: op = "icmp ne"; break;
    case GT: op = "icmp sgt"; break;
    case LT: op = "icmp slt"; break;
    case GEQ: op = "icmp sge"; break;
    case LEQ: op = "icmp sle"; break;
    default: op = "icmp eq";
  }
  if (lhsType == "i1*") {
    std::string temp1 = createTemp();
    irStream << "  %" << temp1 << " = load i1, i1* " << lhs << "\n";
    lhs = "%" + temp1;
    lhsType = "i1";
  }
  if (rhsType == "i1*") {
    std::string temp2 = createTemp();
    irStream << "  %" << temp2 << " = load i1, i1* " << rhs << "\n";
    rhs = "%" + temp2;
    rhsType = "i1";
  }
  if (lhsType == "i1" && rhsType == "i1") {
    compareType = "i1";
  }
  irStream << "  %" << temp << " = " << op << " " << compareType << " " << lhs << ", " << rhs << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(AssignmentExpressionNode* node) {
  // 获取 lhs 地址和类型
  //irStream << "; visiting assignment expression\n";
  //irStream << "; getting address of lhs in assignment expression\n";
  std::string lhsAddr = getLhsAddress(node->expression1.get());
  //irStream << "; address of lhs in assignment expression: " << lhsAddr << "\n";
  //irStream << "; getting type of lhs in assignment expression\n";
  std::string lhsType = getLhsType(node->expression1.get());
  //irStream << "; type of lhs in assignment " << lhsType << "\n";
  if (lhsAddr.empty() || lhsType.empty()) {
    return "";
  }

  // 检查 rhs 是否是 if expression
  if (auto* ifExpr = dynamic_cast<IfExpressionNode*>(node->expression2.get())) {
    // 特殊处理 if expression：生成分支并在每个分支中 store
    //irStream << "; rhs is if expression, generating branches\n";
    std::string cond;
    if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(ifExpr->conditions->condition)) {
      cond = visit(std::get<std::unique_ptr<ExpressionNode>>(ifExpr->conditions->condition).get());
      if (auto* path = dynamic_cast<PathExpressionNode*>(std::get<std::unique_ptr<ExpressionNode>>(ifExpr->conditions->condition).get())) {
        std::string condName = path->toString();
        std::string condType = lookupVarType(condName);
        if (condType == "i1*") {
          std::string condTemp = createTemp();
          irStream << "  %" << condTemp << " = load i1, i1* " << cond << "\n";
          cond = "%" + condTemp;
        }
      }
    } else {
      error("Unsupported condition type in if assignment");
      return "";
    }

    std::string thenLabel = createLabel();
    std::string elseLabel = createLabel();
    std::string endLabel = createLabel();

    irStream << "  br i1 " << cond << ", label %" << thenLabel << ", label %" << elseLabel << "\n";

    irStream << thenLabel << ":\n";
    irStream << "  ; then branch assignment\n";
    // 处理 statements，但不处理 expression_without_block 的 store/br
    for (auto& stmt : ifExpr->block_expression->statement) {
      visit(stmt.get());
    }
    std::string thenValue = ifExpr->block_expression->expression_without_block ? visit(ifExpr->block_expression->expression_without_block.get()) : "";
    std::string thenType = ifExpr->block_expression->expression_without_block ? getLhsType(ifExpr->block_expression->expression_without_block.get()) : "void";
    if (lhsType == "i64" && thenType == "i32") {
      std::string sextTemp = createTemp();
      irStream << "  %" << sextTemp << " = sext i32 " << thenValue << " to i64\n";
      thenValue = "%" + sextTemp;
    }
    irStream << "  store " << expandStructType(lhsType) << " " << thenValue << ", " << expandStructType(lhsType) << "* " << lhsAddr << "\n";
    irStream << "  br label %" << endLabel << "\n";

    irStream << elseLabel << ":\n";
    irStream << "  ; else branch assignment\n";
    std::string elseValue;
    if (ifExpr->else_block) {
      // 类似处理 else_block
      for (auto& stmt : ifExpr->else_block->statement) {
        visit(stmt.get());
      }
      elseValue = ifExpr->else_block->expression_without_block ? visit(ifExpr->else_block->expression_without_block.get()) : "";
    } else if (ifExpr->else_if) {
      // 如果是 else_if，递归处理，但简化假设是 IfExpressionNode
      if (auto* elseIf = dynamic_cast<IfExpressionNode*>(ifExpr->else_if.get())) {
        // 类似处理
        for (auto& stmt : elseIf->block_expression->statement) {
          visit(stmt.get());
        }
        elseValue = elseIf->block_expression->expression_without_block ? visit(elseIf->block_expression->expression_without_block.get()) : "";
      } else {
        elseValue = visit(ifExpr->else_if.get());
      }
    } else {
      error("If expression in assignment must have else");
      return "";
    }
    irStream << "  store " << expandStructType(lhsType) << " " << elseValue << ", " << expandStructType(lhsType) << "* " << lhsAddr << "\n";
    irStream << "  br label %" << endLabel << "\n";

    irStream << endLabel << ":\n";
  } else {
    // 普通 rhs
    std::string rhsValue = visit_in_rhs(node->expression2.get());
    std::string rhsType = getLhsType(node->expression2.get());
    std::string lhsValue = visit(node->expression1.get());
    //irStream << "; lhs value in assignment expression: " << lhsValue << '\n';
    irStream << "; lhs type in assignment expression: " << lhsType << '\n';
    //irStream << "; rhs value in assignment expression: " << rhsValue << '\n';
    irStream << "; rhs type in assignment expression: " << rhsType << '\n';

    // 类型对齐：如果 rhs 比 lhs 多一层指针，则 load 之后再 store
    // 例：lhsValueType = T, rhsValueType = T*  =>  load T, T* rhsValue
    if (!lhsType.empty() && rhsType == lhsType + "*") {
      std::string rhsTemp = createTemp();
      irStream << "  %" << rhsTemp << " = load " << expandStructType(lhsType)
               << ", " << expandStructType(rhsType) << " " << rhsValue << "\n";
      rhsValue = "%" + rhsTemp;
      rhsType = lhsType;
    }
    if (!rhsType.empty() && lhsType == rhsType + "*") {
      std::string lhsTemp = createTemp();
      irStream << "  %" << lhsTemp << " = load " << expandStructType(rhsType)
               << ", " << expandStructType(lhsType) << " " << lhsValue << "\n";
      lhsValue = "%" + lhsTemp;
      lhsType = rhsType;
    }
    if (lhsType == "i32*" && rhsType == "i32*") {
      std::string rhsTemp = createTemp();
      irStream << "  %" << rhsTemp << " = load i32, i32* " << rhsValue << "\n";
      rhsValue = "%" + rhsTemp;
      irStream << "  store i32 " << rhsValue << ", i32* " << lhsValue << "\n";
      return "";
    }

    if (lhsType == "i1*" && rhsType == "i1*") {
      std::string rhsTemp = createTemp();
      irStream << "  %" << rhsTemp << " = load i1, i1* " << rhsValue << "\n";
      rhsValue = "%" + rhsTemp;
      irStream << "  store i1 " << rhsValue << ", i1* " << lhsValue << "\n";
      return "";
    }
    // 类型转换：如果 rhs 是 i64，lhs 是 i32，trunc
    if (rhsType == "i64" && lhsType == "i32") {
      std::string truncTemp = createTemp();
      irStream << "  %" << truncTemp << " = trunc i64 " << rhsValue << " to i32\n";
      rhsValue = "%" + truncTemp;
      rhsType = "i32";
    }

    // 类型转换：如果 lhs 是 i64，rhs 是 i32，sext
    if (lhsType == "i64" && rhsType == "i32") {
      std::string sextTemp = createTemp();
      irStream << "  %" << sextTemp << " = sext i32 " << rhsValue << " to i64\n";
      rhsValue = "%" + sextTemp;
      rhsType = "i64";
    }

    // 统一使用 lhsValueType 进行 store
    irStream << "  store " << expandStructType(lhsType) << " " << rhsValue
             << ", " << expandStructType(lhsType) << "* " << lhsAddr << "\n";
  }
  return "";
}

std::string IRGenerator::visit(CompoundAssignmentExpressionNode* node) {
  // 获取 lhs 地址和类型
  irStream << "; visiting compoundassignmentexpression\n";
  //irStream << "; getting address of lhs in compoundexpression\n";
  std::string lhsAddr = getLhsAddress(node->expression1.get());
  //irStream << "; address of lhs in compoundexpression: "<< lhsAddr << "\n";
  //irStream << "; getting type of lhs in compoundexpression\n";
  std::string lhsType = getLhsType(node->expression1.get());
  //irStream << "; type of lhs in compoundexpression: "<< lhsType << "\n";
  if (lhsAddr.empty() || lhsType.empty()) {
    return "";
  }

  if (!lhsType.empty() && lhsType.back() == '*') {
    lhsType.pop_back();
  }

  // load lhs 值
  std::string lhsValue;
  if (lhsType.find('%') == 0 && lhsType.back() == '*') {
    // 结构体指针，load 结构体
    std::string structType = lhsType.substr(0, lhsType.size() - 1);
    std::string loadTemp = createTemp();
    irStream << "  %" << loadTemp << " = load " << expandStructType(structType) << ", " << expandStructType(lhsType) << " " << lhsAddr << "\n";
    lhsValue = "%" + loadTemp;
  } else {
    // 基本类型，load
    std::string loadTemp = createTemp();
    irStream << "  %" << loadTemp << " = load " << expandStructType(lhsType) << ", " << expandStructType(lhsType) << "* " << lhsAddr << "\n";
    lhsValue = "%" + loadTemp;
  }

  // 获取 rhs 值
  std::string rhsValue = visit(node->expression2.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression2.get())) {
    std::string rhsName = path->toString();
    //irStream << "; rhs name in compoundassignmentexpression: " << rhsName << "\n";
    std::string rhsType = lookupVarType(rhsName);
    std::string rhsTemp = createTemp();
    irStream << "; rhs type in compoundassignmentexpression: " << rhsType << "\n";
    irStream << "; lhs type in compoundassignmentexpression: " << lhsType << "\n";
    if (isLetDefined(rhsName) && rhsType == lhsType + "*") {
      irStream << "  %" << rhsTemp << " = load " << expandStructType(lhsType)
               << ", " << expandStructType(rhsType) << " " << rhsValue << "\n";
      rhsValue = "%" + rhsTemp;
      rhsType = lhsType;
    }
  }

  // 计算 lhs op rhs
  std::string op;
  switch (node->type) {
    case ADD: op = "add"; break;
    case MINUS: op = "sub"; break;
    case MUL: op = "mul"; break;
    case DIV: op = "sdiv"; break;
    case MOD: op = "srem"; break;
    case AND: op = "and"; break;
    case OR: op = "or"; break;
    case XOR: op = "xor"; break;
    case SHL: op = "shl"; break;
    case SHR: op = "ashr"; break;
    default: op = "add";
  }
  std::string resultTemp = createTemp();
  irStream << "  %" << resultTemp << " = " << op << " " << lhsType << " " << lhsValue << ", " << rhsValue << "\n";

  // store 结果到 lhs
  if (lhsType.find('%') == 0 && lhsType.back() == '*') {
    // 结构体指针，store 结构体值
    std::string structType = lhsType.substr(0, lhsType.size() - 1);
    irStream << "  store " << expandStructType(structType) << " %" << resultTemp << ", " << expandStructType(lhsType) << " " << lhsAddr << "\n";
  } else if (lhsType[0] == '{') {
    // struct value
    irStream << "  store " << expandStructType(lhsType) << " %" << resultTemp << ", " << expandStructType(lhsType) << "* " << lhsAddr << "\n";
  } else {
    // 基本类型
    irStream << "  store " << expandStructType(lhsType) << " %" << resultTemp << ", " << expandStructType(lhsType) << "* " << lhsAddr << "\n";
  }
  //irStream << "; finish visiting compound assignment expression\n";
  return "";
}

std::string IRGenerator::visit(StructExpressionNode* node) {
  //irStream << "; visiting struct expression\n";
  std::string structName = node->pathin_expression->toString();
  auto it = structFields.find(structName);
  if (it == structFields.end()) {
    error("Unknown struct " + structName);
    return "";
  }
  std::string structType = expandStructType("%" + structName);

  // 初始值
  std::string currentValue;
  if (node->struct_base) {
    currentValue = visit(node->struct_base->expression.get());
  } else {
    currentValue = "undef";
  }

  // 为每个字段生成 insertvalue
  if (node->struct_expr_fields) {
    for (const auto& field : node->struct_expr_fields->struct_expr_fields) {
      std::string fieldName;
      if (std::holds_alternative<Identifier>(field->id_or_tupe_index)) {
        fieldName = std::get<Identifier>(field->id_or_tupe_index).id;
      } else {
        // tuple index, 暂时不支持
        error("Tuple index in struct expression not supported");
        return "";
      }
      // 找到索引
      int index = -1;
      for (int i = 0; i < it->second.size(); ++i) {
        if (it->second[i].first == fieldName) {
          index = i;
          break;
        }
      }
      if (index == -1) {
        error("Field not found: " + fieldName);
        return "";
      }
      std::string fieldValue = visit(field->expression.get());
      std::string temp = createTemp();
      irStream << "  %" << temp << " = insertvalue " << expandStructType(structType) << " " << currentValue << ", " << expandStructType(it->second[index].second) << " " << fieldValue << ", " << index << "\n";
      currentValue = "%" + temp;
    }
  }

  return currentValue;
}

std::string IRGenerator::visit(DereferenceExpressionNode* node) {
  //irStream << "; visit dereferenceexpression\n";
  std::string ptr = visit(node->expression.get());
  std::string ptrType = getLhsType(node->expression.get());
  std::string valueType;
  if (ptrType.back() == '*') {
    valueType = ptrType.substr(0, ptrType.size() - 1);
  } else {
    error("Dereference on non-pointer type");
    return "";
  }
  std::string temp = createTemp();
  irStream << "  %" << temp << " = load " << expandStructType(valueType) << ", " << expandStructType(ptrType) << " " << ptr << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(NegationExpressionNode* node) {
  std::string expr = visit(node->expression.get());
  std::string temp = createTemp();
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression.get())) {
    std::string negName = path->toString();
    std::string negType = lookupVarType(negName);
    if (negType == "i32*") {
      std::string negTemp = createTemp();
      irStream << "  %" << negTemp << " = load i32, i32* " << expr << "\n";
      expr = "%" + negTemp;
    }
  }
  if (node->type == NegationExpressionNode::MINUS) {
    irStream << "  %" << temp << " = sub i32 0, " << expr << "\n";
  } else if (node->type == NegationExpressionNode::BANG) {
    std::string exprType = getLhsType(node->expression.get());
    if (exprType == "i1") {
      irStream << "  %" << temp << " = xor i1 " << expr << ", -1\n";
    } else if (exprType == "i32") {
      irStream << "  %" << temp << " = xor i32 " << expr << ", -1\n";
    } else if (exprType == "i1*") {
      std::string exprTemp = createTemp();
      irStream << "  %" << exprTemp << " = load i1, i1* " << expr << "\n";
      irStream << "  %" << temp << " = xor i1 %" << exprTemp << ", -1\n";
    }
  } else {
    error("Unknown negation type");
    return "";
  }
  return "%" + temp;
}

std::string IRGenerator::visit(BorrowExpressionNode* node) {
  std::string addr = getLhsAddress(node->expression.get());
  if (!addr.empty()) {
    return addr;
  } else {
    // 计算值，alloca 临时，store，返回指针
    std::string value = visit(node->expression.get());
    std::string type = getLhsType(node->expression.get());
    if (type.empty()) type = "i32"; // 默认
    std::string tempPtr = createTemp();
    irStream << "  %" << tempPtr << " = alloca " << type << "\n";
    irStream << "  store " << expandStructType(type) << " " << value << ", " << expandStructType(type) << "* %" << tempPtr << "\n";
    return "%" + tempPtr;
  }
}

std::string IRGenerator::visit(MethodCallExpressionNode* node) {
  //irStream << "; visiting method call expression\n";
  std::string methodName;
  if (std::holds_alternative<Identifier>(node->path_expr_segment)) {
    methodName = std::get<Identifier>(node->path_expr_segment).id;
  } else {
    error("PathInType in method call not supported");
    return "";
  }
  std::string self = visit(node->expression.get());
  irStream << "; self: " << self << '\n';
  std::string selfType = getLhsType(node->expression.get());
  if (selfType.empty()) selfType = "i32";  // 默认

  // 根据 receiver 的类型合成真实的函数名：Type::FuncName。
  // 注意：selfType 可能带有若干 '*'（比如 %Foo**），这里需要全部去掉。
  // 同时 IR 层结构体类型通常长成 %Foo，因此要去掉前导 '%'.
  std::string baseTypeForName;
  baseTypeForName = stripTrailingStars(selfType);
  if (!baseTypeForName.empty() && baseTypeForName.front() == '%') baseTypeForName.erase(baseTypeForName.begin());
  if (self == "%self") {
    //irStream << "; base of methodcall expression: self\n";
    //irStream << "; type of methodcall expression: " << selfType << "\n";
    baseTypeForName = stripTrailingStars(selfType);
    if (baseTypeForName.front() == '%') baseTypeForName.erase(baseTypeForName.begin());
  }
  std::string mangledMethodName = baseTypeForName.empty() ? methodName : (baseTypeForName + "_" + methodName);
  //irStream << "; self: " << self << ", type: " << selfType << '\n';
  irStream << "; baseTypeForName: " << baseTypeForName << '\n';
  irStream << "; method name: " << mangledMethodName << '\n';
  // 检查是否需要 autoref
  auto it = paramTypesTable.find(mangledMethodName);
  if (it != paramTypesTable.end()) {
    const auto& paramTypes = it->second;
    if (!paramTypes.empty()) {
      std::string expectedSelfType = paramTypes[0];
      //irStream << "; expected self type: " << expectedSelfType << '\n';
      if (expectedSelfType.back() == '*' && selfType.back() != '*') {
        // 需要引用，但self不是引用，生成 &self
        std::string addr = getLhsAddress(node->expression.get());
        if (!addr.empty()) {
          self = addr;
          selfType = selfType + "*";
          //irStream << "; autoref: using address " << self << '\n';
        } else {
          // 如果不能取地址，创建一个临时
          std::string tempPtr = createTemp();
          irStream << "  %" << tempPtr << " = alloca " << expandStructType(selfType) << "\n";
          irStream << "  store " << expandStructType(selfType) << " " << self << ", " << expandStructType(selfType) << "* %" << tempPtr << "\n";
          self = "%" + tempPtr;
          selfType = selfType + "*";
          //irStream << "; autoref: created temp ptr " << self << '\n';
        }
      } else if (expectedSelfType == "ptr" && selfType[0] == '%' && selfType.back() != '*') {
        // 需要引用，但self是值，生成 &self
        std::string addr = getLhsAddress(node->expression.get());
        if (!addr.empty()) {
          self = addr;
          selfType = "ptr";
        } else {
          std::string tempPtr = createTemp();
          irStream << "  %" << tempPtr << " = alloca " << expandStructType(selfType) << "\n";
          irStream << "  store " << expandStructType(selfType) << " " << self << ", " << expandStructType(selfType) << "* %" << tempPtr << "\n";
          self = "%" + tempPtr;
          selfType = "ptr";
        }
      }

      // 参数类型检查：如果 self 的实际类型比声明多一层指针，则需要 load
      if (!expectedSelfType.empty() && selfType == expectedSelfType + "*") {
        irStream << "; self type: " << selfType << ", expected: " << expectedSelfType << ", need load\n";
        std::string tmp = createTemp();
        irStream << "  %" << tmp << " = load " << expandStructType(expectedSelfType)
                 << ", " << expandStructType(selfType) << " " << self << "\n";
        self = "%" + tmp;
        selfType = expectedSelfType;
      } else if (expectedSelfType == "%" + selfType.substr(1) && selfType[0] == '%' && selfType.back() == '*' && expectedSelfType != selfType) {
        // expected is value, self is pointer, load
        irStream << "; self type: " << selfType << ", expected: " << expectedSelfType << ", need load\n";
        std::string tmp = createTemp();
        irStream << "  %" << tmp << " = load " << expandStructType(expectedSelfType)
                 << ", " << expandStructType(selfType) << " " << self << "\n";
        self = "%" + tmp;
        selfType = expectedSelfType;
      }
    }
  }

  std::string args;
  // 处理 self
  irStream << "; self type: " << selfType << '\n';
  if (selfType[0] == '%' && selfType.back() != '*') {
    std::string name = selfType.substr(1);
    auto it2 = structFields.find(name);
    if (it2 != structFields.end()) {
      for (size_t j = 0; j < it2->second.size(); ++j) {
        if (!args.empty()) args += ", ";
        std::string temp = createTemp();
        irStream << "  %" << temp << " = extractvalue " << expandStructType(selfType) << " " << self << ", " << j << "\n";
        args += it2->second[j].second + " %" + temp;
      }
    } else {
      args = expandStructType(selfType) + " " + self;
    }
  } else {
    args = expandStructType(selfType) + " " + self;
  }
  if (node->call_params) {
    if (it != paramTypesTable.end()) {
      const auto& paramTypes = it->second;
      size_t argIndex = 1;  // self是0
      irStream << "; size of call_params: " << node->call_params->expressions.size() << '\n';
      for (size_t i = 0; i < node->call_params->expressions.size(); ++i) {
        std::string argValue = visit(node->call_params->expressions[i].get());
        irStream << "; argvalue: " << argValue << '\n';
        std::string argType = (argIndex < paramTypes.size()) ? paramTypes[argIndex] : "i32";

        // 参数类型检查：如果实参类型比声明类型多一层指针，则需要 load
        std::string actualType;
        actualType = getLhsType(node->call_params->expressions[i].get());
        if (dynamic_cast<BorrowExpressionNode*>(node->call_params->expressions[i].get())) {
          actualType = actualType.substr(0, actualType.size() - 1);
        }
        irStream << "; arg type: " << argType << ", actual: " << actualType << '\n';
        if (!actualType.empty() && actualType == argType + "*") {
          irStream << "; need loading\n";
          std::string tmp = createTemp();
          irStream << "  %" << tmp << " = load " << expandStructType(argType)
                   << ", " << expandStructType(actualType) << " " << argValue << "\n";
          argValue = "%" + tmp;
        }

        if (argType[0] == '%' && argType.back() != '*') {
          std::string name = argType.substr(1);
          auto it3 = structFields.find(name);
          if (it3 != structFields.end()) {
            for (size_t j = 0; j < it3->second.size(); ++j) {
              if (!args.empty()) args += ", ";
              std::string temp = createTemp();
              irStream << "  %" << temp << " = extractvalue " << expandStructType(argType) << " " << argValue << ", " << j << "\n";
              args += it3->second[j].second + " %" + temp;
            }
          } else {
            if (!args.empty()) args += ", ";
            args += expandStructType(argType) + " " + argValue;
          }
        } else {
          if (!args.empty()) args += ", ";
          args += expandStructType(argType) + " " + argValue;
        }
        argIndex++;
      }
    } else {
      for (auto& arg : node->call_params->expressions) {
        if (!args.empty()) args += ", ";
        args += "i32 " + visit(arg.get());
      }
    }
  }
  //irStream << "; args: " << args << '\n';
  std::string retType = functionTable[mangledMethodName];
  if (retType.empty()) retType = "i32";
  std::string temp = createTemp();
  if (retType == "void") {
    irStream << "  call void " << llvmGlobalRef(mangledMethodName) << "(" << args << ")\n";
    return "";
  } else {
    std::string temp = createTemp();
    irStream << "  %" << temp << " = call " << expandStructType(retType) << " " << llvmGlobalRef(mangledMethodName) << "(" << args << ")\n";
    return "%" + temp;
  }
}

std::string stripStarOnce(const std::string &t) {
  if (!t.empty() && t.back() == '*') return t.substr(0, t.size() - 1);
  return t;
}

bool isArrayType(const std::string &t) {
  return !t.empty() && t.front() == '[';
}

bool isPointerType(const std::string &t) {
  return !t.empty() && t.back() == '*';
}

std::string IRGenerator::visit(IndexExpressionNode* node) {
  //irStream << "; visiting index expression\n";
  std::string baseAddr = getLhsAddress(node->base.get());
  //irStream << "; base address of index expression: " << baseAddr << '\n';
  std::string baseType = getLhsType(node->base.get());
  //irStream << "; base type of index expression: " << baseType << '\n';
  if (baseAddr.empty() || baseType.empty()) return "";

  std::string idxVal = visit(node->index.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->index.get())) {
    std::string idx_path = path->toString();
    std::string idx_type = lookupVarType(idx_path);
    std::string idx_addr = lookupSymbol(idx_path);
    if (idx_type == "i32*") {
      std::string idx_temp = createTemp();
      irStream << "  %" << idx_temp << " = load i32, i32* %" << idx_addr << '\n';
      idxVal = "%" + idx_temp;
    }
  }
  std::string ptrTemp = createTemp();

  //irStream << "; idxVal: " << idxVal << '\n';

  if (isArrayType(baseType) && !isPointerType(baseType)) {
    irStream << "  %" << ptrTemp << " = getelementptr " << expandStructType(baseType) << ", " << expandStructType(baseType) << "* " << baseAddr << ", i32 0, i32 " << idxVal << "\n";
    std::string res = "%" + ptrTemp;
    std::string loadTemp = createTemp();
    std::string elemType = getElementType(baseType);
    irStream << "  %" << loadTemp << " = load " << expandStructType(elemType) << ", " << expandStructType(elemType) << "* " << res << "\n";
    return "%" + loadTemp;
  }

  if (isArrayType(baseType) && isPointerType(baseType)) {
    irStream << "  %" << ptrTemp << " = getelementptr " << expandStructType(stripStarOnce(baseType)) << ", " << expandStructType(baseType) << " " << baseAddr << ", i32 0, i32 " << idxVal << "\n";
    std::string res = "%" + ptrTemp;
    std::string loadTemp = createTemp();
    std::string elemType = getElementType(baseType);
    //irStream << "; elem type: " << elemType << '\n';

    irStream << "  %" << loadTemp << " = load " << expandStructType(stripStarOnce(elemType)) << ", " << expandStructType(stripStarOnce(elemType)) << "* " << res << "\n";
    return "%" + loadTemp;
  }

  std::string loadedPtr = createTemp();
  irStream << "  %" << loadedPtr << " = load " << expandStructType(baseType) << ", " << expandStructType(baseType) << "* " << baseAddr << "\n";

  if (isArrayType(stripStarOnce(baseType))) {
    std::string arrayType = stripStarOnce(baseType);
    irStream << "  %" << ptrTemp << " = getelementptr " << expandStructType(arrayType) << ", " << expandStructType(arrayType) << "* %" << loadedPtr << ", i32 0, i32 " << idxVal << "\n";
    std::string elemType = getElementType(arrayType);
    std::string loadTemp = createTemp();
    irStream << "  %" << loadTemp << " = load " << expandStructType(stripStarOnce(elemType)) << ", " << expandStructType(stripStarOnce(elemType)) << "* %" << ptrTemp << "\n";
    return "%" + loadTemp;
  } else {
    std::string elemType = getElementType(baseType);
    irStream << "  %" << ptrTemp << " = getelementptr " << elemType << ", " << elemType << "* %" << loadedPtr << ", i32 " << idxVal << "\n";
    std::string loadTemp = createTemp();
    irStream << "  %" << loadTemp << " = load " << expandStructType(elemType) << ", " << expandStructType(elemType) << "* %" << ptrTemp << "\n";
    return "%" + loadTemp;
  }
}


std::string IRGenerator::visit(PredicateLoopExpressionNode* node) {
  std::string loopLabel = createLabel();
  std::string bodyLabel = createLabel();
  std::string endLabel = createLabel();

  std::string oldLoopLabel = currentLoopLabel;
  std::string oldBreakLabel = currentBreakLabel;
  currentLoopLabel = loopLabel;
  currentBreakLabel = endLabel;

  // Pre-allocate addresses for let statements in the loop body
  loopPreAlloc.clear();
  for (auto& stmt : node->block_expression->statement) {
    if (stmt->let_statement) {
      std::string varName = stmt->let_statement->pattern->toString();
      std::string type = stmt->let_statement->type ? toIRType(stmt->let_statement->type.get()) : "i32";
      std::string allocTemp = createTemp();
      irStream << "  %" << allocTemp << " = alloca " << expandStructType(type) << "\n";
      loopPreAlloc[varName] = allocTemp;
    }
  }

  irStream << "  br label %" << loopLabel << "\n";
  irStream << loopLabel << ":\n";
  std::string cond;
  if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(node->conditions->condition)) {
    cond = visit(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get());
    if (auto* path = dynamic_cast<PathExpressionNode*>(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get())) {
      std::string condName = path->toString();
      std::string condType = lookupVarType(condName);
      if (condType == "i1*") {
        std::string condTemp = createTemp();
        irStream << "  %" << condTemp << " = load i1, i1* " << cond << "\n";
        cond = "%" + condTemp;
      }
    }
  } else {
    error("Unsupported condition type in while");
    return "";
  }
  irStream << "  br i1 " << cond << ", label %" << bodyLabel << ", label %" << endLabel << "\n";

  irStream << bodyLabel << ":\n";
  visit(node->block_expression.get());
  irStream << "  br label %" << loopLabel << "\n";

  irStream << endLabel << ":\n";

  currentLoopLabel = oldLoopLabel;
  currentBreakLabel = oldBreakLabel;
  loopPreAlloc.clear(); // Clear after loop
  return "";
}

std::string IRGenerator::visit(ContinueExpressionNode* node) {
  if (currentLoopLabel.empty()) {
    error("continue outside of loop");
    return "";
  }
  irStream << "  br label %" << currentLoopLabel << "\n";
  return "";
}

std::string IRGenerator::visit(BreakExpressionNode* node) {
  if (currentBreakLabel.empty()) {
    error("break outside of loop");
    return "";
  }
  irStream << "  br label %" << currentBreakLabel << "\n";
  return "";
}

std::string IRGenerator::visit(TypeCastExpressionNode* node) {
  irStream << "; visiting typecastexpression\n";
  std::string expr = visit(node->expression.get());
  std::string srcType = getLhsType(node->expression.get());
  std::string dstType = toIRType(node->type.get());
  bool isDstU32 = false;
  if (auto* typePath = dynamic_cast<TypePathNode*>(node->type.get())) {
    if (typePath->type_path->toString() == "u32") {
      isDstU32 = true;
    }
  }
  if (srcType == "i1" && dstType == "i32") {
    std::string temp = createTemp();
    irStream << "  %" << temp << " = zext i1 " << expr << " to i32\n";
    return "%" + temp;
  } else if (srcType == "i32" && dstType == "i32") {
    if (isDstU32) {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = sext i32 " << expr << " to i64\n";
      return "%" + temp;
    } else {
      return expr;
    }
  } else if (srcType == "i64" && dstType == "i32") {
    std::string temp = createTemp();
    irStream << "  %" << temp << " = trunc i64 " << expr << " to i32\n";
    return "%" + temp;
  } else if (srcType == "i64*" && dstType == "i32") {
    std::string srcTemp = createTemp();
    irStream << "  %" << srcTemp << " = load i64, i64* " << expr << "\n";
    std::string temp = createTemp();
    irStream << "  %" << temp << " = trunc i64 %" << srcTemp << " to i32\n";
    return "%" + temp;
  } else if (srcType == "i32*" && (dstType == "i32" || dstType == "usize")) {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = load i32, i32* " << expr << "\n";
      return "%" + temp;
  } else {
    // Other casts not implemented, just return the expression
    return expr;
  }
}

std::string IRGenerator::visit(ArrayExpressionNode* node) {
  if (node->if_empty) {
    error("Empty array expression not supported");
    return "";
  }
  std::string elementType = "i32"; // default
  if (!node->expressions.empty()) {
    elementType = getLhsType(node->expressions[0].get());
  }
  int size;
  if (node->type == ArrayExpressionType::REPEAT) {
    auto countOpt = evaluateConstant(node->expressions[1].get());
    if (!countOpt) {
      error("Array repeat count must be constant");
      return "";
    }
    size = *countOpt;
  } else {
    size = node->expressions.size();
  }
  // Memory model for array construction:
  //   1) alloca array once
  //   2) gep to element
  //   3) store element
  //   4) load whole array value as expression result
  std::string arrPtr = createTemp();
  std::string arrayType = "[" + std::to_string(size) + " x " + expandStructType(elementType) + "]";
  irStream << "  %" << arrPtr << " = alloca " << arrayType << "\n";

  // Store elements
  if (node->type == ArrayExpressionType::REPEAT) {
    std::string value = visit(node->expressions[0].get());
    // Generate loop for repeat initialization
    std::string loopVar = createTemp();
    std::string loopLabel = createLabel();
    std::string bodyLabel = createLabel();
    std::string endLabel = createLabel();
    irStream << "  %" << loopVar << " = alloca i32\n";
    irStream << "  store i32 0, i32* %" << loopVar << "\n";
    irStream << "  br label %" << loopLabel << "\n";
    irStream << loopLabel << ":\n";
    std::string iv = createTemp();
    irStream << "  %" << iv << " = load i32, i32* %" << loopVar << "\n";
    std::string cond = createTemp();
    irStream << "  %" << cond << " = icmp slt i32 %" << iv << ", " << size << "\n";
    irStream << "  br i1 %" << cond << ", label %" << bodyLabel << ", label %" << endLabel << "\n";
    irStream << bodyLabel << ":\n";
    std::string elemPtr = createTemp();
    irStream << "  %" << elemPtr << " = getelementptr " << arrayType << ", " << arrayType << "* %" << arrPtr
             << ", i32 0, i32 %" << iv << "\n";
    irStream << "  store " << expandStructType(elementType) << " " << value
             << ", " << expandStructType(elementType) << "* %" << elemPtr << "\n";
    std::string ivNext = createTemp();
    irStream << "  %" << ivNext << " = add i32 %" << iv << ", 1\n";
    irStream << "  store i32 %" << ivNext << ", i32* %" << loopVar << "\n";
    irStream << "  br label %" << loopLabel << "\n";
    irStream << endLabel << ":\n";
  } else {
    for (int i = 0; i < size; ++i) {
      std::string value = visit(node->expressions[i].get());
      std::string elemPtr = createTemp();
      irStream << "  %" << elemPtr << " = getelementptr " << arrayType << ", " << arrayType << "* %" << arrPtr
               << ", i32 0, i32 " << i << "\n";
      if (auto* path = dynamic_cast<PathExpressionNode*>(node->expressions[i].get())) {
        std::string varName = path->toString();
        std::string varType = lookupVarType(varName);
        if (varType == "i1*") {
          std::string val = createTemp();
          irStream << "  %" << val << " = load i1, i1* " << value << "\n";
          value = "%" + val;
        }
        if (varType == "i32*") {
          std::string val = createTemp();
          irStream << "  %" << val << " = load i32, i32* " << value << "\n";
          value = "%" + val;
        }
      }
      irStream << "  store " << expandStructType(elementType) << " " << value
               << ", " << expandStructType(elementType) << "* %" << elemPtr << "\n";
    }
  }

  std::string arrVal = createTemp();
  irStream << "  %" << arrVal << " = load " << arrayType << ", " << arrayType << "* %" << arrPtr << "\n";
  return "%" + arrVal;
}

std::string IRGenerator::visit(LazyBooleanExpressionNode* node) {
  irStream << "; visiting lazy boolean expression\n";
  std::string resultPtr = createTemp();
  irStream << "  %" << resultPtr << " = alloca i1\n";
  std::string lhs = visit(node->expression1.get());
  std::string trueLabel = createLabel();
  std::string falseLabel = createLabel();
  std::string endLabel = createLabel();

  if (node->type == LAZY_AND) {
    if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression1.get())) {
      std::string lhsName = path->toString();
      std::string lhsType = lookupVarType(lhsName);
      if (lhsType == "i1*") {
        std::string lhsVal = createTemp();
        irStream << "  %" << lhsVal << " = load i1, i1* " << lhs << "\n";
        lhs = "%" + lhsVal;
      }
    }
    irStream << "  br i1 " << lhs << ", label %" << trueLabel << ", label %" << falseLabel << "\n";

    irStream << trueLabel << ":\n";
    std::string rhs = visit(node->expression2.get());
    if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression2.get())) {
      std::string rhsName = path->toString();
      std::string rhsType = lookupVarType(rhsName);
      if (rhsType == "i1*") {
        std::string rhsVal = createTemp();
        irStream << "  %" << rhsVal << " = load i1, i1* " << rhs << "\n";
        rhs = "%" + rhsVal;
      }
    }
    irStream << "  store i1 " << rhs << ", i1* %" << resultPtr << "\n";
    irStream << "  br label %" << endLabel << "\n";

    irStream << falseLabel << ":\n";
    irStream << "  store i1 0, i1* %" << resultPtr << "\n";
    irStream << "  br label %" << endLabel << "\n";
  } else { // LAZY_OR
    if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression1.get())) {
      std::string lhsName = path->toString();
      std::string lhsType = lookupVarType(lhsName);
      if (lhsType == "i1*") {
        std::string lhsVal = createTemp();
        irStream << "  %" << lhsVal << " = load i1, i1* " << lhs << "\n";
        lhs = "%" + lhsVal;
      }
    }
    irStream << "  br i1 " << lhs << ", label %" << trueLabel << ", label %" << falseLabel << "\n";

    irStream << trueLabel << ":\n";
    irStream << "  store i1 1, i1* %" << resultPtr << "\n";
    irStream << "  br label %" << endLabel << "\n";

    irStream << falseLabel << ":\n";
    std::string rhs = visit(node->expression2.get());
    if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression2.get())) {
      std::string rhsName = path->toString();
      std::string rhsType = lookupVarType(rhsName);
      if (rhsType == "i1*") {
        std::string rhsVal = createTemp();
        irStream << "  %" << rhsVal << " = load i1, i1* " << rhs << "\n";
        rhs = "%" + rhsVal;
      }
    }
    irStream << "  store i1 " << rhs << ", i1* %" << resultPtr << "\n";
    irStream << "  br label %" << endLabel << "\n";
  }

  irStream << endLabel << ":\n";
  std::string result = createTemp();
  irStream << "  %" << result << " = load i1, i1* %" << resultPtr << "\n";
  return "%" + result;
}

std::string IRGenerator::visit(BlockExpressionNode* node) {
  //irStream << "; visiting block expression\n";
  enterScope();
  for (const auto& stmt : node->statement) {
    visit(stmt.get());
  }
  std::string result = "";
  if (node->expression_without_block) {
    irStream << "; visiting expression without block in block expression\n";
    if (inFunctionBody) {
      //irStream << "; visiting expression without block in block expression in func body\n";
      std::string value = visit(node->expression_without_block.get());
      if (auto* p = std::get_if<std::unique_ptr<PathExpressionNode>>(&node->expression_without_block->expr)) {
        PathExpressionNode* path = p->get();
        //irStream << "; visiting path expression in block expression\n";
        std::string retName = path->toString();
        //irStream << "; retName: " << retName << "\n";
        std::string retType = lookupVarType(retName);
        std::string retTemp = createTemp();
        if (isLetDefined(retName) && retType == currentRetType + "*") {
          irStream << "  %" << retTemp << " = load " << expandStructType(currentRetType)
                 << ", " << expandStructType(retType) << " " << value << "\n";
          value = "%" + retTemp;
        }
      }
      if (!std::holds_alternative<std::unique_ptr<ReturnExpressionNode>>(node->expression_without_block->expr)) {
        irStream << "  store " << expandStructType(currentRetType) << " " << value << ", " << expandStructType(currentRetType) << "* %" << returnVar << "\n";
        irStream << "  br label %" << returnLabel << "\n";
      }
    } else {
      result = visit(node->expression_without_block.get());
    }
  }
  exitScope();
  return result;
}

std::string IRGenerator::visit(ReturnExpressionNode* node) {
  irStream << "; visiting return expression\n";
  if (node->expression) {
    std::string value = visit_in_rhs(node->expression.get());
    if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression.get())) {
      std::string retName = path->toString();
      std::string retType = lookupVarType(retName);
      std::string retTemp = createTemp();
      //irStream << "; retType: " << retType << "\n";
      //irStream << "; currentRetType: " << currentRetType << "\n";
      if (isLetDefined(retName) && expandStructType(retType) == expandStructType(currentRetType) + "*") {
        irStream << "  %" << retTemp << " = load " << expandStructType(currentRetType)
               << ", " << expandStructType(retType) << " " << value << "\n";
        value = "%" + retTemp;
      }
    }
    irStream << "  ret " << currentRetType << " " << value << "\n";
  } else {
    irStream << "  ret void\n";
  }
  return "";
}

std::string IRGenerator::visit(GroupedExpressionNode* node) {
  return visit(node->expression.get());
}

std::string IRGenerator::visit(ExpressionWithoutBlockNode* node) {
  return std::visit([&](auto&& arg) -> std::string {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::unique_ptr<LiteralExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<PathExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<OperatorExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<NegationExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<CallExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<MethodCallExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<IndexExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ArrayExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<RangeExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<FieldExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<GroupedExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<StructExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<DereferenceExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<LazyBooleanExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<TypeCastExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<TupleExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<TupleIndexingExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ContinueExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<BreakExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ReturnExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<UnderscoreExpressionNode>>) {
      return visit(arg.get());
    } else {
      error("Unsupported ExpressionWithoutBlock type");
      return "";
    }
  }, node->expr);
}

std::string IRGenerator::visit(OperatorExpressionNode* node) {
  return std::visit([&](auto&& arg) -> std::string {
    using T = std::decay_t<decltype(arg)>;
    if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<ComparisonExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<AssignmentExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<CompoundAssignmentExpressionNode>>) {
      return visit(arg.get());
    } else if constexpr (std::is_same_v<T, std::unique_ptr<BorrowExpressionNode>>) {
      return visit(arg.get());
    } else {
      error("Unsupported OperatorExpression type");
      return "";
    }
  }, node->operator_expression);
}


std::string IRGenerator::visit(FieldExpressionNode* node) {
  irStream << "; visiting field expression\n";
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression.get())) {
    std::string baseName = path->toString();
    std::string fieldSym = lookupSymbol(baseName + "." + node->identifier.id);
    if (!fieldSym.empty()) {
      return "%" + fieldSym;
    }
  }
  std::string addr = getLhsAddress(node);
  irStream << "; lhs address of fieldexpression: " << addr << '\n';
  std::string type = getLhsType(node);
  irStream << "; lhs type of fieldexpression: " << type << '\n';
  if (addr.empty() || type.empty()) {
    return "";
  }
  std::string temp = createTemp();
  irStream << "  %" << temp << " = load " << expandStructType(type) << ", " << expandStructType(type) << "* " << addr << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit(IfExpressionNode* node) {
  // 处理 conditions
  irStream << "; visiting ifexpression\n";
  std::string cond;
  if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(node->conditions->condition)) {
    const auto& expr = std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition);
    cond = visit(expr.get());
    irStream << "; cond: " << cond << "\n";
    irStream << "; expresstin type: " << typeid(*expr.get()).name() << "\n";
    if (auto* path = dynamic_cast<PathExpressionNode*>(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get())) {
      std::string condName = path->toString();
      irStream << "; condName: " << condName << "\n";
      std::string condType = lookupVarType(condName);
      irStream << "; condType: " << condType << "\n";
      if (condType == "i1*") {
        std::string condTemp = createTemp();
        irStream << "  %" << condTemp << " = load i1, i1* " << cond << "\n";
        cond = "%" + condTemp;
      }
    }
    if (auto* group = dynamic_cast<GroupedExpressionNode*>(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get())) {
      if (auto* path = dynamic_cast<PathExpressionNode*>(group->expression.get())) {
        std::string condName = path->toString();
        irStream << "; condName: " << condName << "\n";
        std::string condType = lookupVarType(condName);
        irStream << "; condType: " << condType << "\n";
        if (condType == "i1*") {
          std::string condTemp = createTemp();
          irStream << "  %" << condTemp << " = load i1, i1* " << cond << "\n";
          cond = "%" + condTemp;
        }
      }
    }
  } else {
    // 其他条件类型，暂时不支持
    error("Unsupported condition type in if");
    return "";
  }

  std::string thenLabel = createLabel();
  std::string elseLabel = createLabel();

  // 检查是否所有分支都一定会return
  bool allBranchesWillReturn = willReturn(node->block_expression.get());
  if (!allBranchesWillReturn) {
    //irStream << "; cannot return in main block\n";
  }
  if (node->else_block) {
    if (!willReturn(node->else_block.get())) {
      //irStream << "; cannot return in else_block\n";
    }
    allBranchesWillReturn &= willReturn(node->else_block.get());
  } else if (node->else_if) {
    allBranchesWillReturn &= willReturnInExpression(node->else_if.get());
  } else {
    //irStream << "; having no else block or else if\n";
    allBranchesWillReturn = false;
  }
  std::string endLabel = allBranchesWillReturn ? "" : createLabel();
  //irStream << "; end label: " << endLabel << '\n';

  irStream << "  br i1 " << cond << ", label %" << thenLabel << ", label %" << elseLabel << "\n";

  // 总是生成 phi，如果需要
  std::string thenValue = "";
  std::string elseValue = "";
  std::string phiValue = "";

  irStream << thenLabel << ":\n";
  irStream << "  ; then branch\n";
  thenValue = visit(node->block_expression.get());
  if (!allBranchesWillReturn) {
    irStream << "  br label %" << endLabel << "\n";
  }

  irStream << elseLabel << ":\n";
  irStream << "  ; else branch\n";
  if (node->else_block) {
    elseValue = visit(node->else_block.get());
    if (!allBranchesWillReturn) {
      irStream << "  br label %" << endLabel << "\n";
    }
  } else if (node->else_if) {
    elseValue = visit(node->else_if.get());
    if (!allBranchesWillReturn) {
      irStream << "  br label %" << endLabel << "\n";
    }
  } else {
    // no else, always br
    if (!allBranchesWillReturn) {
      irStream << "  br label %" << endLabel << "\n";
    }
  }

  if (!allBranchesWillReturn) {
    irStream << endLabel << ":\n";
    if (!thenValue.empty() && !elseValue.empty()) {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = phi " << currentRetType << " [ " << thenValue << ", %" << thenLabel << " ], [ " << elseValue << ", %" << elseLabel << " ]\n";
      phiValue = "%" + temp;
    }
  }

  // 如果在函数体中，store phiValue 到 returnVar 并 br
  if (inFunctionBody) {
    if (!phiValue.empty()) {
      irStream << "  store " << expandStructType(currentRetType) << " " << phiValue << ", " << expandStructType(currentRetType) << "* %" << returnVar << "\n";
      irStream << "  br label %" << returnLabel << "\n";
    }
    return "";
  } else {
    if (inFunctionBody) {
      if (!phiValue.empty()) {
        irStream << "  store " << expandStructType(currentRetType) << " " << phiValue << ", " << expandStructType(currentRetType) << "* %" << returnVar << "\n";
        irStream << "  br label %" << returnLabel << "\n";
      }
      return "";
    } else {
      return phiValue;
    }
  }
}

void IRGenerator::visit(ItemNode* node) {
    if (auto* func = dynamic_cast<FunctionNode*>(node)) {
        visit(func);
    } else if (auto* struct_ = dynamic_cast<StructStructNode*>(node)) {
        visit(struct_);
    } else if (auto* tuple = dynamic_cast<TupleStructNode*>(node)) {
        visit(tuple);
    } else if (auto* enum_ = dynamic_cast<EnumerationNode*>(node)) {
        visit(enum_);
    } else if (auto* const_ = dynamic_cast<ConstantItemNode*>(node)) {
        visit(const_);
    } else if (auto* impl = dynamic_cast<InherentImplNode*>(node)) {
        visit(impl);
    } else if (auto* traitImpl = dynamic_cast<TraitImplNode*>(node)) {
        visit(traitImpl);
    }
    // 添加其他Item类型
}

void IRGenerator::visit(FunctionNode* node) {
  std::string funcName = node->identifier;
  std::string retType = node->return_type ? expandStructType(toIRType(node->return_type->type.get())) : "void";
  if (funcName == "main") retType = "i32";
  currentRetType = retType;

    // 函数签名
    std::string params = "";
    std::vector<std::pair<std::string, std::string>> paramList; // paramType, paramName
    std::vector<std::pair<std::string, std::string>> structList; // structName, baseName

    if (node->function_parameter) {
        // 处理 self_param
        if (node->function_parameter->self_param) {
            std::string selfType;
            if (node->impl_type_name) {
              if (std::holds_alternative<std::unique_ptr<ShorthandSelf>>(node->function_parameter->self_param->self)) {
                auto& ss = std::get<std::unique_ptr<ShorthandSelf>>(node->function_parameter->self_param->self);
                if (ss->if_prefix) {
                  selfType = "%" + *node->impl_type_name + "*";
                } else {
                  selfType = "%" + *node->impl_type_name;
                }
              } else {
                auto& ts = std::get<std::unique_ptr<TypedSelf>>(node->function_parameter->self_param->self);
                selfType = toIRType(ts->type.get());
                if (ts->if_mut) {
                  selfType += "*";
                }
              }
            } else {
              selfType = "i8*"; // 默认
            }
            std::string selfName = "self";
            if (selfType[0] == '%' && selfType.back() != '*') {
                std::string name = selfType.substr(1);
                //irStream << "; type of self: " << name << '\n';
                auto it = structFields.find(name);
                if (it != structFields.end()) {
                    structList.push_back({name, selfName});
                    for (size_t j = 0; j < it->second.size(); ++j) {
                        std::string fieldName = selfName + "." + it->second[j].first;
                        //irStream << "; inserting " << fieldName << " into param list\n";
                        paramList.emplace_back(it->second[j].second, fieldName);
                    }
                } else {
                    paramList.emplace_back(selfType, selfName);
                }
            } else {
                //irStream << "; type of self: " << selfType << '\n';
                paramList.emplace_back(selfType, selfName);
            }
        }

        // 处理 function_params
        for (size_t i = 0; i < node->function_parameter->function_params.size(); ++i) {
            const auto& param = node->function_parameter->function_params[i];
            std::string paramType;
            std::string paramName;
            if (std::holds_alternative<std::unique_ptr<FunctionParamPattern>>(param->info)) {
                const auto& fpp = std::get<std::unique_ptr<FunctionParamPattern>>(param->info);
                if (fpp->type) {
                    if (auto* ref = dynamic_cast<ReferenceTypeNode*>(fpp->type.get())) {
                        paramType = toIRType(ref->type.get()) + "*";
                    } else {
                        paramType = toIRType(fpp->type.get());
                    }
                } else {
                    paramType = "i32";
                }
                //irStream << "； the " << i << "th param type in function node: " << paramType << '\n';
                paramName = fpp->pattern ? fpp->pattern->toString() : "arg" + std::to_string(i);
            } else if (std::holds_alternative<std::unique_ptr<TypeNode>>(param->info)) {
                const auto& typeNode = std::get<std::unique_ptr<TypeNode>>(param->info);
                if (auto* ref = dynamic_cast<ReferenceTypeNode*>(typeNode.get())) {
                    paramType = toIRType(ref->type.get()) + "*";
                } else {
                    paramType = toIRType(typeNode.get());
                }
                paramName = "arg" + std::to_string(i);
            } else if (std::holds_alternative<std::unique_ptr<ellipsis>>(param->info)) {
                // 跳过 ellipsis
                continue;
            }
                if (paramType[0] == '%') {
                    std::string name = paramType.substr(1);
                    auto it = structFields.find(name);
                    if (it != structFields.end()) {
                        structList.push_back({name, paramName});
                        for (size_t j = 0; j < it->second.size(); ++j) {
                            std::string fieldName = paramName + "." + it->second[j].first;
                            //irStream << "; inserting " << fieldName << " into param list\n";
                            paramList.emplace_back(it->second[j].second, fieldName);
                        }
                    } else {
                        paramList.emplace_back(paramType, paramName);
                    }
                } else {
                    paramList.emplace_back(paramType, paramName);
                }
        }
    }

    for (size_t i = 0; i < paramList.size(); ++i) {
        params += expandStructType(paramList[i].first) + " %" + paramList[i].second;
        if (i + 1 < paramList.size()) params += ", ";
    }

    irStream << "; Function: " << funcName << "\n";
    irStream << "define " << retType << " " << llvmGlobalRef(funcName) << "(" << params << ") {\n";

    returnLabel = createLabel();
    if (retType != "void" && funcName != "main") {
        returnVar = createTemp();
        irStream << "  %" << returnVar << " = alloca " << expandStructType(retType) << "\n";
    }

    // basic blocks 和 expression visitor
    enterScope();  // 函数作用域

    // 局部变量：参数处理
    //irStream << "; begin processing param\n";
    size_t index = 0;
    for (const auto& p : paramList) {
        //irStream << "; adding the " << index << "th parameter of function to varTypeScope\n";
        index++;
        std::string paramType = p.first;
        std::string paramName = p.second;
        size_t dotPos = paramName.find('.');
        if (dotPos != std::string::npos) {
            // 展开的参数，直接使用参数名作为符号（值传递）
            symbolScopes.back()[paramName] = paramName;
            varTypeScopes.back()[paramName] = paramType;
            //irStream << "; type: " << paramType << '\n';
            //irStream << "; name: " << paramName << '\n';
        } else {
            // 对于值参数，alloc
            if (paramType.back() != '*') {
                std::string temp = createTemp();
                symbolScopes.back()[paramName] = temp;
                varTypeScopes.back()[paramName] = paramType;
                irStream << "  %" << temp << " = alloca " << expandStructType(paramType) << "\n";
                irStream << "  store " << expandStructType(paramType) << " %" << paramName << ", " << expandStructType(paramType) << "* %" << temp << "\n";
            } else {
                // 指针参数，直接使用
                symbolScopes.back()[paramName] = paramName;
                varTypeScopes.back()[paramName] = paramType;
            }
        }
    }

    // 重建结构体
    for (const auto& [structName, baseName] : structList) {
        //irStream << "; reconstructing struct: " << structName << " for " << baseName << '\n';
        std::string structType = "%" + structName;
        auto it = structFields.find(structName);
        if (it != structFields.end()) {
            std::string structValue = "undef";
            for (size_t j = 0; j < it->second.size(); ++j) {
                std::string fieldName = it->second[j].first;
                std::string paramName = baseName + "." + fieldName;
                std::string temp = createTemp();
                irStream << "  %" << temp << " = insertvalue " << expandStructType(structType) << " " << structValue << ", " << expandStructType(it->second[j].second) << " %" << paramName << ", " << j << "\n";
                structValue = "%" + temp;
            }
            symbolScopes.back()[baseName] = structValue.substr(1);
            varTypeScopes.back()[baseName] = structType;
            //irStream << "; struct name: " << baseName << '\n';
            //irStream << "; struct type: " << structType << '\n';
        }
    }

    //irStream << "; finish processing param\n";
    inFunctionBody = true;
    if (node->block_expression) {
        visit(node->block_expression.get());  // 生成代码，包括 store 和 br
    }
    inFunctionBody = false;
    exitScope();

    // 生成统一的 return block
    if (retType != "void") {
        if (funcName == "main") {
            irStream << "  ret i32 0\n";
        } else {
            irStream << returnLabel << ":\n";
            std::string val = createTemp();
            irStream << "  %" << val << " = load " << expandStructType(retType) << ", " << expandStructType(retType) << "* %" << returnVar << "\n";
            irStream << "  ret " << expandStructType(retType) << " %" << val << "\n";
        }
    } else {
        irStream << "  ret void\n";
    }

    irStream << "}\n\n";
}

void IRGenerator::visit(StructStructNode* node) {
  irStream << "%" << node->identifier << " = type { ";
  if (node->struct_fields) {
    for (size_t i = 0; i < node->struct_fields->struct_fields.size(); ++i) {
      const auto& field = node->struct_fields->struct_fields[i];
      std::string fieldType = toIRType(field->type.get());
      irStream << fieldType;
      if (i + 1 < node->struct_fields->struct_fields.size()) irStream << ", ";
    }
  }
  irStream << " }\n\n";
}

void IRGenerator::visit(TupleStructNode* node) {
  irStream << "%" << node->identifier << " = type { ";
  if (node->tuple_fields) {
    for (const auto& field : node->tuple_fields->tuple_fields) {
      std::string typeStr = toIRType(field->type.get());
      irStream << typeStr << ", ";
    }
  }
  irStream << "}\n\n";
}

void IRGenerator::visit(EnumerationNode* node) {
  irStream << "; Enum " << node->identifier << "\n";
}

void IRGenerator::visit(ConstantItemNode* node) {
  irStream << "; visiting constant item\n";
  if (node->identifier && node->expression) {
    std::string value;
    if (auto* lit = dynamic_cast<LiteralExpressionNode*>(node->expression.get())) {
      value = lit->toString();
    }
    constantTable[*node->identifier] = value;
  }
}

void IRGenerator::visit(InherentImplNode* node) {
  //irStream << "; Inherent impl for " << node->type->toString() << "\n";
  const std::string implTypePrefix = sanitizeImplTypePrefix(node->type ? node->type->toString() : "");
  for (auto& assoc : node->associated_item) {
    std::visit([&](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, std::unique_ptr<ConstantItemNode>>) {
        visit(arg.get());
      } else if constexpr (std::is_same_v<T, std::unique_ptr<FunctionNode>>) {
        // 将 impl 内的方法改名为 Type_FuncName，避免不同类型方法重名冲突。
        FunctionNode* fn = arg.get();
        const std::string oldName = fn->identifier;
        if (!implTypePrefix.empty()) {
          fn->identifier = implTypePrefix + "_" + oldName;
        }
        visit(fn);
        fn->identifier = oldName;
      }
    }, assoc->associated_item);
  }
}

void IRGenerator::visit(TraitImplNode* node) {
  irStream << "; Trait impl\n";
}

void IRGenerator::visit(StatementNode* node) {
  if (node->type == LETSTATEMENT && node->let_statement) {
    visit(node->let_statement.get());
  } else if (node->type == EXPRESSIONSTATEMENT && node->expr_statement) {
    visit(node->expr_statement.get());
  } else if (node->type == ITEM && node->item) {
    visit(node->item.get());
  }
}

void IRGenerator::visit(LetStatement* node) {
  std::string varName = node->pattern->toString(); // 简化
  std::string type;
  std::string typeNameStr;
  if (node->type) {
    typeNameStr = node->type->toString();
    if (auto* ref = dynamic_cast<ReferenceTypeNode*>(node->type.get())) {
      std::string innerType = toIRType(ref->type.get());
      // check if u32 and overflow
      if (auto* typePath = dynamic_cast<TypePathNode*>(ref->type.get())) {
        std::string path = typePath->type_path->toString();
        if (path == "u32") {
          innerType = "i64";
        }
      }
      type = innerType + "*";
    } else {
      type = toIRType(node->type.get());
      if (auto* typePath = dynamic_cast<TypePathNode*>(node->type.get())) {
        std::string path = typePath->type_path->toString();
        if (path == "u32") {
          type = "i64";
        }
      }
    }
  } else {
    type = "i32";
    typeNameStr = "i32";
  }
  std::string temp;
  if (loopPreAlloc.find(varName) != loopPreAlloc.end()) {
    temp = loopPreAlloc[varName];
  } else {
    temp = createTemp();
    irStream << "; var name in let statement: " << varName << '\n';
    irStream << "; var type in let statement: " << type + "*" << '\n';
    irStream << "; var address in let statement: " << temp << '\n';
    bool isArray = type.find('[') != std::string::npos;
    irStream << "  %" << temp << " = alloca " << expandStructType(type) << "\n";
  }
  if (node->expression) {
    // 检查 rhs 是否是 if expression
      // 普通 rhs
      std::string value = visit_in_rhs(node->expression.get());
      std::string rhsType = getLhsType(node->expression.get());
      if (type == "i64" && rhsType == "i32") {
        rhsType = "i64";
        std::string rhsTemp = createTemp();
        irStream << "  %" << rhsTemp << " = sext i32 " << value << " to i64\n";
        value = "%" + rhsTemp;
      }
      if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression.get())) {
        std::string letName = path->toString();
        std::string letType = lookupVarType(letName);
        if (letType == type + "*") {
          std::string letTemp = createTemp();
          irStream << "  %" << letTemp << " = load " << type << ", " << type << "* " << value << '\n';
          value = "%" + letTemp;
        }
      }
      irStream << "  store " << expandStructType(type) << " " << value << ", " << expandStructType(type) << "* %" << temp << "\n";
    }
  // 更新 scopes 在计算 value 之后
  symbolScopes.back()[varName] = temp;
  varTypeScopes.back()[varName] = type + "*";
  typeNameScopes.back()[varName] = typeNameStr;
  isLetDefinedScopes.back()[varName] = true;
}

void IRGenerator::visit(ExpressionStatement* node) {
  visit(node->expression.get());
}

std::string IRGenerator::createTemp() {
  return "t" + std::to_string(tempCounter++);
}

std::string IRGenerator::createLabel() {
  return "L" + std::to_string(labelCounter++);
}

std::string IRGenerator::emitAlloca(const std::string& type, const std::string& name) {
  std::string temp = name.empty() ? createTemp() : name;
  irStream << "  %" << temp << " = alloca " << expandStructType(type) << "\n";
  return "%" + temp;
}

std::string IRGenerator::emitLoad(const std::string& type, const std::string& ptr) {
  std::string temp = createTemp();
  irStream << "  %" << temp << " = load " << expandStructType(type) << ", " << expandStructType(type) << "* " << ptr << "\n";
  return "%" + temp;
}

std::string IRGenerator::emitStore(const std::string& value, const std::string& ptr) {
  irStream << "  store i32 " << value << ", i32* " << ptr << "\n";
  return "";
}

std::string IRGenerator::emitBinaryOp(const std::string& op, const std::string& lhs, const std::string& rhs, const std::string& type) {
  std::string temp = createTemp();
  irStream << "  %" << temp << " = " << op << " " << expandStructType(type) << " " << lhs << ", " << rhs << "\n";
  return "%" + temp;
}

void IRGenerator::error(const std::string& msg) {
  std::cerr << "IR Generation Error: " << msg << std::endl;
  throw std::runtime_error(msg);
}

std::string IRGenerator::getCurrentIR() {
  return irStream.str();
}

void IRGenerator::preScan(const std::vector<std::unique_ptr<ASTNode>>& ast) {
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (auto* const_ = dynamic_cast<ConstantItemNode*>(item)) {
        visit(const_);
      }
    }
  }
  for (const auto& node : ast) {
    if (auto* item = dynamic_cast<ItemNode*>(node.get())) {
      if (auto* func = dynamic_cast<FunctionNode*>(item)) {
        // 记录函数原型
        std::string retType = func->return_type ? toIRType(func->return_type->type.get()) : "void";
        if (func->identifier == "main") retType = "i32";
        //irStream << "; return type of function: " << func->identifier << " : " << retType << '\n';
        functionTable[func->identifier] = retType;
        // 记录参数类型
        std::vector<std::string> paramTypes;
        if (func->function_parameter) {
          if (func->function_parameter->self_param) {
            irStream << "; having self in function: " << func->identifier << "\n";
            std::string selfType;
            if (func->impl_type_name) {
              if (std::holds_alternative<std::unique_ptr<ShorthandSelf>>(func->function_parameter->self_param->self)) {
                auto& ss = std::get<std::unique_ptr<ShorthandSelf>>(func->function_parameter->self_param->self);
                if (ss->if_prefix) {
                  selfType = "%" + *func->impl_type_name + "*";
                } else {
                  selfType = "%" + *func->impl_type_name;
                }
              } else {
                auto& ts = std::get<std::unique_ptr<TypedSelf>>(func->function_parameter->self_param->self);
                if (auto* ref = dynamic_cast<ReferenceTypeNode*>(ts->type.get())) {
                  selfType = toIRType(ref->type.get()) + "*";
                } else {
                  selfType = toIRType(ts->type.get());
                }
                if (ts->if_mut) {
                  selfType += "*";
                }
              }
            } else {
              selfType = "i8*";
            }
            paramTypes.push_back(selfType);
          }
          for (size_t i = 0; i < func->function_parameter->function_params.size(); ++i) {
            const auto& param = func->function_parameter->function_params[i];
            std::string paramType;
            if (std::holds_alternative<std::unique_ptr<FunctionParamPattern>>(param->info)) {
              const auto& fpp = std::get<std::unique_ptr<FunctionParamPattern>>(param->info);
              if (fpp->type) {
                if (auto* ref = dynamic_cast<ReferenceTypeNode*>(fpp->type.get())) {
                  paramType = toIRType(ref->type.get()) + "*";
                } else {
                  paramType = toIRType(fpp->type.get());
                }
              } else {
                paramType = "i32";
              }
            } else if (std::holds_alternative<std::unique_ptr<TypeNode>>(param->info)) {
              const auto& typeNode = std::get<std::unique_ptr<TypeNode>>(param->info);
              if (auto* ref = dynamic_cast<ReferenceTypeNode*>(typeNode.get())) {
                paramType = toIRType(ref->type.get()) + "*";
              } else {
                paramType = toIRType(typeNode.get());
              }
            }
            paramTypes.push_back(paramType);
          }
        }
        paramTypesTable[func->identifier] = paramTypes;
        //irStream << "; func name: " << func->identifier << '\n';
        for (size_t i = 0; i < paramTypesTable[func->identifier].size(); i++) {
          //irStream << "; the "<< i << "th param: " << paramTypesTable[func->identifier][i] << '\n';
        }
      } else if (auto* impl = dynamic_cast<InherentImplNode*>(item)) {
        // 处理 impl 中的函数
        const std::string implTypePrefix = sanitizeImplTypePrefix(impl->type ? impl->type->toString() : "");
        for (auto& assoc : impl->associated_item) {
          std::visit([&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::unique_ptr<FunctionNode>>) {
              auto* func = arg.get();
              const std::string mangledName = implTypePrefix.empty() ? func->identifier : (implTypePrefix + "_" + func->identifier);
              std::string retType = func->return_type ? toIRType(func->return_type->type.get()) : "void";
              functionTable[mangledName] = retType;
              std::vector<std::string> paramTypes;
              if (func->function_parameter) {
                if (func->function_parameter->self_param) {
                  std::string selfType;
                  if (func->impl_type_name) {
                    if (std::holds_alternative<std::unique_ptr<ShorthandSelf>>(func->function_parameter->self_param->self)) {
                      auto& ss = std::get<std::unique_ptr<ShorthandSelf>>(func->function_parameter->self_param->self);
                      if (ss->if_prefix) {
                        selfType = "%" + *func->impl_type_name + "*";
                      } else {
                        selfType = "%" + *func->impl_type_name;
                      }
                    } else {
                      auto& ts = std::get<std::unique_ptr<TypedSelf>>(func->function_parameter->self_param->self);
                      if (auto* ref = dynamic_cast<ReferenceTypeNode*>(ts->type.get())) {
                        selfType = toIRType(ref->type.get()) + "*";
                      } else {
                        selfType = toIRType(ts->type.get());
                      }
                      if (ts->if_mut) {
                        selfType += "*";
                      }
                    }
                  } else {
                    selfType = "i8*";
                  }
                  paramTypes.push_back(selfType);
                }
                for (size_t i = 0; i < func->function_parameter->function_params.size(); ++i) {
                  const auto& param = func->function_parameter->function_params[i];
                  std::string paramType;
                  if (std::holds_alternative<std::unique_ptr<FunctionParamPattern>>(param->info)) {
                    const auto& fpp = std::get<std::unique_ptr<FunctionParamPattern>>(param->info);
                    if (fpp->type) {
                      if (auto* ref = dynamic_cast<ReferenceTypeNode*>(fpp->type.get())) {
                        paramType = toIRType(ref->type.get()) + "*";
                      } else {
                        paramType = toIRType(fpp->type.get());
                      }
                    } else {
                      paramType = "i32";
                    }
                  } else if (std::holds_alternative<std::unique_ptr<TypeNode>>(param->info)) {
                    const auto& typeNode = std::get<std::unique_ptr<TypeNode>>(param->info);
                    if (auto* ref = dynamic_cast<ReferenceTypeNode*>(typeNode.get())) {
                      paramType = toIRType(ref->type.get()) + "*";
                    } else {
                      paramType = toIRType(typeNode.get());
                    }
                  }
                  paramTypes.push_back(paramType);
                }
              }
              paramTypesTable[mangledName] = paramTypes;
              //irStream << "; impl func name: " << func->identifier << '\n';
              for (size_t i = 0; i < paramTypesTable[mangledName].size(); i++) {
                //irStream << "; the "<< i << "th param: " << paramTypesTable[func->identifier][i] << '\n';
              }
            }
          }, assoc->associated_item);
        }
      } else if (auto* struct_ = dynamic_cast<StructStructNode*>(item)) {
        // 记录 struct 字段
        //irStream << "; recording elements of struct " << struct_->identifier << ": \n";
        std::vector<std::pair<std::string, std::string>> fields;
        if (struct_->struct_fields) {
          size_t index = 0;
          for (const auto& field : struct_->struct_fields->struct_fields) {
            std::string fieldType = toIRType(field->type.get());
            //irStream << "; the " << index << "th element in the struct: " << fieldType << '\n';
            index++;
            fields.emplace_back(field->identifier, fieldType);
          }
        }
        structFields[struct_->identifier] = fields;
        typeTable[struct_->identifier] = "%" + struct_->identifier;
      }
    }
  }
}

void IRGenerator::generateStructTypes() {
  for (const auto& [name, fields] : structFields) {
    irStream << "%" << name << " = type { ";
    for (size_t i = 0; i < fields.size(); ++i) {
      irStream << fields[i].second;
      if (i + 1 < fields.size()) irStream << ", ";
    }
    irStream << " }\n";
  }
  if (!structFields.empty()) irStream << "\n";
}

void IRGenerator::enterScope() {
  symbolScopes.push_back({});
  varTypeScopes.push_back({});
  fieldScopes.push_back({});
  fieldTypeScopes.push_back({});
  isLetDefinedScopes.push_back({});
  typeNameScopes.push_back({});
}

void IRGenerator::exitScope() {
  if (!symbolScopes.empty()) symbolScopes.pop_back();
  if (!varTypeScopes.empty()) varTypeScopes.pop_back();
  if (!fieldScopes.empty()) fieldScopes.pop_back();
  if (!fieldTypeScopes.empty()) fieldTypeScopes.pop_back();
  if (!isLetDefinedScopes.empty()) isLetDefinedScopes.pop_back();
  if (!typeNameScopes.empty()) typeNameScopes.pop_back();
}

std::string IRGenerator::lookupSymbol(const std::string& name) {
  size_t dotPos = name.find('.');
  if (dotPos != std::string::npos) {
    std::string base = name.substr(0, dotPos);
    std::string field = name.substr(dotPos + 1);
    for (auto it = fieldScopes.rbegin(); it != fieldScopes.rend(); ++it) {
      if (it->count(base) && (*it)[base].count(field)) {
        return (*it)[base][field];
      }
    }
  }
  for (auto it = symbolScopes.rbegin(); it != symbolScopes.rend(); ++it) {
    if (it->find(name) != it->end()) return (*it)[name];
  }
  return "";
}

std::string IRGenerator::lookupVarType(const std::string& name) {
   //irStream << "; looking up var type of " << name << '\n';
   size_t dotPos = name.find('.');
   if (dotPos != std::string::npos) {
     std::string base = name.substr(0, dotPos);
     std::string field = name.substr(dotPos + 1);
     for (auto it = fieldTypeScopes.rbegin(); it != fieldTypeScopes.rend(); ++it) {
       if (it->count(base) && (*it)[base].count(field)) {
         return (*it)[base][field];
       }
     }
   }
   for (auto it = varTypeScopes.rbegin(); it != varTypeScopes.rend(); ++it) {
     if (it->find(name) != it->end()) {
       irStream << "; type of " << name << ": " << (*it)[name] << " in func lookupVarType\n";
       return (*it)[name];
     }
   }
   return "";
}

std::string IRGenerator::getTypeName(const std::string& name) {
  for (auto it = typeNameScopes.rbegin(); it != typeNameScopes.rend(); ++it) {
    if (it->find(name) != it->end()) return (*it)[name];
  }
  return "";
}

std::string IRGenerator::getLhsAddress(ExpressionNode* lhs) {
  //irStream << "; getting lhs address\n";
  if (auto* path = dynamic_cast<PathExpressionNode*>(lhs)) {
    std::string name = path->toString();
    //irStream << "; name of pathexpression in getting address: " << name << '\n';
    if (constantTable.find(name) != constantTable.end()) {
      // 常量，不能取地址
      return "";
    } else {
      return "%" + lookupSymbol(name);
    }
  } else if (auto* deref = dynamic_cast<DereferenceExpressionNode*>(lhs)) {
    // *expr, expr 应该是指针，地址是 expr 的值
    return visit(deref->expression.get());
  } else if (auto* field = dynamic_cast<FieldExpressionNode*>(lhs)) {
    // 检查是否是 register 字段
    irStream << "; getting lhsaddress of field expression\n";
    if (auto* path = dynamic_cast<PathExpressionNode*>(field->expression.get())) {
      std::string baseName = path->toString();
      std::string fieldSym = lookupSymbol(baseName + "." + field->identifier.id);
      if (!fieldSym.empty() && fieldSym == baseName + "." + field->identifier.id) {
        // 是 register，不能取地址
        return "";
      }
    }
    // expr.field, 生成 getelementptr
    //irStream << "; getting address of base expr in field expression\n";
    std::string baseAddr = getLhsAddress(field->expression.get());
    //irStream << "; address of base expr in field expression: " << baseAddr << '\n';
    std::string baseType = getLhsTypeWithStar(field->expression.get());
    irStream << "; type of base expr in field expression: " << baseType << '\n';
    if (baseType == "") {
      if (auto* path = dynamic_cast<PathExpressionNode*>(field->expression.get())) {
        //irStream << "; trying to get type in struct field\n";
        std::string name = path->toString();
        //irStream <<"; finding " << name << " in struct field\n";
        std::string fieldName = name + "." + field->identifier.id;
        //irStream << "; getting type of struct item: " << fieldName << '\n';
        baseType = lookupVarType(fieldName);
        baseAddr = "%" + lookupSymbol(fieldName);
        return baseAddr;
      }
    }
    size_t starPos = baseType.find('*');
    if (starPos == std::string::npos) {
      // base is struct value, need to alloca temporary
      irStream << "; allocating temporary struct for type: " << baseType << "\n";
      std::string tempPtr = createTemp();
      irStream << "  %" << tempPtr << " = alloca " << expandStructType(baseType) << "\n";
      irStream << "  store " << expandStructType(baseType) << " " << baseAddr << ", " << expandStructType(baseType) << "* %" << tempPtr << "\n";
      baseAddr = "%" + tempPtr;
      baseType += "*";
      starPos = baseType.size() - 1;
    }
    std::string structName = baseType.substr(1, starPos - 1);
    auto it = structFields.find(structName);
    if (it == structFields.end()) {
      error("Unknown struct " + structName);
      return "";
    }
    int index = -1;
    for (int i = 0; i < it->second.size(); ++i) {
      if (it->second[i].first == field->identifier.id) {
        index = i;
        break;
      }
    }
    if (index == -1) {
      error("Field not found " + field->identifier.id);
      return "";
    }
    std::string fieldPtr = createTemp();
    irStream << "  %" << fieldPtr << " = getelementptr " << expandStructType("%" + structName) << ", " << expandStructType("%" + structName) << "* " << baseAddr << ", i32 0, i32 " << index << "\n";
    return "%" + fieldPtr;
  } else if (auto* index = dynamic_cast<IndexExpressionNode*>(lhs)) {
    irStream << "; getting address of index expression\n";
    std::string baseAddr = getLhsAddress(index->base.get());
    //irStream << "; base address in indexpression: " << baseAddr << '\n';
    std::string baseType = getLhsType(index->base.get());
    //irStream << "; base type in indexpression: " << baseType << '\n';
    std::string idxVal = visit(index->index.get());
    //irStream << "; index value in getting address of index expression: " << indexValue << '\n';
    std::string temp = createTemp();

    if (auto* path = dynamic_cast<PathExpressionNode*>(index->index.get())) {
      std::string idx_path = path->toString();
      std::string idx_type = lookupVarType(idx_path);
      std::string idx_addr = lookupSymbol(idx_path);
      if (idx_type == "i32*") {
        std::string idx_temp = createTemp();
        irStream << "  %" << idx_temp << " = load i32, i32* %" << idx_addr << '\n';
        idxVal = "%" + idx_temp;
      }
    }
    
    if (baseType.size() > 2 && baseType[0] == '[' && baseType.back() != '*') {
      irStream << "  %" << temp << " = getelementptr " << expandStructType(baseType) << ", " << expandStructType(baseType) << "* " << baseAddr << ", i32 0, i32 " << idxVal << "\n";
      //irStream << "; getting lhs address: %" << temp << '\n';
      return "%" + temp;
    }
   
    if (baseType.size() > 3 && baseType[0] == '[' && baseType.back() == '*') {
      std::string arrayType = baseType.substr(0, baseType.size() - 1);
      irStream << "  %" << temp << " = getelementptr " << expandStructType(arrayType) << ", " << expandStructType(arrayType) << "* " << baseAddr << ", i32 0, i32 " << idxVal << "\n";
      //irStream << "; getting lhs address: %" << temp << '\n';
      return "%" + temp;
    }
   
    std::string elementType = getElementType(baseType);
    std::string loadedPtr = createTemp();
    irStream << "  %" << loadedPtr << " = load " << expandStructType(baseType) << ", " << expandStructType(baseType) << "* " << baseAddr << "\n";
    irStream << "  %" << temp << " = getelementptr " << expandStructType(elementType) << ", " << expandStructType(elementType) << "* %" << loadedPtr << ", i32 " << idxVal << "\n";
    return "%" + temp;
  } else if (auto* array = dynamic_cast<ArrayExpressionNode*>(lhs)) {
    // For array expression, generate the value, alloc temp, store, return pointer
    std::string value = visit(array);
    std::string arrayType = getLhsType(array);
    std::string tempPtr = createTemp();
    irStream << "  %" << tempPtr << " = alloca " << expandStructType(arrayType) << "\n";
    irStream << "  store " << expandStructType(arrayType) << " " << value << ", " << expandStructType(arrayType) << "* %" << tempPtr << "\n";
    return "%" + tempPtr;
  } else {
    error("Unsupported lhs type in assignment");
    return "";
  }
}

std::string IRGenerator::getElementType(const std::string& typeStr) {
  //irStream << "; getting element type\n";
  bool isPtr = false;
  std::string base = typeStr;
  if (!base.empty() && base.back() == '*') {
    isPtr = true;
    base = base.substr(0, base.size() - 1);
  }
  std::string elem;
  if (base.size() > 2 && base[0] == '[') {
    // 数组，elem是元素类型
    size_t xPos = base.find(" x ");
    if (xPos != std::string::npos) {
      elem = base.substr(xPos + 3, base.size() - xPos - 4); // remove ]
    } else {
      elem = base;
    }
    // 对于数组，不加*，因为索引数组返回元素类型，无论base是否是指针
  } else {
    elem = base;
    if (isPtr) {
      elem += '*';
    }
  }
  //irStream << "; elem type: " << elem << '\n';
  return elem;
}

std::string IRGenerator::getLhsType(ExpressionNode* lhs) {
  irStream << "; getting lhs type\n";
  if (auto* lit = dynamic_cast<LiteralExpressionNode*>(lhs)) {
    // 对于literal，返回其类型
    if (std::holds_alternative<std::unique_ptr<bool>>(lit->literal)) {
      return "i1";
    } else {
      // check if large number
      if (std::holds_alternative<std::unique_ptr<integer_literal>>(lit->literal)) {
        std::string value = std::get<std::unique_ptr<integer_literal>>(lit->literal)->value;
        try {
          long long val = std::stoll(value);
          if (val > 2147483647LL || val < -2147483648LL) {
            return "i64";
          }
        } catch (...) {
        }
      }
      return "i32";
    }
  } else if (auto* group = dynamic_cast<GroupedExpressionNode*>(lhs)) {
    //irStream << "; getting type of grouped expression\n";
    return getLhsType(group->expression.get());
  } else if (auto* path = dynamic_cast<PathExpressionNode*>(lhs)) {
    std::string name = path->toString();
    irStream << "; name of pathexpression in getting type : " << name << '\n';
    if (constantTable.find(name) != constantTable.end()) {
      // 常量
      std::string value = constantTable[name];
      if (value[0] == '"') {
        return "i8*";
      } else {
        return "i32";
      }
    } else {
      std::string lhsType = lookupVarType(name);
      irStream << "; result of getLhsType of PathExpression: " << lhsType << '\n';
      return lhsType;
    }
  } else if (auto* cast = dynamic_cast<TypeCastExpressionNode*>(lhs)) {
    std::string dstType = toIRType(cast->type.get());
    bool isDstU32 = false;
    if (auto* typePath = dynamic_cast<TypePathNode*>(cast->type.get())) {
      if (typePath->type_path->toString() == "u32") {
        isDstU32 = true;
      }
    }
    if (isDstU32) {
      irStream << "; type of typecastexpression: i64\n";
      return "i64";
    } else {
      irStream << "; type of typecastexpression: " << dstType << '\n';
      return dstType;
    }
  } else if (auto* opExpr = dynamic_cast<OperatorExpressionNode*>(lhs)) {
    // 让类型推导与 visit(OperatorExpressionNode*) 保持一致
    return std::visit([&](auto&& arg) -> std::string {
      using T = std::decay_t<decltype(arg)>;
      if constexpr (std::is_same_v<T, std::unique_ptr<ArithmeticOrLogicalExpressionNode>>) {
        return "i32";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<ComparisonExpressionNode>>) {
        return "i1";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<AssignmentExpressionNode>>) {
        // assignment 表达式通常没有可用值，这里返回 void 以避免崩溃
        return "void";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<CompoundAssignmentExpressionNode>>) {
        return "void";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<BorrowExpressionNode>>) {
        return getLhsType(arg.get());
      } else {
        return "i32";
      }
    }, opExpr->operator_expression);
  } else if (dynamic_cast<ArithmeticOrLogicalExpressionNode*>(lhs)) {
    irStream << "; getting type of arithmetic expr\n";
    std::string lhsType = getLhsType(static_cast<ArithmeticOrLogicalExpressionNode*>(lhs)->expression1.get());
    std::string rhsType = getLhsType(static_cast<ArithmeticOrLogicalExpressionNode*>(lhs)->expression2.get());
    irStream << "; lhsType: " << lhsType << ", rhsType: " << rhsType << ", resultType: " << ((lhsType == "i64" || rhsType == "i64") ? "i64" : "i32") << '\n';
    return (lhsType == "i64" || rhsType == "i64" || lhsType == "i64*" || rhsType == "i64*") ? "i64" : "i32";
  } else if (dynamic_cast<ComparisonExpressionNode*>(lhs)) {
    return "i1";
  } else if (dynamic_cast<LazyBooleanExpressionNode*>(lhs)) {
    return "i1";
  } else if (auto* neg = dynamic_cast<NegationExpressionNode*>(lhs)) {
    // 当前 IR 里 NegationExpressionNode::BANG 也按 i32 来处理
    std::string res = getLhsType(neg->expression.get());
    if (res == "i1*") return "i1";
    if (res == "i32*") return "i32";
    return res;
  } else if (auto* deref = dynamic_cast<DereferenceExpressionNode*>(lhs)) {
    // *expr, 类型是 expr 类型去掉 *
    std::string exprType = getLhsType(deref->expression.get());
    if (exprType.back() == '*') {
      return exprType.substr(0, exprType.size() - 1);
    } else {
      error("Dereference on non-pointer type");
      return "";
    }
  } else if (auto* field = dynamic_cast<FieldExpressionNode*>(lhs)) {
    // 检查是否是 register 字段
    irStream << "; getting lhstype of field expression\n";
    if (auto* path = dynamic_cast<PathExpressionNode*>(field->expression.get())) {
      std::string baseName = path->toString();
      std::string fieldType = lookupVarType(baseName + "." + field->identifier.id);
      if (!fieldType.empty()) {
        return fieldType;
      }
    }
    irStream << "; getting base type of field expression\n";
    std::string baseType = getLhsTypeWithStar(field->expression.get());
    irStream << "; base type in fieldexpression: " << baseType << '\n';
    if (baseType == "") {
      if (auto* path = dynamic_cast<PathExpressionNode*>(field->expression.get())) {
        //irStream << "; trying to get type in struct field\n";
        std::string name = path->toString();
        //irStream <<"; finding " << name << " in struct field\n";
        std::string fieldName = name + "." + field->identifier.id;
        //irStream << "; getting type of struct item: " << fieldName << '\n';
        baseType = lookupVarType(fieldName);
        return baseType;
      }
    }
    size_t starPos = baseType.find('*');
    std::string structName;
    if (starPos != std::string::npos) {
      structName = baseType.substr(1, starPos - 1);
    } else {
      structName = baseType.substr(1);
    }
    auto it = structFields.find(structName);
    if (it == structFields.end()) {
      error("Unknown struct " + structName);
      return "";
    }
    int index = -1;
    for (int i = 0; i < it->second.size(); ++i) {
      if (it->second[i].first == field->identifier.id) {
        index = i;
        break;
      }
    }
    if (index == -1) {
      error("Field not found " + field->identifier.id);
      return "";
    }
    if (starPos != std::string::npos) {
      //irStream << "; field expression type: " << it->second[index].second << "\n";
      return it->second[index].second;
    } else {
      //irStream << "; field expression type: " << it->second[index].second << "\n";
      return it->second[index].second;
    }
  } else if (auto* index = dynamic_cast<IndexExpressionNode*>(lhs)) {
    // base[index], 类型是 base 的元素类型
    //irStream << "; getting type of index expression\n";
    std::string baseType = getLhsTypeWithStar(index->base.get());
    irStream << "; base type in index expression: " << baseType << "\n";
    std::string elementType = getElementType(baseType);
    irStream << "; element type in getting type of indexexpression: " << elementType << '\n';
    if (elementType != baseType) {
      // 是数组
      return elementType;
    } else if (baseType.back() == '*') {
      // 是指针
      return baseType.substr(0, baseType.size() - 1);
    } else {
      error("Index on non-array non-pointer type");
      return "";
    }
  } else if (auto* structExpr = dynamic_cast<StructExpressionNode*>(lhs)) {
    std::string structName = structExpr->pathin_expression->toString();
    return "%" + structName;
  } else if (auto* array = dynamic_cast<ArrayExpressionNode*>(lhs)) {
    // 数组类型，如 [3 x i32]*
    std::string elemType = "i32";
    if (!array->expressions.empty()) {
      elemType = getLhsType(array->expressions[0].get());
    }
    int size;
    if (array->type == ArrayExpressionType::REPEAT) {
      auto countOpt = evaluateConstant(array->expressions[1].get());
      if (!countOpt) {
        error("Array repeat count must be constant");
        return "";
      }
      size = *countOpt;
    } else {
      size = array->expressions.size();
    }
    return "[" + std::to_string(size) + " x " + elemType + "]";
  } else if (auto* cast = dynamic_cast<TypeCastExpressionNode*>(lhs)) {
    return toIRType(cast->type.get());
  } else if (auto* borrow = dynamic_cast<BorrowExpressionNode*>(lhs)) {
    // &expr 的类型是 expr 类型再加一层指针
    irStream << "; getting type of borrow expression\n";
    std::string inner = getLhsType(borrow->expression.get());
    if (inner.empty()) inner = "i32";
    return inner + "*";
  } else if (auto* call = dynamic_cast<CallExpressionNode*>(lhs)) {
    std::string funcName;
    if (auto* path = dynamic_cast<PathExpressionNode*>(call->expression.get())) {
      funcName = path->toString();
    } else {
      error("Unsupported function expression in call");
      return "";
    }
    funcName = mangleFuncName(funcName);
    std::string retType = functionTable[funcName];
    irStream << "; function name: " << funcName << '\n';
    if (retType.empty()) {
      retType = "i32";
      irStream << "; function " << funcName << " not found in function table, default to i32\n";
    }
    return retType;
  } else if (auto* ifExpr = dynamic_cast<IfExpressionNode*>(lhs)) {
    if (ifExpr->block_expression && ifExpr->block_expression->expression_without_block) {
      return getLhsType(ifExpr->block_expression->expression_without_block.get());
    } else {
      return "void";
    }
  } else if (auto* exprWithoutBlock = dynamic_cast<ExpressionWithoutBlockNode*>(lhs)) {
    return std::visit([&](auto&& arg) -> std::string {
      return getLhsType(arg.get());
    }, exprWithoutBlock->expr);
  } else if (auto* methodCall = dynamic_cast<MethodCallExpressionNode*>(lhs)) {
    std::string methodName;
    if (std::holds_alternative<Identifier>(methodCall->path_expr_segment)) {
      methodName = std::get<Identifier>(methodCall->path_expr_segment).id;
    } else {
      error("PathInType in method call not supported");
      return "";
    }
    std::string selfType = getLhsType(methodCall->expression.get());
    std::string baseTypeForName = stripTrailingStars(selfType);
    if (!baseTypeForName.empty() && baseTypeForName.front() == '%') baseTypeForName.erase(baseTypeForName.begin());
    std::string mangledMethodName = baseTypeForName.empty() ? methodName : (baseTypeForName + "_" + methodName);
    std::string retType = functionTable[mangledMethodName];
    if (retType.empty()) retType = "i32";
    return retType;
  } else {
    error(std::string("Unsupported expression in getting lhs type: ") + typeid(*lhs).name());
    return "";
  }
}

std::string IRGenerator::getLhsTypeWithStar(ExpressionNode* lhs) {
  if (auto* path = dynamic_cast<PathExpressionNode*>(lhs)) {
    std::string name = path->toString();
    //irStream << "; name of pathexpression in getting type : " << name << '\n';
    if (constantTable.find(name) != constantTable.end()) {
      // 常量
      std::string value = constantTable[name];
      if (value[0] == '"') {
        return "i8*";
      } else {
        return "i32";
      }
    } else {
      std::string type = lookupVarType(name);
      if (type.back() != '*') {
        return type + "*";
      } else {
        return type;
      }
    }
  } else if (auto* deref = dynamic_cast<DereferenceExpressionNode*>(lhs)) {
    // *expr, 类型是 expr 类型去掉 *
    std::string exprType = getLhsType(deref->expression.get());
    if (exprType.back() == '*') {
      return exprType.substr(0, exprType.size() - 1);
    } else {
      error("Dereference on non-pointer type");
      return "";
    }
  } else if (auto* field = dynamic_cast<FieldExpressionNode*>(lhs)) {
    // 检查是否是 register 字段
    if (auto* path = dynamic_cast<PathExpressionNode*>(field->expression.get())) {
      std::string baseName = path->toString();
      std::string fieldType = lookupVarType(baseName + "." + field->identifier.id);
      if (!fieldType.empty()) {
        return fieldType;
      }
    }
    //irStream << "; getting base type of field expression\n";
    std::string baseType = getLhsTypeWithStar(field->expression.get());
    //irStream << "; base type in fieldexpression: " << baseType << '\n';
    if (baseType == "") {
      if (auto* path = dynamic_cast<PathExpressionNode*>(field->expression.get())) {
        //irStream << "; trying to get type in struct field\n";
        std::string name = path->toString();
        //irStream <<"; finding " << name << " in struct field\n";
        std::string fieldName = name + "." + field->identifier.id;
        //irStream << "; getting type of struct item: " << fieldName << '\n';
        baseType = lookupVarType(fieldName);
        return baseType;
      }
    }
    size_t starPos = baseType.find('*');
    std::string structName;
    if (starPos != std::string::npos) {
      structName = baseType.substr(1, starPos - 1);
    } else {
      structName = baseType.substr(1);
    }
    auto it = structFields.find(structName);
    if (it == structFields.end()) {
      error("Unknown struct " + structName);
      return "";
    }
    int index = -1;
    for (int i = 0; i < it->second.size(); ++i) {
      if (it->second[i].first == field->identifier.id) {
        index = i;
        break;
      }
    }
    if (index == -1) {
      error("Field not found " + field->identifier.id);
      return "";
    }
    if (starPos != std::string::npos) {
      return it->second[index].second + "*";
    } else {
      return it->second[index].second;
    }
  } else if (auto* index = dynamic_cast<IndexExpressionNode*>(lhs)) {
    // base[index], 类型是 base 的元素类型
    irStream << "; getting type with star of index expression\n";
    std::string baseType = getLhsTypeWithStar(index->base.get());
    //irStream << "; base type in index expression: " << baseType << "\n";
    std::string elementType = getElementType(baseType);
    //irStream << "; element type in getting type of indexexpression: " << elementType << '\n';
    if (elementType != baseType) {
      // 是数组
      if (baseType.back() == '*') elementType = elementType + "*";
      return elementType;
    } else if (baseType.back() == '*') {
      // 是指针
      return baseType.substr(0, baseType.size() - 1);
    } else {
      error("Index on non-array non-pointer type");
      return "";
    }
  } else if (auto* methodCall = dynamic_cast<MethodCallExpressionNode*>(lhs)) {
    std::string methodName;
    if (std::holds_alternative<Identifier>(methodCall->path_expr_segment)) {
      methodName = std::get<Identifier>(methodCall->path_expr_segment).id;
    } else {
      error("PathInType in method call not supported");
      return "";
    }
    std::string selfType = getLhsType(methodCall->expression.get());
    std::string baseTypeForName = stripTrailingStars(selfType);
    if (!baseTypeForName.empty() && baseTypeForName.front() == '%') baseTypeForName.erase(baseTypeForName.begin());
    std::string mangledMethodName = baseTypeForName.empty() ? methodName : (baseTypeForName + "_" + methodName);
    std::string retType = functionTable[mangledMethodName];
    if (retType.empty()) retType = "i32";
    return retType;
  } else {
    error("Unsupported lhs type in assignment");
    return "";
  }
}

std::optional<int> IRGenerator::evaluateConstant(ExpressionNode* expr) {
  if (auto* lit = dynamic_cast<LiteralExpressionNode*>(expr)) {
    if (std::holds_alternative<std::unique_ptr<integer_literal>>(lit->literal)) {
      return std::stoi(std::get<std::unique_ptr<integer_literal>>(lit->literal)->value);
    }
  } else if (auto* path = dynamic_cast<PathExpressionNode*>(expr)) {
    std::string name = path->toString();
    if (constantTable.find(name) != constantTable.end()) {
      return std::stoi(constantTable[name]);
    } else {
      irStream << "; constant: " << name << " not found\n";
    }
  } else if (auto* arith = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(expr)) {
    auto lhs = evaluateConstant(arith->expression1.get());
    auto rhs = evaluateConstant(arith->expression2.get());
    if (lhs && rhs) {
      switch (arith->type) {
        case ADD: return *lhs + *rhs;
        case MINUS: return *lhs - *rhs;
        case MUL: return *lhs * *rhs;
        case DIV: return *lhs / *rhs;
        case MOD: return *lhs % *rhs;
        default: break;
      }
    }
  } else {
    irStream << "Unknow type of expression in evaluating constant" << typeid(*expr).name() << '\n';
    error(std::string("Unknow type of expression in evaluating constant") + typeid(*expr).name());
  }
  return std::nullopt;
}

bool IRGenerator::hasReturn(BlockExpressionNode* block) {
  for (auto& stmt : block->statement) {
    if (auto* exprStmt = dynamic_cast<ExpressionStatement*>(stmt.get())) {
      if (hasReturnInExpression(exprStmt->expression.get())) return true;
    }
  }
  if (block->expression_without_block) {
    return true;
  }
  return false;
}

bool IRGenerator::hasReturnInExpression(ExpressionNode* expr) {
  if (dynamic_cast<ReturnExpressionNode*>(expr)) return true;
  if (auto* block = dynamic_cast<BlockExpressionNode*>(expr)) return hasReturn(block);
  if (auto* if_ = dynamic_cast<IfExpressionNode*>(expr)) {
    if (hasReturn(if_->block_expression.get())) return true;
    if (if_->else_block && hasReturn(if_->else_block.get())) return true;
    if (if_->else_if && hasReturnInExpression(if_->else_if.get())) return true;
    return false;
  }
  return false;
}

bool IRGenerator::willReturn(BlockExpressionNode* block) {
  //irStream << "; checking if block expression will return\n";
  for (auto& stmt : block->statement) {
    if (stmt->expr_statement) {
      //irStream << "; checking expression statement in block expression\n";
      if (willReturnInExpression(stmt->expr_statement->expression.get())) return true;
    }
  }
  if (block->expression_without_block) {
    return true;
  }
  return false;
}

bool IRGenerator::willReturnInExpression(ExpressionNode* expr) {
  if (dynamic_cast<ReturnExpressionNode*>(expr)) return true;
  if (auto* block = dynamic_cast<BlockExpressionNode*>(expr)) return willReturn(block);
  if (auto* if_ = dynamic_cast<IfExpressionNode*>(expr)) {
    //irStream << "; checking if ifexpression will return\n";
    if (!willReturn(if_->block_expression.get())) {
      //irStream << "; cannot return in main block\n";
      return false;
    }
    if (if_->else_block) {
      if (!willReturn(if_->else_block.get())) {
        //irStream << "; cannot return in else block\n";
        return false;
      }
    } else if (if_->else_if) {
      if (!willReturnInExpression(if_->else_if.get())) {
        //irStream << "; cannot return in else if expression\n";
        return false;
      }
    } else {
      //irStream << "; having no else_block and else_if\n";
      return false;
    }
    return true;
  }
  //irStream << "; unknown expression in checking will return\n";
  return false;
}

bool IRGenerator::isLetDefined(const std::string& name) {
  size_t dotPos = name.find('.');
  if (dotPos != std::string::npos) {
    // 对于字段，暂时不支持，直接返回 false
    return false;
  }
  for (auto it = isLetDefinedScopes.rbegin(); it != isLetDefinedScopes.rend(); ++it) {
    if (it->find(name) != it->end()) {
      return (*it)[name];
    }
  }
  return false;
}

std::string IRGenerator::visit_in_rhs(ExpressionNode* node) {
  if (auto* arith = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(node)) {
    return visit_in_rhs(arith);
  } else if (auto* ifExpr = dynamic_cast<IfExpressionNode*>(node)) {
    return visit_in_rhs(ifExpr);
  } else if (auto* group = dynamic_cast<GroupedExpressionNode*>(node)) {
    return visit_in_rhs(group);
  } else {
    return visit(node);
  }
}

std::string IRGenerator::visit_in_rhs(ArithmeticOrLogicalExpressionNode* node) {
  irStream << "; visiting arithmetic or logical expression in let\n";
  std::string lhs = visit_in_rhs(static_cast<ExpressionNode*>(node->expression1.get()));
  std::string lhsType = getLhsType(node->expression1.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression1.get())) {
    std::string lhs_path = path->toString();
    std::string lhs_type = lookupVarType(lhs_path);
    std::string lhs_addr = lookupSymbol(lhs_path);
    if (lhs_type == "i32*") {
      std::string lhs_temp = createTemp();
      irStream << "  %" << lhs_temp << " = load i32, i32* %" << lhs_addr << '\n';
      lhs = "%" + lhs_temp;
      lhsType = "i32";
    } else if (lhs_type == "i64*") {
      std::string lhs_temp = createTemp();
      irStream << "  %" << lhs_temp << " = load i64, i64* %" << lhs_addr << '\n';
      lhs = "%" + lhs_temp;
      lhsType = "i64";
    }
  }
  if (auto* type_cast = dynamic_cast<TypeCastExpressionNode*>(node->expression1.get())) {
    if (auto* path = dynamic_cast<PathExpressionNode*>(type_cast->expression.get())) {
      std::string lhs_path = path->toString();
      std::string lhs_type = lookupVarType(lhs_path);
      std::string lhs_addr = lookupSymbol(lhs_path);
      if (lhs_type == "i32*") {
        std::string lhs_temp = createTemp();
        irStream << "  %" << lhs_temp << " = load i32, i32* %" << lhs_addr << '\n';
        lhs = "%" + lhs_temp;
        lhsType = "i32";
      } else if (lhs_type == "i64*") {
        std::string lhs_temp = createTemp();
        irStream << "  %" << lhs_temp << " = load i64, i64* %" << lhs_addr << '\n';
        lhs = "%" + lhs_temp;
        lhsType = "i64";
        if (toIRType(type_cast->type.get()) == "i32") {
          lhsType = "i32";
          std::string temp = createTemp();
          irStream << "  %" << temp << " = trunc i64 " << lhs << " to i32\n";
          lhs = "%" + temp;
        }
      }
    }
  }
  std::string rhs = visit_in_rhs(static_cast<ExpressionNode*>(node->expression2.get()));
  std::string rhsType = getLhsType(node->expression2.get());
  if (auto* path = dynamic_cast<PathExpressionNode*>(node->expression2.get())) {
    std::string rhs_path = path->toString();
    std::string rhs_type = lookupVarType(rhs_path);
    std::string rhs_addr = lookupSymbol(rhs_path);
    if (rhs_type == "i32*") {
      std::string rhs_temp = createTemp();
      irStream << "  %" << rhs_temp << " = load i32, i32* %" << rhs_addr << '\n';
      rhs = "%" + rhs_temp;
      rhsType = "i32";
    } else if (rhs_type == "i64*") {
      std::string rhs_temp = createTemp();
      irStream << "  %" << rhs_temp << " = load i64, i64* %" << rhs_addr << '\n';
      rhs = "%" + rhs_temp;
      rhsType = "i64";
    }
  }
  if (auto* type_cast = dynamic_cast<TypeCastExpressionNode*>(node->expression2.get())) {
    if (auto* path = dynamic_cast<PathExpressionNode*>(type_cast->expression.get())) {
      std::string rhs_path = path->toString();
      std::string rhs_type = lookupVarType(rhs_path);
      std::string rhs_addr = lookupSymbol(rhs_path);
      if (rhs_type == "i32*") {
        std::string rhs_temp = createTemp();
        irStream << "  %" << rhs_temp << " = load i32, i32* %" << rhs_addr << '\n';
        rhs = "%" + rhs_temp;
        rhsType = "i32";
      } else if (rhs_type == "i64*") {
        std::string rhs_temp = createTemp();
        irStream << "  %" << rhs_temp << " = load i64, i64* %" << rhs_addr << '\n';
        rhs = "%" + rhs_temp;
        rhsType = "i64";
      }
    }
  }
  std::string resultType = (lhsType == "i64" || rhsType == "i64") ? "i64" : "i32";
  // Convert operands to resultType if necessary
  if (resultType == "i64") {
    if (lhsType == "i32") {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = sext i32 " << lhs << " to i64\n";
      lhs = "%" + temp;
    }
    if (rhsType == "i32") {
      std::string temp = createTemp();
      irStream << "  %" << temp << " = sext i32 " << rhs << " to i64\n";
      rhs = "%" + temp;
    }
  }
  irStream << "; lhsType: " << lhsType << ", rhsType: " << rhsType << ", resultType: " << resultType << "\n";
  std::string temp = createTemp();
  std::string op;
  switch (node->type) {
    case ADD: op = "add"; break;
    case MINUS: op = "sub"; break;
    case MUL: op = "mul"; break;
    case DIV: op = "sdiv"; break;
    case MOD: op = "srem"; break;
    case AND: op = "and"; break;
    case OR: op = "or"; break;
    case XOR: op = "xor"; break;
    case SHL: op = "shl"; break;
    case SHR: op = "ashr"; break;
    default: op = "add";
  }
  irStream << "  %" << temp << " = " << op << " " << resultType << " " << lhs << ", " << rhs << "\n";
  return "%" + temp;
}

std::string IRGenerator::visit_in_rhs(IfExpressionNode* node) {
  irStream << "; visiting if expression in rhs\n";
  std::string cond;
  if (std::holds_alternative<std::unique_ptr<ExpressionNode>>(node->conditions->condition)) {
    cond = visit(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get());
    irStream << "; condition expression type: " << typeid(*std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get()).name() << '\n';
    if (auto* path = dynamic_cast<PathExpressionNode*>(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get())) {
      std::string condName = path->toString();
      irStream << "; condName: " << condName << '\n';
      std::string condType = lookupVarType(condName);
      irStream << "; condType: " << condType << '\n';
      if (condType == "i1*") {
        std::string condTemp = createTemp();
        irStream << "  %" << condTemp << " = load i1, i1* " << cond << "\n";
        cond = "%" + condTemp;
      }
    } else if (auto* grouped = dynamic_cast<GroupedExpressionNode*>(std::get<std::unique_ptr<ExpressionNode>>(node->conditions->condition).get())) {
      if (auto* path = dynamic_cast<PathExpressionNode*>(grouped->expression.get())) {
        std::string condName = path->toString();
        irStream << "; condName: " << condName << '\n';
        std::string condType = lookupVarType(condName);
        irStream << "; condType: " << condType << '\n';
        if (condType == "i1*") {
          std::string condTemp = createTemp();
          irStream << "  %" << condTemp << " = load i1, i1* " << cond << "\n";
          cond = "%" + condTemp;
        }
      }
    }
  } else {
    error("Unsupported condition type in if assignment");
    return "";
  }

  std::string thenLabel = createLabel();
  std::string elseLabel = createLabel();
  std::string endLabel = createLabel();

  // 不使用 phi：用一个临时栈槽在分支内 store，endLabel 处 load。
  // 结果类型尽量从 if 表达式自身推断（getLhsType(IfExpressionNode*)）。
  std::string resultType = getLhsType(static_cast<ExpressionNode*>(node));
  if (resultType.empty() || resultType == "void" || resultType == "i32*") resultType = "i32";
  std::string resultPtr = createTemp();
  irStream << "  %" << resultPtr << " = alloca " << expandStructType(resultType) << "\n";

  irStream << "  br i1 " << cond << ", label %" << thenLabel << ", label %" << elseLabel << "\n";

  irStream << thenLabel << ":\n";
  irStream << "  ; then branch let assignment\n";
  // 处理 statements，但不处理 expression_without_block 的 store/br
  for (auto& stmt : node->block_expression->statement) {
    visit(stmt.get());
  }
  std::string thenValue = node->block_expression->expression_without_block ? visit(node->block_expression->expression_without_block.get()) : "";
  std::string thenType = node->block_expression->expression_without_block ? getLhsType(node->block_expression->expression_without_block.get()) : resultType;
  // i32/i64 对齐（最常见的数值分支对齐需求）
  irStream << "; thenType: " << thenType << ", resultType: " << resultType << "\n";
  if (resultType == "i64" && thenType == "i32") {
    std::string t = createTemp();
    irStream << "  %" << t << " = sext i32 " << thenValue << " to i64\n";
    thenValue = "%" + t;
  } else if (resultType == "i32" && thenType == "i64") {
    std::string t = createTemp();
    irStream << "  %" << t << " = trunc i64 " << thenValue << " to i32\n";
    thenValue = "%" + t;
  } else if (resultType == "i32" && thenType == "i32*") {
    std::string t = createTemp();
    irStream << "  %" << t << " = load i32, i32* " << thenValue << "\n";
    thenValue = "%" + t;
  }
  irStream << "  store " << expandStructType(resultType) << " " << thenValue
           << ", " << expandStructType(resultType) << "* %" << resultPtr << "\n";
  irStream << "  br label %" << endLabel << "\n";

  irStream << elseLabel << ":\n";
  irStream << "  ; else branch let assignment\n";
  std::string elseValue;
  std::string elseType = resultType;
  if (node->else_block) {
    // 类似处理 else_block
    for (auto& stmt : node->else_block->statement) {
      visit(stmt.get());
    }
    elseValue = node->else_block->expression_without_block ? visit(node->else_block->expression_without_block.get()) : "";
    elseType = node->else_block->expression_without_block ? getLhsType(node->else_block->expression_without_block.get()) : resultType;
  } else if (node->else_if) {
    // 注意：@/RCompiler-FAQ/include/parser.hpp 中 IfExpressionNode::else_if 可能继续挂着 IfExpressionNode（else if 链）。
    // 这里递归使用 visit_in_rhs 覆盖整条链，但最终仍然在本层用 store/load 汇总结果（无 phi）。
    elseValue = visit_in_rhs(node->else_if.get());
    elseType = getLhsType(node->else_if.get());
  } else {
    error("If expression in let must have else");
    return "";
  }
  if (resultType == "i64" && elseType == "i32") {
    std::string t = createTemp();
    irStream << "  %" << t << " = sext i32 " << elseValue << " to i64\n";
    elseValue = "%" + t;
  } else if (resultType == "i32" && elseType == "i64") {
    std::string t = createTemp();
    irStream << "  %" << t << " = trunc i64 " << elseValue << " to i32\n";
    elseValue = "%" + t;
  } else if (resultType == "i32" && elseType == "i32*") {
    std::string t = createTemp();
    irStream << "  %" << t << " = load i32, i32* " << elseValue << "\n";
    elseValue = "%" + t;
  }
  irStream << "  store " << expandStructType(resultType) << " " << elseValue
           << ", " << expandStructType(resultType) << "* %" << resultPtr << "\n";
  irStream << "  br label %" << endLabel << "\n";

  irStream << endLabel << ":\n";
  std::string loaded = createTemp();
  irStream << "  %" << loaded << " = load " << expandStructType(resultType)
           << ", " << expandStructType(resultType) << "* %" << resultPtr << "\n";
  return "%" + loaded;
}

std::string IRGenerator::visit_in_rhs(GroupedExpressionNode* node) {
  return visit_in_rhs(node->expression.get());
}

#endif
