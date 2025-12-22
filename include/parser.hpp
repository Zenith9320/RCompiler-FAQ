#ifndef PARSER_HPP
#define PARSER_HPP
#include <vector>
#include <memory>
#include <string>
#include <variant>
#include <optional>
#include <unordered_set>
#include "lexer.hpp"

class ModuleNode;
class FunctionNode;
class StructStructNode;
class TupleStructNode;
class EnumerationNode;
class ConstantItemNode;
class TraitNode;
class InherentImplNode;
class TraitImplNode;
class GenParaNode;
class AssociatedItemNode;

using ItemType = std::variant<
    std::unique_ptr<ModuleNode>,
    std::unique_ptr<FunctionNode>,
    std::unique_ptr<StructStructNode>,
    std::unique_ptr<TupleStructNode>,
    std::unique_ptr<EnumerationNode>,
    std::unique_ptr<ConstantItemNode>,
    std::unique_ptr<TraitNode>,
    std::unique_ptr<InherentImplNode>,
    std::unique_ptr<TraitImplNode>,
    std::unique_ptr<GenParaNode>,
    std::unique_ptr<AssociatedItemNode>
>;

const std::unordered_set<std::string> IntSuffixes = {"i8", "i16", "i32", "i64", "i128", "isize"};
const std::unordered_set<std::string> UintSuffixes = {"u8", "u16", "u32", "u64", "u128", "usize"};
const std::unordered_set<std::string> FloatSuffixes = {"f32", "f64"};

//节点的类型
enum NodeType {
  NullStatement, OuterAttribute, Module, InnerAttribute, Function, Struct, Enumeration, ConstantItem, Trait, Implementation, GenPara, AssociatedItem, StructStruct, RangeExpression,
  TupleStruct, NodeType_StructField, NodeType_TupleField, EnumVariant, EnumVariants, EnumVariantTuple, EnumVariantStruct, EnumVariantDiscriminant, LiteralExpression, BlockExpression,
  ExpressionWithoutBlock, OperatorExpression, BorrowExpression, DereferenceExpression, NegationExpression, ArithmeticOrLogicalExpression, ComparisonExpression, LazyBooleanExpression,
  TypeCastExpression, AssignmentExpression, CompoundAssignmentExpression, GroupedExpression, ArrayExpression, IndexExpression, TupleExpression, TupeIndexingExpression, StructExpression,
  CallExpression, MethodCallExpression, FieldExpression, InfiniteLoopExpression, PredicateLoopExpression, LoopExpression, IfExpression, MatchExpression, ReturnExpression, UnderscoreExpression,
  ParenthesizedType, TypePath_node, TupleType, NeverType, ArrayType, SliceType, InferredType, QualifiedPathInType, Statement, PathExpression, ReferenceType, InherentImplementation, TraitImplementation,
  BreakExpression, ContinueExpression
};

//运算类型
enum OperationType {
  ADD, MINUS, MUL, DIV, MOD, AND, OR, XOR, SHL, SHR
};

//比较类型
enum ComparisonType {
  EQ, NEQ, GT, LT, GEQ, LEQ
};

template<typename Target, typename T>
bool is_type(const std::unique_ptr<T>& ptr) {
  return dynamic_cast<Target*>(ptr.get()) != nullptr;
}

class Identifier {
 public:
  std::string id;
  Identifier(std::string s) : id(s) {};

  bool check() {
   if (id.empty()) return false;
    if (!std::isalpha(static_cast<unsigned char>(id[0]))) {
      return false;
    }
    for (size_t i = 1; i < id.size(); ++i) {
      unsigned char c = id[i];
      if (!(std::isalnum(c) || c == '_')) {
        return false;
      }
    }
    return true;
  }
};

class Keyword {
 public:
  std::string keyword;

  Keyword(std::string s) : keyword(s) {};

  bool check() {
    if (keywords.find(keyword) != keywords.end()) return true;
    return false;
  }
};

struct Lifetime {
  std::string lifetime; // "'static" or "'_"
  
  Lifetime(std::string s) : lifetime(s) {};
};

/*
base class
*/
class ASTNode {
 public:
  NodeType type;
  int row, col;
  ASTNode(NodeType t, int r, int c) : type(t), row(r), col(c) {}
  virtual ~ASTNode() = default;
};

/*
class of expression
*/
class ExpressionNode : public ASTNode {
 public:
  ExpressionNode(NodeType t, int l, int c) : ASTNode(t, l, c) {};

  NodeType get_type() { return type; }

  int get_l() { return row; }

  int get_c() { return col; }

  virtual ~ExpressionNode() = default;
};

/*
class of items
*/
class ItemNode : public ASTNode {
 public:
  ItemNode(NodeType t, int l, int c) : ASTNode(t, l, c) {};
};

/*
class of types
*/
enum class TypeType {
  ParenthesizedType_node, TypePath_node, TupleType_node, NeverType_node, ArrayType_node, SliceType_node, InferredType_node, QualifiedPathInType_node, ReferenceType_node
};

class TypeNode : public ASTNode {
 public:
  TypeType node_type;

  TypeNode(TypeType tt, NodeType t, int l, int c) : ASTNode(t, l, c), node_type(tt) {};
  virtual std::string toString() const {
    return "<unknown_type>";
  }
  virtual ~TypeNode() = default;
};

//ParenthesizedType → ( Type )
class ParenthesizedTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> type;

  ParenthesizedTypeNode(std::unique_ptr<TypeNode> t, int l, int c) : type(std::move(t)), TypeNode(TypeType::ParenthesizedType_node, NodeType::ParenthesizedType, l, c) {};

  std::string toString() const override {
    if (!type) return "(<null>)";
    return "(" + type->toString() + ")";
  }
};  

class TypePath;

//TypePath → ::? TypePathSegment ( :: TypePathSegment )*
class TypePathNode : public TypeNode {
 public:
  std::unique_ptr<TypePath> type_path;

  TypePathNode(std::unique_ptr<TypePath> tp, int l, int c) : type_path(std::move(tp)), TypeNode(TypeType::TypePath_node, NodeType::TypePath_node, l, c) {};
  TypePathNode(std::string s) : TypeNode(TypeType::TypePath_node, NodeType::TypePath_node, 0, 0) {
    std::vector<std::string> strings;
    strings.push_back(s);
    type_path = std::move(std::make_unique<TypePath>(strings));
  };

  std::string toString() const;
};

//TupleType → ( ) | ( ( Type , )+ Type? )
class TupleTypeNode : public TypeNode {
 public:
  std::vector<std::unique_ptr<TypeNode>> types;

  TupleTypeNode(std::vector<std::unique_ptr<TypeNode>> t, int l, int c) : types(std::move(t)), TypeNode(TypeType::TupleType_node, NodeType::TupleType, l, c) {};

  std::string toString() const override {
    std::string result = "(";
    for (size_t i = 0; i < types.size(); ++i) {
      result += types[i] ? types[i]->toString() : "<null>";
      if (i + 1 < types.size()) result += ", ";
    }
    result += ")";
    return result;
  }
};  

//NeverType → !
class NeverTypeNode : public TypeNode { 
 public:
  NeverTypeNode(int l, int c) : TypeNode(TypeType::NeverType_node, NodeType::NeverType, l, c) {};

  std::string toString() const override {
    return "!";
  }
};

//ArrayType → [ Type ; Expression ]
class ArrayTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<ExpressionNode> expression;

  ArrayTypeNode(std::unique_ptr<TypeNode> t, std::unique_ptr<ExpressionNode> e, int l, int c) 
              : type(std::move(t)), expression(std::move(e)), TypeNode(TypeType::ArrayType_node, NodeType::ArrayType, l, c) {};
  
  ArrayTypeNode(TypeNode* t, ExpressionNode* e, int l, int c)
              : type(t), expression(e), TypeNode(TypeType::ArrayType_node, NodeType::ArrayType, l, c) {}
  std::string toString() const override {
    return "[" + (type ? type->toString() : "<null>") + "]";
  }
};

//SliceType → [ Type ]
class SliceTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> type;

  SliceTypeNode(std::unique_ptr<TypeNode> t, int l, int c) : type(std::move(t)), TypeNode(TypeType::SliceType_node, NodeType::SliceType, l, c) {};

  std::string toString() const override {
      return "[" + (type ? type->toString() : "<null>") + "]";
  }
};

//InferredType → _
class InferredTypeNode : public TypeNode { 
 public:
  InferredTypeNode(int l, int c) : TypeNode(TypeType::InferredType_node, NodeType::InferredType, l, c) {};

  std::string toString() const override {
      return "_";
  }
};

class TypePathSegment;

//QualifiedPathInType → QualifiedPathType ( :: TypePathSegment )+
//QualifiedPathType → < Type ( as TypePath )? >
class QualifiedPathInTypeNode : public TypeNode {
 public:
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<TypePath> type_path;
  std::vector<std::unique_ptr<TypePathSegment>> type_path_segments;

  QualifiedPathInTypeNode(std::unique_ptr<TypeNode> t, std::unique_ptr<TypePath> tp, std::vector<std::unique_ptr<TypePathSegment>> tps, int l, int c)
                        : TypeNode(TypeType::QualifiedPathInType_node, NodeType::QualifiedPathInType, l, c), type(std::move(t)), type_path(std::move(tp)), type_path_segments(std::move(tps)) {};
};

//ReferenceType → & mut? TypeNoBounds
class ReferenceTypeNode : public TypeNode {
 public:
  bool if_mut = false;
  std::unique_ptr<TypeNode> type;

  ReferenceTypeNode(std::unique_ptr<TypeNode> t, bool im, int l, int c) : type(std::move(t)), if_mut(im), TypeNode(TypeType::ReferenceType_node, NodeType::ReferenceType, l, c) {};
  ReferenceTypeNode(TypeNode* t, bool im, int l, int c) : type(t), if_mut(im), TypeNode(TypeType::ReferenceType_node, NodeType::ReferenceType, l, c) {};

  std::string toString() const override {
    std::string ans = "&";
    if (if_mut) {
      ans = ans + "mut";
    }
    ans += type->toString();
    return ans;
  }
};

bool is_type_mutable(TypeNode* type) {
  if (auto* paren = dynamic_cast<ParenthesizedTypeNode*>(type)) {
    return is_type_mutable(paren->type.get());
  } else if (auto* ref = dynamic_cast<ReferenceTypeNode*>(type)) {
    return ref->if_mut;
  } else if (auto* slice = dynamic_cast<SliceTypeNode*>(type)) {
    return is_type_mutable(slice->type.get());
  } else {
    return false;
  }
}

/*
null statement
*/
class NullStatementNode : public ASTNode {
  NullStatementNode(int l, int c) : ASTNode(NodeType::NullStatement, l, c) {}
};

/*
classes of attributes
actually they are not nodes, but to distinguish them from type, they pretend to be nodes
*/
class OuterAttributeNode : public ASTNode {
 public:
  std::string attr;
  OuterAttributeNode(int l, int c) : ASTNode(NodeType::OuterAttribute, l, c) {}
};

class InnerAttributeNode : public ASTNode {
  std::string attr;

  InnerAttributeNode(std::string a, int l, int c) : ASTNode(NodeType::InnerAttribute, l, c) {
    attr = a;
  }
};

/*
1.  mod ID ;
2.  mod ID { items }
*/
class ModuleNode : public ItemNode {
 public:
  std::string id;
  bool isDeclaration;

  //std::vector<InnerAttributeNode> InnerAttributes;
  std::vector<std::unique_ptr<ItemNode>> items;

  ModuleNode(std::string i, int l, int c) :id(i), ItemNode(NodeType::Module, l ,c) {};

  ModuleNode(std::string i, int l, int c, std::vector<std::unique_ptr<ItemNode>> item)
            : id(i), ItemNode(NodeType::Module, l, c), items(std::move(item)) {};
};

/*
structs used in function node
*/
//const? async?​ ItemSafety?​ ( extern Abi? )?
enum class ItemSafety { None, Safe, Unsafe };
struct FunctionQualifier {
  bool is_const = false;
  bool is_async = false;
  bool is_unsafe = false;
  bool has_extern = false;        
  std::optional<std::string> abi = std::nullopt; 

  FunctionQualifier() = default;

  FunctionQualifier(bool c, bool a, bool s, bool e, std::optional<std::string> abi_str)
      : is_const(c), is_async(a), is_unsafe(s), has_extern(e), abi(std::move(abi_str)) {}
};

//&? mut? self
struct ShorthandSelf {
  bool if_prefix = false; //是否有&
  bool if_mut = false;

  ShorthandSelf(bool ip, bool im) : if_prefix(ip), if_mut(im) {};
};

//mut? self : Type
struct TypedSelf {
  bool if_mut = false;
  std::unique_ptr<TypeNode> type;

  TypedSelf(bool im, std::unique_ptr<TypeNode> t) : if_mut(im), type(std::move(t)) {};
};

//( ShorthandSelf | TypedSelf )
struct SelfParam {
  std::variant<std::unique_ptr<ShorthandSelf>, std::unique_ptr<TypedSelf>> self;

  std::unique_ptr<TypeNode> type_node;

  SelfParam(std::unique_ptr<ShorthandSelf> s) : self(std::move(s)) {}

  SelfParam(std::unique_ptr<TypedSelf> s) : self(std::move(s)) {}
};

//...
struct ellipsis {
  std::string ellip = "...";

  ellipsis() = default;
};

class PatternNoTopAlt;

//FunctionParamPattern → PatternNoTopAlt : ( Type | ... )
struct FunctionParamPattern {
 public:
  std::unique_ptr<PatternNoTopAlt> pattern;
  std::unique_ptr<TypeNode> type;

  FunctionParamPattern(std::unique_ptr<PatternNoTopAlt> p, std::unique_ptr<TypeNode> t) : pattern(std::move(p)), type(std::move(t)) {};
};

//( FunctionParamPattern | ... | Type )
struct FunctionParam {
  //std::vector<OuterAttributeNode> outer_attributes;
  std::variant<std::unique_ptr<FunctionParamPattern>, std::unique_ptr<ellipsis>, std::unique_ptr<TypeNode>> info;

  FunctionParam(std::unique_ptr<FunctionParamPattern> fpp) : info(std::move(fpp)) {};
  FunctionParam(std::unique_ptr<TypeNode> t) : info(std::move(t)) {};
  FunctionParam(std::unique_ptr<ellipsis> e) : info(std::move(e)) {};
};

// ->Type
struct FunctionReturnType {
  std::unique_ptr<TypeNode> type;

  FunctionReturnType(std::unique_ptr<TypeNode> t) : type(std::move(t)) {};
};

//1.  SelfParam
//2.  SelfParam? FunctionParam*
struct FunctionParameter {
  int type = 0; // 1 for only SelfParam, 2 for having FunctionParam
  std::unique_ptr<SelfParam> self_param;
  std::vector<std::unique_ptr<FunctionParam>> function_params;

  FunctionParameter() = default;
  FunctionParameter(int t, std::vector<std::unique_ptr<FunctionParam>> fp) : type(t), function_params(std::move(fp)) {};
  FunctionParameter(int t, std::unique_ptr<SelfParam> sp, std::vector<std::unique_ptr<FunctionParam>> fp) : type(t), self_param(std::move(sp)), function_params(std::move(fp)) {};

  bool is_selfParam_mut() const {
    if (!self_param) return false;

    return std::visit([](const auto& ptr) -> bool {
      using T = std::decay_t<decltype(ptr)>;
      if constexpr (std::is_same_v<T, std::unique_ptr<ShorthandSelf>>) {
        return ptr && ptr->if_mut;
      } else if constexpr (std::is_same_v<T, std::unique_ptr<TypedSelf>>) {
        return ptr && ptr->if_mut;
      } else {
        return false;
      }
    }, self_param->self);
  }
};

//WhereClause → where ( WhereClauseItem , )* WhereClauseItem?
struct WhereClause {

};

//{ Statements? }
class StatementNode;
struct BlockExpression {
  std::vector<std::unique_ptr<StatementNode>> statements;

  BlockExpression(std::vector<std::unique_ptr<StatementNode>> stmts) : statements(std::move(stmts)) {};
};

/*
FunctionQualifier fn identifier GenericParams? ( FunctionParameters? ) FunctionReturnType? WhereClause? ( BlockExpression | ; )
*/
class BlockExpressionNode;

class FunctionNode : public ItemNode {
 public:
  FunctionQualifier function_qualifier;
  std::string identifier;
  //std::unique_ptr<GenParaNode> generic_params = nullptr;
  std::unique_ptr<FunctionParameter> function_parameter;
  std::unique_ptr<FunctionReturnType> return_type;
  //std::optional<WhereClause> where_clause = std::nullopt;
  std::unique_ptr<BlockExpressionNode> block_expression = nullptr;
  std::optional<std::string> impl_type_name = std::nullopt;

  FunctionNode(FunctionQualifier fq, std::string id, int l, int c) 
            : function_qualifier(fq), identifier(id), ItemNode(NodeType::Function, l, c) {};
};

//Trait → unsafe? trait IDENTIFIER ( : TypeParamBounds? )? { AssociatedItem* }
class TraitNode : public ItemNode {
 public:
  bool isUnsafe;
  std::string identifier;
  std::unique_ptr<TypeNode> type;
  std::vector<std::unique_ptr<AssociatedItemNode>> associatedItems;

  TraitNode(bool unsafeFlag, const std::string &name, std::unique_ptr<TypeNode> bounds, std::vector<std::unique_ptr<AssociatedItemNode>> items, int l, int c)
          : isUnsafe(unsafeFlag), identifier(name), type(std::move(bounds)), associatedItems(std::move(items)), ItemNode(NodeType::Trait , l, c) {}
};

/*
Some classes for struct
*/

//SimplePathSegment
struct SimplePathSegment {
  enum SimplePathType {
    SUPER, SELF, CRATE, $CRATE, ID
  };

  SimplePathType type;
  std::optional<std::string> id = std::nullopt;

  SimplePathSegment(std::string s) : type(SimplePathType::ID), id(s) {};
  SimplePathSegment(SimplePathType t) : type(t) {};
};

//SimplePath
struct SimplePath {
  std::vector<SimplePathSegment> simplepath_segments;

  SimplePath(std::vector<SimplePathSegment> sps) : simplepath_segments(std::move(sps)) {};
};

//visibility→ pub | pub ( crate ) | pub ( self ) | pub ( super ) | pub ( in SimplePath )
struct Visibility {
  enum VisType {
    CRATE, SELF, SUPER, SIMPLEPATH, NONE
  };

  VisType type;
  std::optional<SimplePath> simple_path;

  Visibility(VisType t) : type(t), simple_path(std::nullopt) {};
  Visibility(SimplePath sp) : type(VisType::SIMPLEPATH), simple_path(sp) {};
};

//StructField → IDENTIFIER : Type
struct StructField {
  //Visibility visibility;
  std::string identifier;
  std::unique_ptr<TypeNode> type;

  StructField(std::string id, std::unique_ptr<TypeNode> t) : identifier(id), type(std::move(t)) {};
};

//StructFields → StructField ( , StructField )* ,?
class StructFieldNode : public ASTNode {
 public:
  std::vector<std::unique_ptr<StructField>> struct_fields;

  StructFieldNode(std::vector<std::unique_ptr<StructField>> sf, int l, int c) : struct_fields(std::move(sf)), ASTNode(NodeType::NodeType_StructField, l, c) {};
};

//TupleField → Visibility? Type
struct TupleField {
  //std::optional<Visibility> visibility = std::nullopt;
  std::unique_ptr<TypeNode> type;

  TupleField(std::unique_ptr<TypeNode> t) : type(std::move(t)) {};
};

//TupleFields → TupleField ( , TupleField )* ,?
class TupleFieldNode : public ASTNode {
 public:
  std::vector<std::unique_ptr<TupleField>> tuple_fields;

  TupleFieldNode(std::vector<std::unique_ptr<TupleField>> tf, int l, int c) : tuple_fields(std::move(tf)), ASTNode(NodeType::NodeType_TupleField, l, c) {};
};

/*
1.  StructStruct: struct IDENTIFIER GenericParams? WhereClause? ( { StructFields? } | ; )
2.  TupleStruct: struct IDENTIFIER GenericParams? ( TupleFields? ) WhereClause? ;
*/
class StructStructNode : public ItemNode {
 public:
  std::string identifier;
  //std::unique_ptr<GenParaNode> generic_param;
  //std::unique_ptr<WhereClause> where_clause;
  std::unique_ptr<StructFieldNode> struct_fields;

  StructStructNode(std::string id, int l, int c) : identifier(id), ItemNode(NodeType::StructStruct, l, c) {};
};
class TupleStructNode : public ItemNode {
 public:
  std::string identifier;
  //std::unique_ptr<GenParaNode> generic_param;
  //std::unique_ptr<WhereClause> where_clause;
  std::unique_ptr<TupleFieldNode> tuple_fields;

  TupleStructNode(std::string id, int l, int c) : identifier(id), ItemNode(NodeType::TupleStruct, l, c) {};
};

/*
classes for Enumeration
*/

//EnumVariantTuple → ( TupleFields? )
class EnumVariantTupleNode : public ASTNode {
 public: 
  std::unique_ptr<TupleFieldNode> tuple_field;

  EnumVariantTupleNode(int l, int c) : ASTNode(NodeType::EnumVariantTuple, l, c) {};
};

//EnumVariantStruct → { StructFields? }
class EnumVariantStructNode : public ASTNode {
 public: 
  std::unique_ptr<StructFieldNode> struct_field;

  EnumVariantStructNode(int l, int c) : ASTNode(NodeType::EnumVariantStruct, l, c) {};
};

//EnumVariantDiscriminant → = Expression
class EnumVariantDiscriminantNode : public ASTNode {
 public:
  std::unique_ptr<ExpressionNode> expression;

  EnumVariantDiscriminantNode(std::unique_ptr<ExpressionNode>exp, int l, int c) : expression(std::move(exp)), ASTNode(NodeType::EnumVariantDiscriminant, l, c) {};
  EnumVariantDiscriminantNode(int l, int c) : ASTNode(NodeType::EnumVariantDiscriminant, l, c) {};
};

//EnumVairant:EnumVariant → IDENTIFIER ( EnumVariantTuple | EnumVariantStruct )? EnumVariantDiscriminant?
class EnumVariantNode {
 public:
  std::string identifier;
  std::unique_ptr<EnumVariantTupleNode> enum_variant_tuple;
  std::unique_ptr<EnumVariantStructNode> enum_variant_struct;
  std::unique_ptr<EnumVariantDiscriminantNode> discriminant;

  EnumVariantNode(std::string s) : identifier(s) {};
};

//EnumVariants → EnumVariant ( , EnumVariant )* ,?
class EnumVariantsNode : public ASTNode {
 public:
  std::vector<std::unique_ptr<EnumVariantNode>> enum_variants;

  EnumVariantsNode(int l, int c) : ASTNode(NodeType::EnumVariants, l, c) {};
  
  EnumVariantsNode(std::vector<std::unique_ptr<EnumVariantNode>> variants, int l, int c)
      : enum_variants(std::move(variants)), ASTNode(NodeType::EnumVariants, l, c) {}
};

//Enumeration → enum IDENTIFIER { EnumVariants? }

class EnumerationNode : public ItemNode {
 public:
  std::string identifier;
  std::unique_ptr<EnumVariantsNode> enum_variants;

  EnumerationNode(std::string id, int l, int c) : identifier(id), ItemNode(NodeType::Enumeration, l, c) {};
};

enum ConstantType {
  ID, _
};
class ConstantItemNode : public ItemNode {
 public:
  ConstantType constant_type;
  std::optional<std::string> identifier;
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<ExpressionNode> expression;

  ConstantItemNode(std::string id, int l, int c) : constant_type(ConstantType::ID), identifier(id), ItemNode(NodeType::ConstantItem, l, c) {};
  ConstantItemNode(int l, int c) : constant_type(ConstantType::_), ItemNode(NodeType::ConstantItem, l, c) {};
};

/*
class for trait node
*/

//PathIdentSegment → IDENTIFIER | super | self | Self | crate | $crate
class PathIdentSegment {
 public:
  std::optional<std::string> identifier = std::nullopt;
  enum PathIdentSegmentType {
    ID, super, self, Self, crate, $crate
  };
  PathIdentSegmentType type;

  PathIdentSegment(std::string id) : type(PathIdentSegmentType::ID), identifier(id) {};
  PathIdentSegment(PathIdentSegmentType t) : type(t) {};
  std::string toString() const {
    switch (type) {
      case PathIdentSegmentType::ID: return identifier.has_value() ? identifier.value() : "<invalid-id>";
      case PathIdentSegmentType::super: return "super";
      case PathIdentSegmentType::self: return "self";
      case PathIdentSegmentType::Self: return "Self";
      case PathIdentSegmentType::crate: return "crate";
      case PathIdentSegmentType::$crate: return "$crate";
      default: return "<unknown>";
    }
  }
};

//TypePathFnInputs → Type ( , Type )* ,?
class TypePathFnInputs {
 public:
  std::vector<std::unique_ptr<TypeNode>> types;

  TypePathFnInputs(std::vector<std::unique_ptr<TypeNode>> t) : types(std::move(t)) {};
  std::string toString() const {
    std::string result = "(";
    for (size_t i = 0; i < types.size(); ++i) {
      if (types[i]) {
        result += types[i]->toString();
      } else {
        result += "<null-type>";
      }
      if (i + 1 < types.size()) {
        result += ", ";
      }
    }
    result += ")";
    return result;
  }
};

//TypePathFn → ( TypePathFnInputs? ) ( -> TypeNoBounds )?
class TypePathFn {
 public:
  std::unique_ptr<TypePathFnInputs> type_path_fn_inputs;
  std::unique_ptr<TypeNode> type_no_bounds;

  TypePathFn(std::unique_ptr<TypePathFnInputs> tpfi, std::unique_ptr<TypeNode> tnb) : type_path_fn_inputs(std::move(tpfi)), type_no_bounds(std::move(tnb)) {};
  std::string toString() const {
    std::string result;
    if (type_path_fn_inputs) {
      result += type_path_fn_inputs->toString();
    } else {
      result += "()";
    }

    if (type_no_bounds) {
      result += " -> " + type_no_bounds->toString();
    }
    return result;
}
};

//GenericArgsConst → BlockExpression | LiteralExpression | - LiteralExpression | SimplePathSegment
class GenericArgsConst {

};

//GenericArgsBinding → IDENTIFIER GenericArgs? = Type
class GenericArgsBinding {

};

//GenericArgsBounds → IDENTIFIER GenericArgs? : TypeParamBounds
class GenericArgsBounds {

};

//GenericArg → Lifetime | Type | GenericArgsConst | GenericArgsBinding | GenericArgsBounds
class GenericArg {

};

//GenericArgs →  < > | < ( GenericArg , )* GenericArg ,? >
class GenericArgs {
 public:
  
};

//TypePathSegment → PathIdentSegment ( ::? ( GenericArgs | TypePathFn ) )?
class TypePathSegment {
 public:
  std::unique_ptr<PathIdentSegment> path_ident_segment;
  std::unique_ptr<TypePathFn> type_path_fn;

  TypePathSegment(std::unique_ptr<PathIdentSegment> pis, std::unique_ptr<TypePathFn> tpf)
                : path_ident_segment(std::move(pis)), type_path_fn(std::move(tpf)) {};
  TypePathSegment(std::string id) : path_ident_segment(std::make_unique<PathIdentSegment>(id)) {};
  std::string toString() const {
    std::string res = path_ident_segment->toString();
    if (type_path_fn) {
      res += "::";
      res += type_path_fn->toString();
    }
    return res;
  }
  
};

//TypePath → ::? TypePathSegment ( :: TypePathSegment )*
class TypePath {
 public:
  std::vector<std::unique_ptr<TypePathSegment>> segments;

  TypePath() = default;
  TypePath(std::vector<std::unique_ptr<TypePathSegment>> s) : segments(std::move(s)) {}; 
  TypePath(std::vector<std::string> strings) {
    std::vector<std::unique_ptr<TypePathSegment>> s;
    for (int i = 0; i < strings.size(); i++) {
      s.push_back(std::move(std::make_unique<TypePathSegment>(strings[i])));
    }
    segments = std::move(s);
  }

  std::string toString() const {
    std::string res;
    for (int i = 0; i < segments.size(); i++) {
      if (i > 0) {
        res += "::";
      }
      res += segments[i]->toString();
    }
    return res;
  };
};

//TraitBound →  ( ? | ForLifetimes )? TypePath | ( ( ? | ForLifetimes )? TypePath )
class TraitBound {
 public:
 
};

//TypeParamBound → TraitBound | UseBound
class TypeParamBound {

};

//TypeParambounds→ TypeParamBound ( + TypeParamBound )* +?
class TypeParamBounds {
 public:
  
  
};

//TypeParam→ IDENTIFIER ( : TypeParamBounds? )? ( = Type )?
class TypeParam {
 public:
  std::string identifier;
  std::optional<TypeParamBounds> type_param_bounds;
  std::unique_ptr<TypeNode> type;
};

//GenericParam → OuterAttribute* ( LifetimeParam | TypeParam | ConstParam )
class GenericParam {
 public:
  
};

//GenericParams → < ( GenericParam ( , GenericParam )* ,? )? >
class GenericParams {
 public:
  std::vector<std::unique_ptr<GenericParam>> generic_params;

  GenericParams(std::vector<std::unique_ptr<GenericParam>> gp) : generic_params(std::move(gp)) {};
};

//AssociatedItem → ( Visibility? ( ConstantItem | Function ) )
class AssociatedItemNode : ItemNode {
 public:
  //Visibility visibility;
  std::variant<std::unique_ptr<ConstantItemNode>, std::unique_ptr<FunctionNode>> associated_item;

  AssociatedItemNode(std::unique_ptr<ConstantItemNode> con, int l, int c) : associated_item(std::move(con)), ItemNode(NodeType::AssociatedItem, l, c) {};
  AssociatedItemNode(std::unique_ptr<FunctionNode> con, int l, int c) : associated_item(std::move(con)), ItemNode(NodeType::AssociatedItem, l, c) {};
};

/*
class for Implementation node
*/

//InherentImpl → impl GenericParams? Type WhereClause? { AssociatedItem*}

//Implementation → InherentImpl | TraitImpl
class InherentImplNode : public ItemNode {
 public:
  std::unique_ptr<TypeNode> type;
  std::vector<std::unique_ptr<AssociatedItemNode>> associated_item;

  InherentImplNode(std::unique_ptr<TypeNode> t, std::vector<std::unique_ptr<AssociatedItemNode>> ai, int l, int c) 
                    : type(std::move(t)), associated_item(std::move(ai)), ItemNode(NodeType::InherentImplementation, l, c) {};
};

//TraitImpl → unsafe? impl !? TypePath for Type { AssociatedItem* }

class TraitImplNode : public ItemNode {
 public:
  bool isUnsafe;  
  bool isNegative;
  std::unique_ptr<TypePath> traitType;
  std::unique_ptr<TypeNode> forType;  
  std::vector<std::unique_ptr<AssociatedItemNode>> associatedItems;

  TraitImplNode(bool unsafeFlag, bool negativeFlag, std::unique_ptr<TypePath> trait,std::unique_ptr<TypeNode> targetType, 
                std::vector<std::unique_ptr<AssociatedItemNode>> items, int l, int c)
    : isUnsafe(unsafeFlag), isNegative(negativeFlag), traitType(std::move(trait)), forType(std::move(targetType)),
      associatedItems(std::move(items)), ItemNode(NodeType::TraitImplementation, l, c) {}
};

//GenericParams → < ( GenericParam ( , GenericParam )* ,? )? >
class GenParaNode : public ItemNode {
 public:
  std::vector<std::unique_ptr<GenericParam>> generic_params;

  GenParaNode(std::vector<std::unique_ptr<GenericParam>> gp, int l, int c) : generic_params(std::move(gp)), ItemNode(NodeType::GenPara, l, c) {};
};


/*
class of statements
*/

class PatternWithoutRange;
class BlockExpressionNode;
class IdentifierPattern;
//LetStatement → let PatternNoTopAlt ( : Type )? ( = Expression | = Expression except LazyBooleanExpression or end with a } else BlockExpression)? ;
class LetStatement {
 public:
  std::unique_ptr<PatternNoTopAlt> pattern;
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<ExpressionNode> expression; //except LazyBooleanExpression or end with a }
  std::unique_ptr<BlockExpressionNode> block_expression;

  LetStatement(std::unique_ptr<PatternNoTopAlt> p, std::unique_ptr<TypeNode> t, std::unique_ptr<ExpressionNode> e, std::unique_ptr<BlockExpressionNode> be)
            : pattern(std::move(p)), type(std::move(t)), expression(std::move(e)), block_expression(std::move(be)) {};

  bool get_if_mutable();
};

//ExpressionStatement → ExpressionWithoutBlock ; | ExpressionWithBlock ;?
class ExpressionStatement {
 public:
  std::unique_ptr<ExpressionNode> expression;

  ExpressionStatement(std::unique_ptr<ExpressionNode> e) : expression(std::move(e)) {};
};

//Statement →  ; | Item  | LetStatement | ExpressionStatement
enum StatementType {
  SEMICOLON, ITEM, LETSTATEMENT, EXPRESSIONSTATEMENT
};
class StatementNode : public ASTNode {
 public:
  StatementType type;
  std::unique_ptr<ItemNode> item; //item
  std::unique_ptr<LetStatement> let_statement; //letstatement
  std::unique_ptr<ExpressionStatement> expr_statement; //ExpressionStatement
  bool check() {
    switch (type) {
      case(SEMICOLON) : {
        if (item == nullptr && let_statement == nullptr && expr_statement == nullptr) {
          return true;
        }
        return false;
      }
      case(ITEM) : {
        if (item != nullptr && let_statement == nullptr && expr_statement == nullptr) {
          return true;
        }
        return false;
      }
      case(LETSTATEMENT) : {
        if (item == nullptr && let_statement != nullptr && expr_statement == nullptr) {
          return true;
        }
        return false;
      }
      case(EXPRESSIONSTATEMENT) : {
        if (item == nullptr && let_statement == nullptr && expr_statement != nullptr) {
          return true;
        }
        return false;
      }
      default: return false;
    }
  };

  StatementNode(StatementType t, std::unique_ptr<ItemNode> i, std::unique_ptr<LetStatement> ls, std::unique_ptr<ExpressionStatement> e, int l, int c)
            : type(t), item(std::move(i)), let_statement(std::move(ls)), expr_statement(std::move(e)), ASTNode(NodeType::Statement, l, c) {};
};

/*
Expression Nodes
*/
bool isHexString(const std::string &s) {
  if (s.empty()) return false;
  for (char c : s) {
    if (!std::isxdigit(static_cast<unsigned char>(c))) {
      return false;
    }
  }
  return true;
}

/*
classes for LiteralExpression
*/
//SUFFIX → IDENTIFIER_OR_KEYWORD
//SUFFIX_NO_E → SUFFIXnot beginning with e or E
class SUFFIX {
 public:
  std::string suffix;

  SUFFIX(std::string s) : suffix(s) {};
  bool check() {
    if (IntSuffixes.find(suffix) != IntSuffixes.end()
      ||FloatSuffixes.find(suffix) != FloatSuffixes.end()
      ||UintSuffixes.find(suffix) != UintSuffixes.end()) return true;
    return false;
  }
};

//CHAR_LITERAL →'( ~[' \ LF CR TAB] | QUOTE_ESCAPE | ASCII_ESCAPE ) ' SUFFIX? QUOTE_ESCAPE → \' | \" ASCII_ESCAPE → \x OCT_DIGIT HEX_DIGIT | \n | \r | \t | \\ | \0
class char_literal {
 public:
  char value;
  std::string raw;

  char parseEscape(const std::string &esc) {
    if (esc == "\\n") return '\n';
    if (esc == "\\r") return '\r';
    if (esc == "\\t") return '\t';
    if (esc == "\\\\") return '\\';
    if (esc == "\\0") return '\0';
    if (esc == "\\\'") return '\'';
    if (esc == "\\\"") return '\"';
    if (esc.size() >= 3 && esc[1] == 'x') {
      int num = std::stoi(esc.substr(2), nullptr, 16);
      return static_cast<char>(num);
    } else {
      throw std::runtime_error("Patse Escpase Error");
    }
  }

  char_literal(const std::string &literal) : raw(literal) {
    std::string inner = raw.substr(1, raw.size() - 2);
    if (inner.size() == 1) {
      value = inner[0];
    } else if (inner[0] == '\\') {
      value = parseEscape(inner);
    }
  }

  bool check() {
    if (raw.size() < 3 || raw.front() != '\'' || raw.back() != '\'') {
      return false;
    }
    std::string inner = raw.substr(1, raw.size() - 2);

    if (inner.size() == 1) {
      char c = inner[0];
      if (c == '\'' || c == '\\' || c == '\n' || c == '\r' || c == '\t') {
        return false; 
      }
      return true;
    }
    if (inner.size() >= 2 && inner[0] == '\\') {
      if (inner == "\\n" || inner == "\\r" || inner == "\\t" ||
        inner == "\\\\" || inner == "\\0" || inner == "\\\'" || inner == "\\\"") {
        return true;
      }
      if (inner.size() >= 3 && inner[1] == 'x') {
        std::string hex = inner.substr(2);
        if (!hex.empty() && isHexString(hex)) {
          return true;
        }
      }
      return false;
    }
    return false;
  }
};

//STRING_CONTINUE → \ LF


//STRING_LITERAL → " ( ~[" \ CR] | QUOTE_ESCAPE | ASCII_ESCAPE | STRING_CONTINUE )* " SUFFIX?
class string_literal {
 public:
  std::string raw;
  std::string value; 

  string_literal(const std::string& rawLiteral) : raw(rawLiteral) {
    value = parseString(rawLiteral);
  }

  bool check() {
    if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"') {
      return false;
    }

    for (size_t i = 1; i + 1 < raw.size(); ++i) {
      char c = raw[i];
      if (c != '"' && c != '\\' && c != '\r') {
        continue;
      }
      if (c == '\\') {
        if (i + 1 >= raw.size() - 1) return false;
        char next = raw[i + 1];
        switch (next) {
          case 'n': case 'r': case 't':
          case '\\': case '"': case '0': { i++; break; }
          case 'x': {
            if (i + 3 >= raw.size() - 1) return false;
            if (!isxdigit(raw[i + 2]) || !isxdigit(raw[i + 3])) return false;
            i += 3;
            break;
          }
          case '\n': { i++; break; }
          default: return false;
        }
      }
      if (c == '"') return false;
      if (c == '\r') return false;
    }
    return true;
  }

 private:
  std::string parseString(const std::string& s) {
    std::string result;
    for (int i = 0; i < s.size(); ++i) {
      if (s[i] == '\\' && i + 1 < s.size()) {
        switch (s[i + 1]) {
          case 'n': result.push_back('\n'); break;
          case 'r': result.push_back('\r'); break;
          case 't': result.push_back('\t'); break;
          case '\\': result.push_back('\\'); break;
          case '"': result.push_back('"'); break;
          case '0': result.push_back('\0'); break;
          default: result.push_back(s[i + 1]); break;
        }
        i++;
      } else {
        result.push_back(s[i]);
      }
    }
    return result;
  }
};

//RAW_STRING_LITERAL → r RAW_STRING_CONTENT SUFFIX?
//RAW_STRING_CONTENT → " ( ~CR )* (non-greedy) " | # RAW_STRING_CONTENT #
class raw_string_literal {
 public:
  std::string raw;   
  std::string value;

  raw_string_literal(const std::string& rawLiteral) : raw(rawLiteral) {
    parseRaw(rawLiteral);
  }

  bool check() {
    for (char c : value) {
      if (c == '\r') return false;
    }
    return true;
  }

 private:
  void parseRaw(const std::string& s) {
    value.clear();
    if (s.size() < 2 || s[0] != 'r') return;
    size_t pos = 1;
    size_t hashCount = 0;
    while (pos < s.size() && s[pos] == '#') { ++hashCount; ++pos; }
    if (pos >= s.size() || s[pos] != '"') return;
    ++pos;
    std::string endMarker = "\"";
    for (size_t i = 0; i < hashCount; ++i) endMarker += '#';
    size_t endPos = pos;
    bool foundEnd = false;
    while (endPos + endMarker.size() <= s.size()) {
      bool match = true;
      for (size_t i = 0; i < endMarker.size(); ++i) {
        if (s[endPos + i] != endMarker[i]) { match = false; break; }
      }
      if (match) { foundEnd = true; break; }
      ++endPos;
    }
    if (!foundEnd) return;
    value = s.substr(pos, endPos - pos);
  }
};

//C_STRING_LITERAL → c" ( ~[" \ CR NUL] | BYTE_ESCAPEexcept \0 or \x00 | STRING_CONTINUE )* " SUFFIX?
class c_string_literal {
 public:
  std::string raw;
  std::string value;

  c_string_literal(const std::string& rawLiteral) : raw(rawLiteral) {
    value = parseString(rawLiteral);
  }

  bool check() {
    if (raw.size() < 3 || raw[0] != 'c' || raw[1] != '"' || raw[raw.size() - 1] != '"') {
      return false;
    }

    for (size_t i = 2; i + 1 < raw.size(); ++i) {
      char c = raw[i];
      if (c == '"' || c == '\r' || c == '\0') return false;

      if (c == '\\') {
        if (i + 1 < raw.size() && raw[i + 1] == '\n') {
          i++;
          continue;
        }
        if (i + 1 >= raw.size() - 1) return false;
        char next = raw[i + 1];
        switch (next) {
          case 'n': case 'r': case 't':
          case '\\': case '"': case '0': { i++; break; }
          case 'x': {
            if (i + 3 >= raw.size() - 1) return false;
            if (!isxdigit(raw[i + 2]) || !isxdigit(raw[i + 3])) return false;
            i += 3;
            break;
          }
          default: return false;
        }
      }
    }
    return true;
  }

 private:
  std::string parseString(const std::string& s) {
    std::string result;
    for (size_t i = 2; i < s.size() - 1; ++i) { // 跳过 c" ... "
      if (s[i] == '\\' && i + 1 < s.size()) {
        char next = s[i + 1];
        switch (next) {
          case 'n': result.push_back('\n'); break;
          case 'r': result.push_back('\r'); break;
          case 't': result.push_back('\t'); break;
          case '\\': result.push_back('\\'); break;
          case '"': result.push_back('"'); break;
          case '0': result.push_back('\0'); break;
          case '\n': break;
          case 'x': {
            if (i + 3 < s.size()) {
              char hex[3] = { s[i + 2], s[i + 3], 0 };
              result.push_back((char)std::strtol(hex, nullptr, 16));
              i += 2;
            }
            break;
          }
          default: result.push_back(next); break;
        }
        i++;
      } else {
        result.push_back(s[i]);
      }
    }
    return result;
  }
};

//RAW_C_STRING_LITERAL →  cr RAW_C_STRING_CONTENT SUFFIX?
//RAW_C_STRING_CONTENT → " ( ~[CR NUL] )* (non-greedy) " | # RAW_C_STRING_CONTENT #
class raw_c_string_literal {
 public:
  std::string raw;
  std::string value;

  raw_c_string_literal(const std::string& rawLiteral) : raw(rawLiteral) {
    value = parse(rawLiteral);
  }

  bool check() {
    for (char c : value) {
      if (c == '\r' || c == '\0') return false;
    }
    return true;
  }

 private:
  std::string parse(const std::string& s) {
    std::string result;
    if (s.size() < 3 || s[0] != 'c' || s[1] != 'r') return result;
    size_t pos = 2;
    size_t hashCount = 0;
    while (pos < s.size() && s[pos] == '#') { ++hashCount; ++pos; }
    if (pos >= s.size() || s[pos] != '"') return result;
    ++pos;
    std::string endMarker = "\"";
    for (size_t i = 0; i < hashCount; ++i) endMarker += '#';
    size_t endPos = pos;
    bool foundEnd = false;
    while (endPos + endMarker.size() <= s.size()) {
      bool match = true;
      for (size_t i = 0; i < endMarker.size(); ++i) {
        if (s[endPos + i] != endMarker[i]) { match = false; break; }
      }
      if (match) { foundEnd = true; break; }
      ++endPos;
    }
    if (!foundEnd) return result;
    result = s.substr(pos, endPos - pos);
    return result;
  }
};

//INTEGER_LITERAL → ( DEC_LITERAL | BIN_LITERAL | OCT_LITERAL | HEX_LITERAL ) SUFFIX_NO_E?
//DEC_LITERAL → DEC_DIGIT ( DEC_DIGIT | _ )*
//BIN_LITERAL → 0b ( BIN_DIGIT | _ )* BIN_DIGIT ( BIN_DIGIT | _ )*
//OCT_LITERAL → 0o ( OCT_DIGIT | _ )* OCT_DIGIT ( OCT_DIGIT | _ )*
//HEX_LITERAL → 0x ( HEX_DIGIT | _ )* HEX_DIGIT ( HEX_DIGIT | _ )*
//BIN_DIGIT → [0-1]
//OCT_DIGIT → [0-7]
//DEC_DIGIT → [0-9]
//HEX_DIGIT → [0-9 a-f A-F]
class integer_literal {
 public:
  std::string raw;
  std::string value;
  int base;   

  integer_literal(const std::string& rawLiteral) : raw(rawLiteral), base(10) {
    value = parse(rawLiteral);
  }

  bool check() {
    if (value.empty()) return false;
    for (char c : value) {
      if (base == 2 && (c != '0' && c != '1')) return false;
      if (base == 8 && (c < '0' || c > '7')) return false;
      if (base == 10 && !std::isdigit((unsigned char)c)) return false;
      if (base == 16 && !std::isxdigit((unsigned char)c)) return false;
    }
    return true;
  }

 private:
  std::string parse(const std::string& s) {
    std::string result;
    size_t pos = 0;
    base = 10;

    if (s.size() >= 2 && s[0] == '0') {
      char prefix = s[1];
      if (prefix == 'b' || prefix == 'B') { base = 2; pos = 2; }
      else if (prefix == 'o' || prefix == 'O') { base = 8; pos = 2; }
      else if (prefix == 'x' || prefix == 'X') { base = 16; pos = 2; }
    }
    for (; pos < s.size(); ++pos) {
      char c = s[pos];
      if (c == '_') continue;
      if (pos == s.size() - 3 && c == 'u' && s[pos + 1] == '3' && s[pos + 2] == '2') break;
      result.push_back(c);
    }
    return result;
  }
};

//FLOAT_LITERAL → DEC_LITERAL .not immediately followed by ., _ or an ASCII_ALPHA character | DEC_LITERAL . DEC_LITERAL SUFFIX_NO_E?
class float_literal {
public:
  std::string raw; 
  std::string value;

  float_literal(const std::string& rawLiteral) : raw(rawLiteral) {
    value = parse(rawLiteral);
  }

  bool check() {
    if (value.empty()) return false;
    bool dotFound = false;
    for (size_t i = 0; i < value.size(); ++i) {
      char c = value[i];
      if (c == '.') {
        if (dotFound) return false;
        dotFound = true;
      } else if (!std::isdigit((unsigned char)c)) {
        return false;
      }
    }
    return dotFound;
  }

 private:
  std::string parse(const std::string& s) {
    std::string result;
    size_t pos = 0;
    while (pos < s.size()) {
      char c = s[pos];
      if (c == '_') { ++pos; continue; }
      if (std::isdigit((unsigned char)c) || c == '.') result.push_back(c);
      else break;
      ++pos;
    }
    return result;
  }
};

//ContinueExpression → continue
class ContinueExpressionNode : public ExpressionNode {
  public:
   ContinueExpressionNode(int l, int c) : ExpressionNode(NodeType::ContinueExpression, l, c) {};
};

//LiteralExpression → CHAR_LITERAL | STRING_LITERAL | RAW_STRING_LITERAL | C_STRING_LITERAL | RAW_C_STRING_LITERAL | INTEGER_LITERAL | FLOAT_LITERAL | true | false
class LiteralExpressionNode : public ExpressionNode {
 public:
  std::variant<std::unique_ptr<char_literal>, std::unique_ptr<string_literal>, std::unique_ptr<raw_string_literal>, std::unique_ptr<c_string_literal>, 
               std::unique_ptr<raw_c_string_literal>, std::unique_ptr<integer_literal>, std::unique_ptr<float_literal>, std::unique_ptr<bool>> literal;
  template <typename T>
  LiteralExpressionNode(std::unique_ptr<T> lit, int l, int c) : literal(std::move(lit)), ExpressionNode(NodeType::LiteralExpression, l, c) {}

  template <typename T>
  LiteralExpressionNode(T* lit, int l, int c) : literal(std::unique_ptr<T>(lit)), ExpressionNode(NodeType::LiteralExpression, l, c) {}
  std::string toString() {
    return std::visit([](auto& litPtr) -> std::string {
      using T = std::decay_t<decltype(litPtr)>;
      if constexpr (std::is_same_v<T, std::unique_ptr<char_literal>>) {
        return std::string("'") + litPtr->value + "'";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<string_literal>>) {
        return "\"" + litPtr->value + "\"";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<raw_string_literal>>) {
        return "r\"" + litPtr->value + "\"";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<c_string_literal>>) {
        return "c\"" + litPtr->value + "\"";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<raw_c_string_literal>>) {
        return "cr\"" + litPtr->value + "\"";
      } else if constexpr (std::is_same_v<T, std::unique_ptr<integer_literal>>) {
        return litPtr->value;
      } else if constexpr (std::is_same_v<T, std::unique_ptr<float_literal>>) {
        return litPtr->value;
      } else if constexpr (std::is_same_v<T, std::unique_ptr<bool>>) {
        return *litPtr ? "true" : "false";
      } else {
        return "<unknown literal>";
      }
    }, literal);
  }
};

class BreakExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expr;

  BreakExpressionNode(std::unique_ptr<ExpressionNode> e, int l, int c) : expr(std::move(e)), ExpressionNode(NodeType::BreakExpression, l, c) {};
};

class PathExpressionNode;
class OperatorExpressionNode;
class GroupedExpressionNode;
class ArrayExpressionNode;
class IndexExpressionNode;
class TupleExpressionNode;
class TupleIndexingExpressionNode;
class StructExpressionNode;
class CallExpressionNode;
class MethodCallExpressionNode;
class FieldExpressionNode;
class RangeExpressionNode;
class ReturnExpressionNode;
class UnderscoreExpressionNode;
class LazyBooleanExpressionNode;
class ArithmeticOrLogicalExpressionNode;
class TypeCastExpressionNode;
class NegationExpressionNode;
class DereferenceExpressionNode;

//ExpressionWithoutBlock → LiteralExpression | PathExpression | OperatorExpression | GroupedExpression | ArrayExpression | AwaitExpression
//                        | IndexExpression | TupleExpression | TupleIndexingExpression | StructExpression | CallExpression | MethodCallExpression
//                        | FieldExpression | ClosureExpression | AsyncBlockExpression | ContinueExpression | BreakExpression
//                        | RangeExpression | ReturnExpression | UnderscoreExpression | MacroInvocation
class ExpressionWithoutBlockNode : public ExpressionNode {
 public:
  std::variant<std::unique_ptr<LiteralExpressionNode>, std::unique_ptr<PathExpressionNode>, std::unique_ptr<OperatorExpressionNode>, std::unique_ptr<NegationExpressionNode>,
              std::unique_ptr<GroupedExpressionNode>, std::unique_ptr<ArrayExpressionNode>, std::unique_ptr<IndexExpressionNode>, std::unique_ptr<TypeCastExpressionNode>,
              std::unique_ptr<TupleExpressionNode>, std::unique_ptr<TupleIndexingExpressionNode>, std::unique_ptr<StructExpressionNode>, std::unique_ptr<BreakExpressionNode>,
              std::unique_ptr<CallExpressionNode>, std::unique_ptr<MethodCallExpressionNode>, std::unique_ptr<FieldExpressionNode>, std::unique_ptr<ArithmeticOrLogicalExpressionNode>,
              std::unique_ptr<RangeExpressionNode>, std::unique_ptr<ReturnExpressionNode>, std::unique_ptr<UnderscoreExpressionNode>, std::unique_ptr<LazyBooleanExpressionNode>, std::unique_ptr<DereferenceExpressionNode>> expr;

  template <typename T>
  ExpressionWithoutBlockNode(std::unique_ptr<T> node, int l, int c)
      : ExpressionNode(NodeType::ExpressionWithoutBlock, l, c), expr(std::move(node)) {}
};

//BlockExpression → { Statements? }
//Statements → Statement+ | Statement+ ExpressionWithoutBlock | ExpressionWithoutBlock
class BlockExpressionNode : public ExpressionNode {
 public:
  bool if_empty = false;
  std::vector<std::unique_ptr<StatementNode>> statement;
  std::unique_ptr<ExpressionWithoutBlockNode> expression_without_block = nullptr;

  BlockExpressionNode(int l, int c) : if_empty(true), ExpressionNode(NodeType::BlockExpression, l, c) {};
  BlockExpressionNode(std::vector<std::unique_ptr<StatementNode>> s, std::unique_ptr<ExpressionWithoutBlockNode> e, int l, int c)
                    : statement(std::move(s)), expression_without_block(std::move(e)), ExpressionNode(NodeType::BlockExpression, l, c) {};
};

//OperatorExpression → BorrowExpression | DereferenceExpression | NegationExpression | ArithmeticOrLogicalExpression | ComparisonExpression 
//                    | LazyBooleanExpression | TypeCastExpression | AssignmentExpression | CompoundAssignmentExpression
class BorrowExpressionNode;
class NegationExpressionNode;
class ArithmeticOrLogicalExpressionNode;
class ComparisonExpressionNode;
class LazyBooleanExpressionNode;
class TypeCastExpressionNode;
class AssignmentExpressionNode;
class CompoundAssignmentExpressionNode;
class OperatorExpressionNode : public ExpressionNode {
 public:
  std::variant<std::unique_ptr<BorrowExpressionNode>, std::unique_ptr<DereferenceExpressionNode>, std::unique_ptr<NegationExpressionNode>, std::unique_ptr<ArithmeticOrLogicalExpressionNode>, 
              std::unique_ptr<ComparisonExpressionNode>, std::unique_ptr<LazyBooleanExpressionNode>, std::unique_ptr<TypeCastExpressionNode>, std::unique_ptr<AssignmentExpressionNode>, 
              std::unique_ptr<CompoundAssignmentExpressionNode>> operator_expression;

  template <typename T>
  OperatorExpressionNode(T expr, int l, int c) : operator_expression(std::move(expr)), ExpressionNode(NodeType::OperatorExpression, l, c) {};
};

//BorrowExpression → ( & | && ) Expression | ( & | && ) mut Expression | ( & | && ) raw const Expression | ( & | && ) raw mut Expression
class BorrowExpressionNode : public ExpressionNode {
 public:
  int and_count = 0; //1 or 2
  bool if_mut = false; //有无mut
  bool if_const = false;
  bool if_raw = false;
  std::unique_ptr<ExpressionNode> expression;

  BorrowExpressionNode(int count, bool im, bool ic, bool ir, std::unique_ptr<ExpressionNode> expr, int l, int c) 
                      : and_count(count), if_mut(im), if_const(ic), if_raw(ir), expression(std::move(expr)), ExpressionNode(NodeType::BorrowExpression, l, c) {};
};

//DereferenceExpression → * Expression
class DereferenceExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;

  DereferenceExpressionNode(std::unique_ptr<ExpressionNode> expr, int l, int c) 
                          : expression(std::move(expr)), ExpressionNode(NodeType::DereferenceExpression, l, c) {};
};

//NegationExpression → - Expression | ! Expression
class NegationExpressionNode : public ExpressionNode {
 public:
  enum NegationType {
    MINUS, BANG
  };
  NegationType type;
  std::unique_ptr<ExpressionNode> expression;

  NegationExpressionNode(NegationType t, std::unique_ptr<ExpressionNode> expr, int l, int c)
                        : type(t), expression(std::move(expr)), ExpressionNode(NodeType::NegationExpression, l, c) {};
};


//ArithmeticOrLogicalExpression → Expression + Expression | Expression - Expression | Expression * Expression | Expression / Expression | Expression % Expression 
//                              | Expression & Expression | Expression | Expression | Expression ^ Expression | Expression << Expression | Expression >> Expression
class ArithmeticOrLogicalExpressionNode : public ExpressionNode {
 public:
  OperationType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  ArithmeticOrLogicalExpressionNode(OperationType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::ArithmeticOrLogicalExpression, l, c) {};
};

//ComparisonExpression → Expression == Expression | Expression != Expression | Expression > Expression | Expression < Expression | Expression >= Expression | Expression <= Expression
class ComparisonExpressionNode : public ExpressionNode {
 public:
  ComparisonType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  ComparisonExpressionNode(ComparisonType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::ComparisonExpression, l, c) {};
};

enum LazyBooleanType {
  LAZY_OR, LAZY_AND
};
//LazyBooleanExpression → Expression || Expression | Expression && Expression
class LazyBooleanExpressionNode : public ExpressionNode {
 public:
  LazyBooleanType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  LazyBooleanExpressionNode(LazyBooleanType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::LazyBooleanExpression, l, c) {};
};

//TypeCastExpression → Expression as TypeNoBounds
class TypeCastExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;
  std::unique_ptr<TypeNode> type;

  TypeCastExpressionNode(std::unique_ptr<ExpressionNode> expr, std::unique_ptr<TypeNode> t, int l, int c) 
                      : expression(std::move(expr)), type(std::move(t)), ExpressionNode(NodeType::TypeCastExpression, l, c) {};
};

//AssignmentExpression → Expression = Expression
class AssignmentExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression1, expression2;

  AssignmentExpressionNode(std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::AssignmentExpression, l, c) {};
};

//CompoundAssignmentExpression → Expression += Expression | Expression -= Expression | Expression *= Expression | Expression /= Expression | Expression %= Expression
//                              | Expression &= Expression | Expression |= Expression | Expression ^= Expression | Expression <<= Expression | Expression >>= Expression
class CompoundAssignmentExpressionNode : public ExpressionNode {
 public: 
  OperationType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  CompoundAssignmentExpressionNode(OperationType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                        : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::CompoundAssignmentExpression, l, c) {};
};

//GroupedExpression → ( Expression )
class GroupedExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;

  GroupedExpressionNode(std::unique_ptr<ExpressionNode> expr, int l, int c) : expression(std::move(expr)), ExpressionNode(NodeType::GroupedExpression, l, c) {};
};

enum ArrayExpressionType {
  LITERAL, REPEAT
};
//ArrayExpression → [ ArrayElements? ]
//ArrayElements → Expression ( , Expression )* ,? | Expression ; Expression
class ArrayExpressionNode : public ExpressionNode {
 public:
  bool if_empty = true;
  ArrayExpressionType type;
  std::vector<std::unique_ptr<ExpressionNode>> expressions;

  ArrayExpressionNode(bool ie, ArrayExpressionType t, std::vector<std::unique_ptr<ExpressionNode>> expr, int l, int c)
                    : if_empty(ie), type(t), expressions(std::move(expr)), ExpressionNode(NodeType::ArrayExpression, l, c) {};

  bool check() {
    if (if_empty) {
      if (expressions.empty()) return true;
      return false;
    } else {
      if (expressions.empty()) return false;
      if (type == ArrayExpressionType::REPEAT && expressions.size() != 2) return false;  
      if (type == ArrayExpressionType::REPEAT && expressions.size() == 2 && expressions[1]->get_type() != NodeType::LiteralExpression) return false;
      if (type == ArrayExpressionType::REPEAT && expressions.size() == 2 && expressions[1]->get_type() == NodeType::LiteralExpression) {
        if (std::holds_alternative<std::unique_ptr<integer_literal>>(dynamic_cast<LiteralExpressionNode*>(expressions[1].get())->literal)) return true;
        return false;
      }
      return true;  
    }
  }
};

//IndexExpression → Expression [ Expression ]
class IndexExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> base;
  std::unique_ptr<ExpressionNode> index;

  IndexExpressionNode(std::unique_ptr<ExpressionNode> b, std::unique_ptr<ExpressionNode> i, int l, int c) : base(std::move(b)), index(std::move(i)), ExpressionNode(NodeType::IndexExpression, l, c) {}

  bool check() {
    return base != nullptr && index != nullptr;
  }
};

//TupleExpression → ( TupleElements? )
//TupleElements → ( Expression , )+ Expression?
class TupleExpressionNode : public ExpressionNode {
public:
  std::vector<std::unique_ptr<ExpressionNode>> expressions;

  TupleExpressionNode(std::vector<std::unique_ptr<ExpressionNode>> expr, int l, int c) : expressions(std::move(expr)), ExpressionNode(NodeType::TupleExpression, l, c) {}
};

//TupleIndexingExpression → Expression . TUPLE_INDEX
//TUPLE_INDEX → INTEGER_LITERAL
class TupleIndexingExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;
  integer_literal tuple_index;

  TupleIndexingExpressionNode(std::unique_ptr<ExpressionNode> expr, integer_literal ti, int l, int c) : expression(std::move(expr)), tuple_index(ti), ExpressionNode(NodeType::TupeIndexingExpression, l , c) {};
};

enum PathInType {
  SUPER, self, Self, CRATE, $CRATE
};
//PathInExpression → ::? PathExprSegment ( :: PathExprSegment )*
//PathExprSegment → PathIdentSegment
//PathIdentSegment → IDENTIFIER | super | self | Self | crate | $crate
class PathInExpression {
 public:
  std::vector<std::variant<PathInType, Identifier>> segments;

  PathInExpression(std::vector<std::variant<PathInType, Identifier>> seg) : segments(std::move(seg)) {};

  std::string toString() {
    std::string ans = "";
    for (int i = 0; i < segments.size(); i++) {
      if (i > 0) ans += "::";

      std::visit([&ans](auto &seg) {
        using T = std::decay_t<decltype(seg)>;
        if constexpr (std::is_same_v<T, PathInType>) {
            switch (seg) {
              case PathInType::SUPER : ans += "super"; break;
              case PathInType::self : ans += "self"; break;
              case PathInType::Self : ans += "Self"; break;
              case PathInType::CRATE : ans += "crate"; break;
              case PathInType::$CRATE : ans += "$crate"; break;
            }
        } else if constexpr (std::is_same_v<T, Identifier>) {
          ans += seg.id;
        }
      }, segments[i]);
    }

    return ans;
  }
};

//QualifiedPathInExpression → QualifiedPathType ( :: PathExprSegment )+
//QualifiedPathType → < Type ( as TypePath )? >
//QualifiedPathInType → QualifiedPathType ( :: TypePathSegment )+
class QualifiedPathInExpression {
 public:
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<TypePath> type_path;
  std::vector<std::variant<PathInType, Identifier>> segments;
  
  QualifiedPathInExpression(std::unique_ptr<TypeNode> t, std::unique_ptr<TypePath> tp, std::vector<std::variant<PathInType, Identifier>> s)
                          : type(std::move(t)), type_path(std::move(tp)), segments(std::move(s)) {};

  std::string toString() {
    return "";
  }
};

class PathExpressionNode : public ExpressionNode {
 public:
  std::variant<std::unique_ptr<PathInExpression>, std::unique_ptr<QualifiedPathInExpression>> path;

  template<typename T>
  PathExpressionNode(T p, int l, int c) : path(std::move(p)), ExpressionNode(NodeType::PathExpression, l, c) {};

  std::string toString() const {
    return std::visit([](auto &ptr) -> std::string {
      if (!ptr) return "<null>";
      return ptr->toString();
    }, path);
  }

  std::string get_type() const {
    std::string res = toString();
    int pos = res.size() - 1;
    for (int i = res.size() - 1; i >= 0; i--) {
      if (res[i] == ':') {
        i -= 2;
        pos = i;
        break;
      }
    }
    std::string ans = "";
    for (int i = 0; i <= pos; i++) {
      ans += res[i];
    }
    return ans;
  }
};

//StructBase → .. Expression
class StructBase {
 public:
  std::unique_ptr<ExpressionNode> expression;

  StructBase(std::unique_ptr<ExpressionNode> expr) : expression(std::move(expr)) {};

  bool check() {
    return expression != nullptr;
  }
};

//StructExprField → IDENTIFIER | ( IDENTIFIER | TUPLE_INDEX ) : Expression
class StructExprField {
 public:
  Identifier id;
  std::variant<Identifier, integer_literal> id_or_tupe_index;
  std::unique_ptr<ExpressionNode> expression;

  StructExprField(Identifier id1, Identifier id2, std::unique_ptr<ExpressionNode> expr) : id(id1), id_or_tupe_index(id2), expression(std::move(expr)) {};
  StructExprField(Identifier id1, integer_literal il, std::unique_ptr<ExpressionNode> expr) : id(id1), id_or_tupe_index(il), expression(std::move(expr)) {};

  bool check() {
    return expression != nullptr;
  }
};

//StructExprFields → StructExprField ( , StructExprField )* ( , StructBase | ,? )
class StructExprFields {
 public:
  std::vector<std::unique_ptr<StructExprField>> struct_expr_fields;
  std::unique_ptr<StructBase> struct_base = nullptr;

  StructExprFields(std::vector<std::unique_ptr<StructExprField>> sef, std::unique_ptr<StructBase> sb) : struct_expr_fields(std::move(sef)), struct_base(std::move(sb)) {};

  bool check() {
    for (int i = 0; i < struct_expr_fields.size(); i++) {
      if (struct_expr_fields[i]->check() == false) return false;
    }
    if (struct_base && struct_base->check() == false) return false;
    return struct_expr_fields.size() >= 1 ;
  }
};

//StructExpression → PathInExpression { ( StructExprFields | StructBase )? }
class StructExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<PathInExpression> pathin_expression = nullptr;
  std::unique_ptr<StructExprFields> struct_expr_fields = nullptr;
  std::unique_ptr<StructBase> struct_base = nullptr;

  StructExpressionNode(std::unique_ptr<PathInExpression> pe, int l, int c) : pathin_expression(std::move(pe)), ExpressionNode(NodeType::StructExpression, l, c) {};
  StructExpressionNode(std::unique_ptr<PathInExpression> pe, std::unique_ptr<StructExprFields> sef, int l, int c)
                    : pathin_expression(std::move(pe)), struct_expr_fields(std::move(sef)), ExpressionNode(NodeType::StructExpression, l, c) {};
  StructExpressionNode(std::unique_ptr<PathInExpression> pe, std::unique_ptr<StructBase> sb, int l, int c)
                    : pathin_expression(std::move(pe)), struct_base(std::move(sb)), ExpressionNode(NodeType::StructExpression, l, c) {};
};
//CallParams → Expression ( , Expression )* ,?
class CallParams {
 public:
  std::vector<std::unique_ptr<ExpressionNode>> expressions;

  CallParams(std::vector<std::unique_ptr<ExpressionNode>> expr) : expressions(std::move(expr)) {};
};

//CallExpression → Expression ( CallParams? )
class CallExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;
  std::unique_ptr<CallParams> call_params = nullptr;

  CallExpressionNode(std::unique_ptr<ExpressionNode> expr, std::unique_ptr<CallParams> cp, int l, int c) 
                  : expression(std::move(expr)), call_params(std::move(cp)), ExpressionNode(NodeType::CallExpression, l, c) {};
};


//MethodCallExpression → Expression . PathExprSegment ( CallParams? )
class MethodCallExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression = nullptr;
  std::variant<PathInType, Identifier> path_expr_segment;
  std::unique_ptr<CallParams> call_params;

  template<typename T>
  MethodCallExpressionNode(std::unique_ptr<ExpressionNode> expr, T pes, std::unique_ptr<CallParams> cp, int l, int c) 
                        : expression(std::move(expr)), path_expr_segment(pes), call_params(std::move(cp)), ExpressionNode(NodeType::MethodCallExpression, l, c) {};
  
  std::string PathtoString() const {

    std::string result;

    result = std::visit([](const auto& seg) -> std::string {
      using T = std::decay_t<decltype(seg)>;
      if constexpr (std::is_same_v<T, PathInType>) {
        switch (seg) {
          case PathInType::SUPER : return "super";
          case PathInType::self : return "self"; 
          case PathInType::Self : return "Self"; 
          case PathInType::CRATE : return "crate";
          case PathInType::$CRATE : return "$crate";
          default : return "<unknown>";
        }
      }
      else if constexpr (std::is_same_v<T, Identifier>) return seg.id;
      else return "<unknown>";
    }, path_expr_segment);

    return result;
  }
};

//FieldExpression → Expression . IDENTIFIER
class FieldExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression = nullptr;
  Identifier identifier;

  FieldExpressionNode(std::unique_ptr<ExpressionNode> expr, Identifier id, int l , int c) : expression(std::move(expr)), identifier(id), ExpressionNode(NodeType::FieldExpression, l, c) {};
};

//LiteralPattern → -? LiteralExpression
class LiteralPattern {
 public:
  bool if_minus = false;
  std::unique_ptr<LiteralExpressionNode> literal = nullptr;

  LiteralPattern(bool im, std::unique_ptr<LiteralExpressionNode> l) : if_minus(im), literal(std::move(l)) {};

  std::string toString() {
    return "LiteralPattern(" + literal->toString() + ')';
  }
};

//IdentifierPattern → ref? mut? IDENTIFIER ( @ PatternNoTopAlt )?
class IdentifierPattern {
 public:
  bool if_ref = false;
  bool if_mut = false;
  Identifier identifier;
  std::unique_ptr<PatternNoTopAlt> pattern_no_top_alt;

  IdentifierPattern(bool ir, bool im, Identifier id, std::unique_ptr<PatternNoTopAlt> pnta) : if_ref(ir), if_mut(im), pattern_no_top_alt(std::move(pnta)), identifier(id) {};

  std::string toString() {
    return identifier.id;
  }
};

//WildcardPattern → _
class WildCardPattern {
 public:
  WildCardPattern() = default;

  std::string toString() {
    return "WildCardPattern(_)";
  }
};

//RestPattern → ..
class RestPattern {
 public:
  RestPattern() = default;

  std::string toString() {
    return "RestPattern(..)";
  }
};

//ReferencePattern → ( & | && ) mut? PatternWithoutRange
class ReferencePattern {
 public:
  int and_count = 0; //1 or 2
  bool if_mut = false;
  std::unique_ptr<PatternWithoutRange> pattern_without_range;

  ReferencePattern(int ac, bool im, std::unique_ptr<PatternWithoutRange> pwr) : and_count(ac), if_mut(im), pattern_without_range(std::move(pwr)) {};

  std::string toString();
};

//StructPattern → PathInExpression { StructPatternElements? }

//StructPatternElements → StructPatternFields ( , | , StructPatternEtCetera )? | StructPatternEtCetera

//StructPatternFields → StructPatternField ( , StructPatternField )*

//StructPatternField → ( TUPLE_INDEX : Pattern | IDENTIFIER : Pattern | ref? mut? IDENTIFIER )

//StructPatternEtCetera → ..

class Pattern;
class StructPatternField {
 public:
  bool if_ref = false;
  bool if_mut = false;
  std::variant<Identifier, integer_literal> identifier_or_tuple_index;
  std::unique_ptr<Pattern> pattern;

  StructPatternField(integer_literal ti, std::unique_ptr<Pattern> p) : identifier_or_tuple_index(ti), pattern(std::move(p)) {};
  StructPatternField(Identifier i, std::unique_ptr<Pattern> p) : identifier_or_tuple_index(i), pattern(std::move(p)) {};
  StructPatternField(bool ir, bool im, Identifier i) : identifier_or_tuple_index(i), if_ref(ir), if_mut(im) {};
};

class StructPattern {
public:
  std::unique_ptr<PathInExpression> path;
  std::vector<std::unique_ptr<StructPatternField>> struct_fields;
  bool hasEtCetera = false;

  StructPattern(std::unique_ptr<PathInExpression> p, std::vector<std::unique_ptr<StructPatternField>> sf, bool ec) : path(std::move(p)), struct_fields(std::move(sf)), hasEtCetera(ec) {};

  std::string toString() {
    return "StructPattern(" + path->toString() + ")";
  }
};

//TupleStructPattern → PathInExpression ( TupleStructItems? )

//TupleStructItems → Pattern ( , Pattern )* ,?
class TupleStructPattern {
 public:
  std::unique_ptr<PathInExpression> path;
  std::vector<std::unique_ptr<Pattern>> patterns;

  TupleStructPattern(std::unique_ptr<PathInExpression> p, std::vector<std::unique_ptr<Pattern>> ps) : path(std::move(p)), patterns(std::move(ps)) {};

  std::string toString() {
    return "TupleStructPattern(" + path->toString() + ")";
  }
};

//TuplePattern → ( TuplePatternItems? )

//TuplePatternItems → Pattern , | RestPattern | Pattern ( , Pattern )+ ,?
class TuplePattern {
 public:
  std::vector<std::unique_ptr<Pattern>> patterns;
  bool if_rest = false;
  std::unique_ptr<RestPattern> rest_pattern;

  TuplePattern(std::vector<std::unique_ptr<Pattern>> p, bool ir) : patterns(std::move(p)), if_rest(ir) {};

  bool check() {
    return !if_rest && patterns.size() > 0 && rest_pattern == nullptr || if_rest && patterns.size() == 0 && rest_pattern != nullptr;
  }

  std::string toString();
};

//GroupedPattern → ( Pattern )
class GroupedPattern {
 public:
  std::unique_ptr<Pattern> pattern;

  GroupedPattern(std::unique_ptr<Pattern> p) : pattern(std::move(p)) {};

  std::string toString();
};


//SlicePattern → [ SlicePatternItems? ]

//SlicePatternItems → Pattern ( , Pattern )* ,?
class SlicePattern {
 public:
  std::vector<std::unique_ptr<Pattern>> patterns;

  SlicePattern(std::vector<std::unique_ptr<Pattern>> p) : patterns(std::move(p)) {};

  std::string toString();
};

//PathPattern → PathExpression
class PathPattern {
 public:
  std::unique_ptr<PathExpressionNode> path;

  PathPattern(std::unique_ptr<PathExpressionNode> p) : path(std::move(p)) {};

  std::string toString() {
    //TODO
    return "PathPattern(" + path->toString() + ")";
  }
};  


//PatternWithoutRange → LiteralPattern | IdentifierPattern | WildcardPattern | RestPattern | ReferencePattern | StructPattern
//                    | TupleStructPattern | TuplePattern | GroupedPattern | SlicePattern | PathPattern
class PatternWithoutRange {
 public:
  std::variant<
    std::unique_ptr<LiteralPattern>,
    std::unique_ptr<IdentifierPattern>,
    std::unique_ptr<WildCardPattern>,
    std::unique_ptr<RestPattern>,
    std::unique_ptr<ReferencePattern>,
    std::unique_ptr<StructPattern>,
    std::unique_ptr<TupleStructPattern>,
    std::unique_ptr<TuplePattern>,
    std::unique_ptr<GroupedPattern>,
    std::unique_ptr<SlicePattern>,
    std::unique_ptr<PathPattern>
  > pattern;

  template<typename T>
  PatternWithoutRange(T p) : pattern(std::move(p)) {};

  std::string toString() const {
    return std::visit([](auto const& ptr) -> std::string {
      using T = std::decay_t<decltype(ptr)>;

      if (!ptr) return "<null>";
      
      return ptr->toString();
    }, pattern);
  }
};

std::string ReferencePattern::toString() {
  return "ReferencePattern(" + pattern_without_range->toString() + ")";
}

//RangePattern → RangeExclusivePattern | RangeInclusivePattern | RangeFromPattern | RangeToExclusivePattern | RangeToInclusivePattern | ObsoleteRangePattern​1
//RangeExclusivePattern → RangePatternBound .. RangePatternBound
//RangeInclusivePattern → RangePatternBound ..= RangePatternBound
//RangeFromPattern → RangePatternBound ..
//RangeToExclusivePattern → .. RangePatternBound
//RangeToInclusivePattern → ..= RangePatternBound
//ObsoleteRangePattern → RangePatternBound ... RangePatternBound
//RangePatternBound → LiteralPattern | PathExpression
class RangePatternBound {
 public:
  std::variant<std::unique_ptr<LiteralPattern>, std::unique_ptr<PathExpressionNode>> value;

  template <typename T>
  RangePatternBound(T v) : value(std::move(v)) {};

  std::string toString() {
    return std::visit([](auto &ptr) -> std::string {
            using T = std::decay_t<decltype(ptr)>;
            if (!ptr) return "<null>";
            return ptr->toString();
          }, value);
  };
};  

class RangeExclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> start;
  std::unique_ptr<RangePatternBound> end;

  RangeExclusivePattern(std::unique_ptr<RangePatternBound> s, std::unique_ptr<RangePatternBound> e) : start(std::move(s)), end(std::move(e)) {};

  std::string toString() {
    return "RangePattern(" + start->toString() + ".." + end->toString() + ")";
  }
};

class RangeInclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> start;
  std::unique_ptr<RangePatternBound> end;

  RangeInclusivePattern(std::unique_ptr<RangePatternBound> s, std::unique_ptr<RangePatternBound> e) : start(std::move(s)), end(std::move(e)) {}
  
  std::string toString() {
    return "RangePattern(" + start->toString() + "..=" + end->toString() + ")";
  }
};

class RangeFromPattern {
 public:
  std::unique_ptr<RangePatternBound> range_pattern_bound;

  RangeFromPattern(std::unique_ptr<RangePatternBound> s) : range_pattern_bound(std::move(s)) {}

  std::string toString() {
    return "RangePattern(" + range_pattern_bound->toString() + "..)";
  }
};

class RangeToExclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> range_pattern_bound;

  RangeToExclusivePattern() = default;
  RangeToExclusivePattern(std::unique_ptr<RangePatternBound> e) : range_pattern_bound(std::move(e)) {}
  
  std::string toString() {
    return "RangePattern(.." + range_pattern_bound->toString() + ")";
  }
};

class RangeToInclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> range_pattern_bound;

  RangeToInclusivePattern(std::unique_ptr<RangePatternBound> e) : range_pattern_bound(std::move(e)) {}
  
  std::string toString() {
    return "RangePattern(..=" + range_pattern_bound->toString() + ")";
  }
};

class ObsoleteRangePattern {
 public:
  std::unique_ptr<RangePatternBound> start;
  std::unique_ptr<RangePatternBound> end;

  ObsoleteRangePattern(std::unique_ptr<RangePatternBound> s, std::unique_ptr<RangePatternBound> e) : start(std::move(s)), end(std::move(e)) {}
  std::string toString() {
    return "RangePattern(" + start->toString() + "..." + end->toString() + ")";
  }
};

class RangePattern {
 public:
  std::variant<
    std::unique_ptr<RangeExclusivePattern>,
    std::unique_ptr<RangeInclusivePattern>,
    std::unique_ptr<RangeFromPattern>,
    std::unique_ptr<RangeToExclusivePattern>,
    std::unique_ptr<RangeToInclusivePattern>,
    std::unique_ptr<ObsoleteRangePattern>
  > value;

  template<typename T>
  RangePattern(T v) : value(std::move(v)) {}

  std::string toString() {
    return std::visit([](auto &ptr) -> std::string {
      if (!ptr) return "<null>";
      return ptr->toString();
    }, value);
  }
};

//PatternNoTopAlt → PatternWithoutRange | RangePattern
class PatternNoTopAlt {
 public:
  std::variant<std::unique_ptr<PatternWithoutRange>, std::unique_ptr<RangePattern>> pattern;

  template<typename T> 
  PatternNoTopAlt(T p) : pattern(std::move(p)) {};

  std::string toString() {
    return std::visit([](auto &ptr) -> std::string {
      if (!ptr) return "<null>";
      return ptr->toString();
    }, pattern);
  }  
};

bool LetStatement::get_if_mutable() {
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
  }, pattern->pattern);
  if (if_mut_pattern) return true;
  return is_type_mutable(type.get()); 
}

//Pattern → |? PatternNoTopAlt ( | PatternNoTopAlt )*
class Pattern {
 public:
  std::vector<std::unique_ptr<PatternNoTopAlt>> patterns;

  Pattern(std::vector<std::unique_ptr<PatternNoTopAlt>> p) : patterns(std::move(p)) {};

  std::string toString() {
    std::string ans;
    for (int i = 0; i < patterns.size(); i++) {
      if (i > 0) ans += '|';
      ans += patterns[i]->toString();
    }
    return ans;
  }
};

std::string TuplePattern::toString() {
  if (!if_rest) {
    std::string ans = "";
    for (int i = 0; i < patterns.size(); i++) {
      if (i > 0) ans += "#";
      ans += patterns[i]->toString();
    }
    return "TuplePattern(" + ans + ")";
  } else {
    return "TuplePattern(" + rest_pattern->toString() + ")";
  }
}

std::string SlicePattern::toString() {
  std::string ans = "";
  for (int i = 0; i < patterns.size(); i++) {
    if (i > 0) ans += "#";
    ans += patterns[i]->toString();
  }
  return "SlicePattern(" + ans + ")";
}

std::string GroupedPattern::toString() {
  return "GroupedPattern(" + pattern->toString() + ")";
}

class RangeExpr;
class RangeFromExpr;
class RangeInclusiveExpr;

//ExcludedConditions → StructExpression | LazyBooleanExpression | RangeExpr | RangeFromExpr 
//                    | RangeInclusiveExpr | AssignmentExpression | CompoundAssignmentExpression
class ExcludedConditions {
 public:
  std::variant<
    std::unique_ptr<StructExpressionNode>,
    std::unique_ptr<LazyBooleanExpressionNode>,
    std::unique_ptr<AssignmentExpressionNode>,
    std::unique_ptr<CompoundAssignmentExpressionNode>,
    std::unique_ptr<RangeExpr>,
    std::unique_ptr<RangeFromExpr>,
    std::unique_ptr<RangeInclusiveExpr>
  > value;

  template<typename T>
  ExcludedConditions(T v) : value(std::move(v)) {};
};

//Scrutinee → Expression except StructExpression

//LetChainCondition → Expression except ExcludedConditions | let Pattern = Scrutinee except ExcludedConditions
class LetChainCondition {
 public:
  std::unique_ptr<ExpressionNode> expression; //Expression or Scrutinee
  std::unique_ptr<Pattern> pattern;
  std::unique_ptr<ExcludedConditions> excluded_conditions;

  LetChainCondition(std::unique_ptr<ExpressionNode> expr, std::unique_ptr<Pattern> p, std::unique_ptr<ExcludedConditions> ec) 
                  : expression(std::move(expr)), pattern(std::move(p)), excluded_conditions(std::move(ec)) {};
};

//LetChain → LetChainCondition ( && LetChainCondition )*
class LetChain {
 public:
  std::vector<std::unique_ptr<LetChainCondition>> let_chain_conditions;

  LetChain(std::vector<std::unique_ptr<LetChainCondition>> lcc) : let_chain_conditions(std::move(lcc)) {};
};

//Conditions → Expression except StructExpression | LetChain
class Conditions {
 public:
  std::variant<std::unique_ptr<ExpressionNode>, LetChain> condition;

  template<typename T>
  Conditions(T con) : condition(std::move(con)) {};
  
  bool check() {
    if (std::holds_alternative<LetChain>(condition)) {
      return true;
    }
   
    auto& expr_ptr = std::get<std::unique_ptr<ExpressionNode>>(condition);
    ExpressionNode* expr = expr_ptr.get();
   
    auto* grouped = dynamic_cast<GroupedExpressionNode*>(expr);
    if (!grouped) return false;
   
    if (!grouped->expression) return false;

    if (auto* assign = dynamic_cast<AssignmentExpressionNode*>(grouped->expression.get())) {
      std::cout << "assignment not allowed in conditions" << std::endl;
      return false;
    }
    return true;
  }
};

//InfiniteLoopExpression → loop BlockExpression
class InfiniteLoopExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<BlockExpressionNode> block_expression;

  InfiniteLoopExpressionNode(std::unique_ptr<BlockExpressionNode> be, int l, int c) : block_expression(std::move(be)), ExpressionNode(NodeType::InfiniteLoopExpression, l, c) {};
};

//PredicateLoopExpression → while Conditions BlockExpression
class PredicateLoopExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<Conditions> conditions;
  std::unique_ptr<BlockExpressionNode> block_expression;

  PredicateLoopExpressionNode(std::unique_ptr<Conditions> con, std::unique_ptr<BlockExpressionNode> be, int l, int c) 
                            : conditions(std::move(con)), block_expression(std::move(be)), ExpressionNode(NodeType::PredicateLoopExpression, l, c) {};
};

//LoopExpression → InfiniteLoopExpression | PredicateLoopExpression
class LoopExpression : public ExpressionNode {
 public:
  std::variant<std::unique_ptr<InfiniteLoopExpressionNode>, std::unique_ptr<PredicateLoopExpressionNode>> loop_expression;

  template<typename T>
  LoopExpression(T le, int l, int c) : loop_expression(std::move(le)), ExpressionNode(NodeType::LoopExpression, l, c) {};
};

// RangeExpr → Expression .. Expression
class RangeExpr {
public:
  std::unique_ptr<ExpressionNode> expr1;
  std::unique_ptr<ExpressionNode> expr2;

  RangeExpr(std::unique_ptr<ExpressionNode> e1, std::unique_ptr<ExpressionNode> e2) : expr1(std::move(e1)), expr2(std::move(e2)) {}
};

// RangeFromExpr → Expression ..
class RangeFromExpr {
public:
  std::unique_ptr<ExpressionNode> expression;

  RangeFromExpr(std::unique_ptr<ExpressionNode> e) : expression(std::move(e)) {}
};

// RangeToExpr → .. Expression
class RangeToExpr {
public:
  std::unique_ptr<ExpressionNode> expression;

  RangeToExpr(std::unique_ptr<ExpressionNode> e) : expression(std::move(e)) {}
};

// RangeFullExpr → ..
class RangeFullExpr {
public:
  RangeFullExpr() = default;
};

// RangeInclusiveExpr → Expression ..= Expression
class RangeInclusiveExpr {
public:
  std::unique_ptr<ExpressionNode> expr1;
  std::unique_ptr<ExpressionNode> expr2;

  RangeInclusiveExpr(std::unique_ptr<ExpressionNode> e1, std::unique_ptr<ExpressionNode> e2) : expr1(std::move(e1)), expr2(std::move(e2)) {}
};

// RangeToInclusiveExpr → ..= Expression
class RangeToInclusiveExpr {
public:
  std::unique_ptr<ExpressionNode> expression;

  RangeToInclusiveExpr(std::unique_ptr<ExpressionNode> e) : expression(std::move(e)) {}
};

//RangeExpression → RangeExpr | RangeFromExpr | RangeToExpr | RangeFullExpr | RangeInclusiveExpr | RangeToInclusiveExpr
class RangeExpressionNode : public ExpressionNode {
public:
  std::variant<
    std::unique_ptr<RangeExpr>,
    std::unique_ptr<RangeFromExpr>,
    std::unique_ptr<RangeToExpr>,
    std::unique_ptr<RangeFullExpr>,
    std::unique_ptr<RangeInclusiveExpr>,
    std::unique_ptr<RangeToInclusiveExpr>
  > value;

  template <typename T>
  RangeExpressionNode(T v, int l, int c) : value(std::move(v)), ExpressionNode(NodeType::RangeExpression, l, c) {}
};

//IfExpression → if Conditions BlockExpression ( else ( BlockExpression | IfExpression ) )?
class IfExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<Conditions> conditions;
  std::unique_ptr<BlockExpressionNode> block_expression;
  std::unique_ptr<BlockExpressionNode> else_block;
  std::unique_ptr<ExpressionNode> else_if;

  IfExpressionNode(std::unique_ptr<Conditions> con, std::unique_ptr<BlockExpressionNode> be, std::unique_ptr<BlockExpressionNode> eb, std::unique_ptr<ExpressionNode> ei, int l, int c)
                : conditions(std::move(con)), block_expression(std::move(be)), else_block(std::move(eb)), else_if(std::move(ei)), ExpressionNode(NodeType::IfExpression, l, c) {}

  bool check() {
    if (conditions == nullptr || block_expression == nullptr) return false;
    if (else_block != nullptr && else_if != nullptr) return false;
    return true;
  }

};

//MatchExpression → match Scrutinee { MatchArms? }
//Scrutinee → Expression except StructExpression
//MatchArms → ( MatchArm => ( ExpressionWithoutBlock , | ExpressionWithBlock ,? ) )* MatchArm => Expression ,?
//MatchArm → Pattern MatchArmGuard?
//MatchArmGuard → if Expression
class MatchArmGuard {
 public:
  std::unique_ptr<ExpressionNode> expression;

  MatchArmGuard(std::unique_ptr<ExpressionNode> expr) : expression(std::move(expr)) {};
};

class MatchArm {
 public:
  std::unique_ptr<Pattern> pattern;
  std::unique_ptr<MatchArmGuard> match_arm_guard;

  MatchArm(std::unique_ptr<Pattern> p, std::unique_ptr<ExpressionNode> expr) 
        : pattern(std::move(p)), match_arm_guard(std::make_unique<MatchArmGuard>(std::move(expr))) {}

  bool check() {
    return pattern != nullptr;
  };
};

class MatchArms {
 public:
  struct match_arms_item {
    std::unique_ptr<MatchArm> match_arm;
    std::unique_ptr<ExpressionNode> expression;
  };
  std::vector<std::unique_ptr<match_arms_item>> match_arms;
  std::unique_ptr<match_arms_item> match_arm;

  MatchArms(std::vector<std::unique_ptr<match_arms_item>> mas, std::unique_ptr<match_arms_item> ma)
          : match_arms(std::move(mas)), match_arm(std::move(ma)) {};
};

class MatchExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> scrutinee;
  std::unique_ptr<MatchArms> match_arms;

  MatchExpressionNode(std::unique_ptr<ExpressionNode> expr, std::unique_ptr<MatchArms> ma, int l, int c)
                  : scrutinee(std::move(expr)), match_arms(std::move(ma)), ExpressionNode(NodeType::MatchExpression, l, c) {};
};

//ReturnExpression → return Expression?
class ReturnExpressionNode : public ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;

  ReturnExpressionNode(std::unique_ptr<ExpressionNode> expr, int l, int c) : expression(std::move(expr)), ExpressionNode(NodeType::ReturnExpression, l, c) {};
};  

//UnderscoreExpression → _
class UnderscoreExpressionNode : public ExpressionNode {
 public:
  UnderscoreExpressionNode(int l, int c) : ExpressionNode(NodeType::UnderscoreExpression, l, c) {};
};

/*
classes for Pratt Parsing (Prefix)
*/
struct PrefixKey {
  TokenType type;
  std::string value;

  bool operator == (const PrefixKey &o) const {
    return type == o.type && value == o.value;
  }
  bool operator < (const PrefixKey& o) const {
    if (type != o.type) return static_cast<int>(type) < static_cast<int>(o.type);;
    return value < o.value;
  }
};

struct InfixKey {
  TokenType type;
  std::string value;

  bool operator == (const InfixKey &o) const {
    return type == o.type && value == o.value;
  }
  bool operator < (const InfixKey& o) const {
    if (type != o.type) return static_cast<int>(type) < static_cast<int>(o.type);;
    return value < o.value;
  }
};

class parser;

class InfixParselet {
 public:
  double precedence;

  InfixParselet(double p) : precedence(p) {}
  virtual ~InfixParselet() = default;

  virtual std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& parser
  ) = 0;
};


class PrefixParselet {
 public:
  virtual std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) = 0;
  virtual ~PrefixParselet() = default;
};

class parser {
 private:
  std::vector<Token> tokens;
  int pos = 0;
  std::map<PrefixKey, std::shared_ptr<PrefixParselet>> prefixParselets;
  std::map<InfixKey, std::shared_ptr<InfixParselet>> infixParselets;

 public:
  parser(std::vector<Token> tokens);

  std::optional<Token> peek() {
    if (pos < tokens.size()) return tokens[pos];
    else return std::nullopt;
  };

  std::optional<Token> get() {
    if (pos < tokens.size()) return tokens[pos++];
    else return std::nullopt;
  };

  void putback(const Token& t) {
    if (pos == 0) throw std::runtime_error("putback called at beginning");
    tokens[--pos] = t;
  }

  int get_pos() { return pos; }

  void roll_back(int pre_pos) {
    pos = pre_pos;
  }
  
  //RangePattern → RangeExclusivePattern | RangeInclusivePattern | RangeFromPattern | RangeToExclusivePattern | RangeToInclusivePattern | ObsoleteRangePattern​1
  //RangeExclusivePattern → RangePatternBound .. RangePatternBound
  //RangeInclusivePattern → RangePatternBound ..= RangePatternBound
  //RangeFromPattern → RangePatternBound ..
  //RangeToExclusivePattern → .. RangePatternBound
  //RangeToInclusivePattern → ..= RangePatternBound
  //ObsoleteRangePattern → RangePatternBound ... RangePatternBound
  //RangePatternBound → LiteralPattern | PathExpression
  std::unique_ptr<RangePatternBound> ParseRangePatternBound();

  std::unique_ptr<RangePattern> ParseRangePattern();

  std::unique_ptr<PatternWithoutRange> parsePatternWithoutRange();

  std::unique_ptr<PatternNoTopAlt> ParsePatternNoTopAlt();

  std::unique_ptr<TypeNode> ParseType();

  //Statement → ; | Item | LetStatement | ExpressionStatement
  //LetStatement → let PatternNoTopAlt ( : Type )? ( = Expression | = Expression except LazyBooleanExpression or end with a } else BlockExpression)? ;

  std::unique_ptr<LetStatement> ParseLetStatement();

  std::unique_ptr<ItemNode> ParseItem();

  std::unique_ptr<ModuleNode> ParseModuleItem();

  std::unique_ptr<TraitNode> ParseTraitItem();

  /*functions used in parsing functions*/
  FunctionQualifier parseFunctionQualifier();

  //FunctionParameters → SelfParam ,? | ( SelfParam , )? FunctionParam ( , FunctionParam )* ,?
  std::unique_ptr<FunctionParameter> ParseFunctionParameters();

  std::unique_ptr<FunctionReturnType> ParseFunctionReturnType();

  std::unique_ptr<FunctionNode> ParseFunctionItem();

  std::unique_ptr<StructStructNode> ParseStructStruct(std::string id);

  std::unique_ptr<TupleStructNode> ParseTupleStruct(std::string id);

  std::variant<std::unique_ptr<TupleStructNode>, std::unique_ptr<StructStructNode>> ParseStructItem();

  std::unique_ptr<TupleFieldNode> ParseTupleFields();

  std::unique_ptr<StructFieldNode> ParseStructFields();

  std::unique_ptr<EnumVariantNode> ParseEnumVariant();

  std::unique_ptr<EnumVariantsNode> ParseEnumVariants();

  std::unique_ptr<EnumerationNode> ParseEnumItem();

  std::unique_ptr<ConstantItemNode> ParseConstItem();

  std::unique_ptr<InherentImplNode> ParseInherentImplItem();

  std::unique_ptr<TraitImplNode> ParseTraitImplItem();

  std::unique_ptr<ExpressionStatement> parseExpressionStatement();

  bool IsItemStart(const Token &tok) {
    return tok.value == "fn" ||
           tok.value == "struct" ||
           tok.value == "enum" ||
           tok.value == "const" ||
           tok.value == "impl" ||
           tok.value == "mod" ||
           tok.value == "type" ||
           tok.value == "trait" ||
           tok.value == "use";
  }


  std::unique_ptr<StatementNode> ParseStatement();

  std::unique_ptr<ParenthesizedTypeNode> ParseParenthesizedType();

  std::unique_ptr<TupleTypeNode> ParseTupleType();

  std::unique_ptr<NeverTypeNode> ParseNeverType();

  std::unique_ptr<ArrayTypeNode> ParseArrayType();

  std::unique_ptr<SliceTypeNode> ParseSliceType();

  std::unique_ptr<ReferenceTypeNode> ParseReferenceType();

  std::unique_ptr<TypePathFn> ParseTypePathFn();

  std::unique_ptr<PathIdentSegment> ParsePathIdentSegment();

  std::unique_ptr<TypePathSegment> ParseTypePathSegment();

  std::unique_ptr<TypePath> ParseTypePath();

  std::unique_ptr<InferredTypeNode> ParseInferredType();

  std::unique_ptr<QualifiedPathInTypeNode> ParseQualifiedPathInType();

  /*
  Parse ExpressionNode
  */
  std::unique_ptr<ExpressionNode> parseExpression(int ctxPrecedence = 0);

  std::unique_ptr<ExpressionWithoutBlockNode> parseExpressionWithoutBlock(int ctxPrecedence = 0);

  std::unique_ptr<BlockExpressionNode> parseBlockExpression();

  std::unique_ptr<Pattern> ParsePattern();

  std::vector<std::unique_ptr<ASTNode>> parse();

  std::unique_ptr<FunctionNode> ParseFunctionItemInImpl(const std::string& impl_type_name);
};

class ContinueExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    p.get();
    return std::make_unique<ContinueExpressionNode>(token.line, token.column);
  }
};

class LiteralParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    switch (token.type) {
      case CHAR_LITERAL:
        return std::make_unique<LiteralExpressionNode>(
          std::make_unique<char_literal>(token.value), token.line, token.column);
      case STRING_LITERAL:
        return std::make_unique<LiteralExpressionNode>(
          std::make_unique<string_literal>(token.value), token.line, token.column);
      case RAW_STRING_LITERAL:
        return std::make_unique<LiteralExpressionNode>(
        std::make_unique<raw_string_literal>(token.value), token.line, token.column);
      case C_STRING_LITERAL:
        return std::make_unique<LiteralExpressionNode>(
          std::make_unique<c_string_literal>(token.value), token.line, token.column);
      case RAW_C_STRING_LITERAL:
        return std::make_unique<LiteralExpressionNode>(
          std::make_unique<raw_c_string_literal>(token.value), token.line, token.column);
      case INTEGER_LITERAL:
        std::cout << "getting int_literalexpression with value: " << token.value << std::endl;
        return std::make_unique<LiteralExpressionNode>(
          std::make_unique<integer_literal>(token.value), token.line, token.column);
      case FLOAT_LITERAL:
        return std::make_unique<LiteralExpressionNode>(
          std::make_unique<float_literal>(token.value), token.line, token.column);
      case STRICT_KEYWORD:
        if (token.value == "true") {
          return std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(true), token.line, token.column);
        } else if (token.value == "false") {
          return std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(false), token.line, token.column);
        } else {
          throw std::runtime_error("Unexpected keyword in literal parselet");
        }
      default:
        throw std::runtime_error("Unexpected token in literal parselet");
    }
  }
};

class BlockExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    int line = token.line;
    int col = token.column;

    std::cout << "begin parsing block expression" << std::endl;
    std::vector<std::unique_ptr<StatementNode>> statements;
    std::unique_ptr<ExpressionWithoutBlockNode> expr = nullptr;

    auto pre_pos = p.get_pos();

    try {
      while (true) {
        auto next = p.peek();
        if (!next.has_value()) {
          throw std::runtime_error("Unexpected end of input inside block");
        }
        if (next->value == "}") break;

        auto pos_before_parsing = p.get_pos();

        bool if1 = true;
        bool if2 = true;

        //try parsing statement
        try {
          std::cout << "ParseStatement in BlockExpression" << std::endl;
          auto stmt = p.ParseStatement();
          if (stmt) {
            statements.push_back(std::move(stmt));
            continue;
          }
        } catch (const std::exception& e) {
          std::cerr << "ParseStatement failed at line "
                    << next->line << ", col " << next->column
                    << ": " << e.what() << std::endl;
          std::cout << "roll back after failing parsing statement" << std::endl;
          p.roll_back(pos_before_parsing);
          if1 = false;
        }

        //try parsing expression wothout block
        try {
          expr = p.parseExpressionWithoutBlock();
          if (!expr) {
            throw std::runtime_error("expression parsing failed");
          } else {
            std::cout << "getting expression without block, block expression parsing finished" << std::endl;
            break;
          }
        } catch (const std::exception& e) {
          std::cerr << "parseExpressionWithoutBlock failed at line "
                    << next->line << ", col " << next->column
                    << ": " << e.what() << std::endl;
          std::cout << "roll back after failing parsing expression" << std::endl;
          p.roll_back(pos_before_parsing);
          if2 = false;
        }

        if (!if1 && !if2) {
          std::cerr << "error in parsing block expression: unknown thing" << std::endl;
          throw std::runtime_error("unable to parse something in blockexpression");
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "[ParseBlockExpressionError] : " << e.what() << std::endl;
      throw std::runtime_error("error in parsing block expression");
    }
    auto close = p.get();
    if (!close.has_value() || close->value != "}") {
      throw std::runtime_error("Expected '}' to close block");
    }
    std::cout << "finish parsing blockexpression" << std::endl;
    std::cout << "next token: " << p.peek()->value << std::endl;
    return std::make_unique<BlockExpressionNode>(
      std::move(statements),
      std::move(expr),
      line, col
    );
  }
};

class BorrowExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing borrow expression" << std::endl;
    int and_count = (token.value == "&&" ? 2 : 1);

    bool isMut = false;
    bool isRaw = false;
    bool isConst = false;

    auto nextTok = p.peek();
    if (nextTok) {
      if (nextTok->value == "mut") {
        p.get();
        isMut = true;
      } 
      else if (nextTok->value == "raw") {
        p.get();
        auto afterRaw = p.get();
        if (!afterRaw) throw std::runtime_error("Expected 'const' or 'mut' after 'raw'");
        if (afterRaw->value == "const") {
          isRaw = true;
          isConst = true;
        } else if (afterRaw->value == "mut") {
          isRaw = true;
          isMut = true;
        } else {
          throw std::runtime_error("Expected 'const' or 'mut' after 'raw'");
        }
      }
    }

    auto expr = p.parseExpression();

    return std::make_unique<BorrowExpressionNode>(
      and_count, isMut, isConst, isRaw,
      std::move(expr),
      token.line, token.column
    );
  }
};

class DereferenceExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing dereference expression" << std::endl;
    auto expr = p.parseExpression(50);
    if (auto* lit = dynamic_cast<LiteralExpressionNode*>(expr.get())) {
      std::cerr << "literal not allowed after dereference" << std::endl;
      throw std::runtime_error("literal after * not allowed");
    }
    return std::make_unique<DereferenceExpressionNode>(
      std::move(expr),
      token.line, token.column
    );
  }
};


class NegationExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    NegationExpressionNode::NegationType negType;

    if (token.value == "-") {
      negType = NegationExpressionNode::MINUS;
    } else if (token.value == "!") {
      negType = NegationExpressionNode::BANG;
    } else {
      throw std::runtime_error("Unexpected token for NegationExpression: " + token.value);
    }

    auto expr = p.parseExpression(25);

    return std::make_unique<NegationExpressionNode>(
      negType, std::move(expr),
      token.line, token.column
    );
  }
};

class GroupedExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    auto expr = p.parseExpression();
    auto next = p.get();
    if (!next || next->type != PUNCTUATION || next->value != ")") throw std::runtime_error("Expected ')' to close grouped expression");
    return std::make_unique<GroupedExpressionNode>(
      std::move(expr),
      token.line,
      token.column
    );
  }
};

class ArrayExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    bool if_empty = true;
    ArrayExpressionType type = ArrayExpressionType::LITERAL;

    auto next = p.peek();
    if (!next || (next->type == PUNCTUATION && next->value == "]")) {
      p.get();
      return std::make_unique<ArrayExpressionNode>(true, type, std::move(elements), token.line, token.column);
    }

    if_empty = false;

    while (true) {
      auto expr = p.parseExpression();
      elements.push_back(std::move(expr));

      auto sep = p.get();
      if (!sep) throw std::runtime_error("Unexpected end of array expression");

      if (sep->type == PUNCTUATION && sep->value == ",") {
        auto lookahead = p.peek();
        if (lookahead && lookahead->type == PUNCTUATION && lookahead->value == "]") { p.get(); break; }
        continue;
      } else if (sep->type == PUNCTUATION && sep->value == ";") {
        type = ArrayExpressionType::REPEAT;
        auto count_expr = p.parseExpression();
        elements.push_back(std::move(count_expr));

        auto close = p.get();
        if (!close || close->type != PUNCTUATION || close->value != "]") {
          throw std::runtime_error("Expected ']' after repeat array");
        }
        break;
      } else if (sep->type == PUNCTUATION && sep->value == "]") break;
      else { throw std::runtime_error("Unexpected token in array expression"); }
    }

    return std::make_unique<ArrayExpressionNode>(if_empty, type, std::move(elements), token.line, token.column);
  }
};

class TupleExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    auto next = p.peek();
    if (next && !(next->type == TokenType::PUNCTUATION && next->value == ")")) {
      while (true) {
        elements.push_back(p.parseExpression());
        auto delimiter = p.get();
        if (!delimiter) throw std::runtime_error("Expected ',' or ')' in tuple expression");
        if (delimiter->type == TokenType::PUNCTUATION && delimiter->value == ")") break;
        if (!(delimiter->type == TokenType::PUNCTUATION && delimiter->value == ",")) throw std::runtime_error("Expected ',' in tuple expression");
        auto after_comma = p.peek();
        if (after_comma && after_comma->type == TokenType::PUNCTUATION && after_comma->value == ")") { p.get(); break; }
      }
    } else {
      auto closing = p.get();
      if (!closing || closing->type != TokenType::PUNCTUATION || closing->value != ")") throw std::runtime_error("Expected ')' for empty tuple");
    }

    return std::make_unique<TupleExpressionNode>(std::move(elements), token.line, token.column);
  }
};

//Tuple & Grouped Expression
class ParenExpressionParselet : public PrefixParselet {
public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing parenExpression" << std::endl;
    std::vector<std::unique_ptr<ExpressionNode>> elements;
    bool has_comma = false;
    auto next = p.peek();
    if (next && !(next->type == TokenType::PUNCTUATION && next->value == ")")) {
      while (true) {
        elements.push_back(p.parseExpression());
        if (elements.size() == 1) {//判断是tuple还是grouped
          auto delimiter = p.peek();
          std::cout << "token after the first expression: " << delimiter->value << std::endl;
          if (!delimiter) throw std::runtime_error("Expected ',' or ')' in paren expression");
          if (delimiter->type == TokenType::PUNCTUATION && delimiter->value == ")") {//如果是(Expression)，直接返回
            p.get();
            std::unique_ptr<ExpressionNode> expr = std::move(elements[0]);
            return std::make_unique<GroupedExpressionNode>(
              std::move(expr),
              token.line,
              token.column
            );
          }
        }
        auto delimiter = p.get();
        if (!delimiter) throw std::runtime_error("Expected ',' or ')' in tuple expression");
        if (delimiter->type == TokenType::PUNCTUATION && delimiter->value == ")") break;
        if (!(delimiter->type == TokenType::PUNCTUATION && delimiter->value == ",")) throw std::runtime_error("Expected ',' in tuple expression");
        auto after_comma = p.peek();
        
        if (after_comma && after_comma->type == TokenType::PUNCTUATION && after_comma->value == ")") { p.get(); break; }
      }
    } else {
      auto closing = p.get();
      if (!closing || closing->type != TokenType::PUNCTUATION || closing->value != ")") throw std::runtime_error("Expected ')' for empty tuple");
    }

    return std::make_unique<TupleExpressionNode>(std::move(elements), token.line, token.column);
  }
};

class BreakExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing break expression" << std::endl;
    std::unique_ptr<ExpressionNode> value = nullptr;
    auto next = p.peek();
    if (next && !(next->type == PUNCTUATION && next->value == ";")) {
      value = p.parseExpression();
    }
 
    return std::make_unique<BreakExpressionNode>(
      std::move(value),
      token.line,
      token.column
    );
  }
};

class PathExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing pathexpression" << std::endl;
    if (token.type == TokenType::PUNCTUATION && token.value == "<") {
      auto typeNode = p.ParseType();
      std::unique_ptr<TypePath> typePath = nullptr;

      if (auto maybeAs = p.peek(); maybeAs && maybeAs->type == TokenType::RESERVED_KEYWORD && maybeAs->value == "as") {
        p.get();
        typePath = p.ParseTypePath();
      }

      auto gt = p.get();
      if (!gt || gt->value != ">") throw std::runtime_error("Expected '>' to close QualifiedPathType");

      std::vector<std::variant<PathInType, Identifier>> segments;
      while (true) {
        auto delim = p.peek();
        if (!delim || delim->value != "::") break;
        p.get();

        auto seg = parse_path_expr_segment(p);
        segments.push_back(seg);
      }

      return std::make_unique<PathExpressionNode>(
        std::make_unique<QualifiedPathInExpression>(
          std::move(typeNode), std::move(typePath), std::move(segments)
        ),
        token.line, token.column
      );
    }

    std::vector<std::variant<PathInType, Identifier>> segments;
    bool leading_double_colon = (token.type == TokenType::PUNCTUATION && token.value == "::");
    if (leading_double_colon) {
      auto seg = parse_path_expr_segment(p);
      segments.push_back(seg);
    } else {
      segments.push_back(parse_path_expr_segment(p, token));
    }

    while (true) {
      auto delim = p.peek();
      if (!delim || delim->value != "::") break;
      p.get();
      auto seg = parse_path_expr_segment(p);
      segments.push_back(seg);
    }

    return std::make_unique<PathExpressionNode>(
      std::make_unique<PathInExpression>(std::move(segments)),
      token.line, token.column
    );
  }

 private:
  std::variant<PathInType, Identifier> parse_path_expr_segment(parser& p, std::optional<Token> firstToken = std::nullopt) {
    Token t;
    if (firstToken) {
      t = *firstToken;
    } else {
      auto tok = p.get();
      if (!tok) throw std::runtime_error("Unexpected EOF in PathExprSegment");
      t = *tok;
    }

    if (t.type == TokenType::IDENTIFIER) return Identifier(t.value);
    if (t.type == TokenType::STRICT_KEYWORD) {
      if (t.value == "super") return PathInType::SUPER;
      if (t.value == "self")  return PathInType::self;
      if (t.value == "Self")  return PathInType::Self;
      if (t.value == "crate") return PathInType::CRATE;
    }
    if (t.type == TokenType::IDENTIFIER) {
      if (t.value == "self") return PathInType::self;
    }
    if (t.type == TokenType::STRICT_KEYWORD && t.value == "$crate") return PathInType::$CRATE;
    throw std::runtime_error("Invalid PathExprSegment: " + t.value);
  }
};

class StructExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing struct expression" << std::endl;
    auto pathInExpr = parse_path_in_expression(p, token);

    auto lbrace = p.get();
    if (!lbrace || lbrace->value != "{") {
      throw std::runtime_error("Expected '{' after PathInExpression in StructExpression");
    }
    if (auto maybeRbrace = p.peek(); maybeRbrace && maybeRbrace->value == "}") {
      p.get();
      return std::make_unique<StructExpressionNode>(std::move(pathInExpr), token.line, token.column);
    }

    auto next = p.peek();
    if (!next) throw std::runtime_error("Unexpected EOF in StructExpression body");

    std::unique_ptr<StructExprFields> structFields = nullptr;
    std::unique_ptr<StructBase> structBase = nullptr;

    if (next->value == "..") {
      p.get();
      auto expr = p.parseExpression();
      structBase = std::make_unique<StructBase>(std::move(expr));
    } else {
      std::vector<std::unique_ptr<StructExprField>> fields;
      structBase = nullptr;

      while (true) {
        auto fieldToken = p.get();
        std::cout << "field token: " << fieldToken->value << std::endl;
        if (!fieldToken || (fieldToken->type != TokenType::IDENTIFIER && fieldToken->type != TokenType::INTEGER_LITERAL)) {
          throw std::runtime_error("Expected identifier or tuple index in StructExprField");
        }

        if (auto colon = p.peek(); colon && colon->value == ":") {
          p.get();
          auto expr = p.parseExpression();

          if (fieldToken->type == TokenType::IDENTIFIER) {
            Identifier id(fieldToken->value);
            fields.push_back(std::make_unique<StructExprField>(id, id, std::move(expr)));
          } else if (fieldToken->type == TokenType::INTEGER_LITERAL) {
            Identifier fake_id(fieldToken->value);
            integer_literal idx(fieldToken->value);
            fields.push_back(std::make_unique<StructExprField>(fake_id, idx, std::move(expr)));
          }
        } else {
          Identifier id(fieldToken->value);
          auto expr = std::make_unique<PathExpressionNode>(
            std::make_unique<PathInExpression>(std::vector<std::variant<PathInType, Identifier>>{id}),
            fieldToken->line, fieldToken->column
          );
          fields.push_back(std::make_unique<StructExprField>(id, id, std::move(expr)));
        }

        auto commaOrRbrace = p.peek();
        if (!commaOrRbrace) throw std::runtime_error("Unexpected EOF after StructExprField");
        if (commaOrRbrace->value == "}") break;

        if (commaOrRbrace->value == ",") {
          p.get();
          auto maybeNext = p.peek();
          if (maybeNext && maybeNext->value == "}") break;
          if (maybeNext && maybeNext->value == "..") {
            p.get();
            auto expr = p.parseExpression();
            structBase = std::make_unique<StructBase>(std::move(expr));
            break;
          }
          continue;
        } else {
          std::cerr << "missing ',' in structexpression" << std::endl;
          throw std::runtime_error("missing ',' in struct expression");
        }
      }

      structFields = std::make_unique<StructExprFields>(std::move(fields), std::move(structBase));
    }
    auto rbrace = p.get();
    if (!rbrace || rbrace->value != "}") throw std::runtime_error("Expected '}' at end of StructExpression");
    if (structFields) return std::make_unique<StructExpressionNode>(std::move(pathInExpr), std::move(structFields), token.line, token.column);
    if (structBase) return std::make_unique<StructExpressionNode>(std::move(pathInExpr), std::move(structBase), token.line, token.column);

    return std::make_unique<StructExpressionNode>(std::move(pathInExpr), token.line, token.column);
  }

 private:
  std::unique_ptr<PathInExpression> parse_path_in_expression(parser& p, const Token& firstToken) {
    std::vector<std::variant<PathInType, Identifier>> segments;
    if (firstToken.type == TokenType::IDENTIFIER) {
      segments.push_back(Identifier(firstToken.value));
    } else {
      throw std::runtime_error("Expected identifier at start of PathInExpression");
    }
    auto delim = p.peek();
    while (delim && delim->value == "::") {
      p.get();
      auto segTok = p.get();
      if (!segTok || segTok->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier after :: in PathInExpression");
      segments.push_back(Identifier(segTok->value));
      delim = p.peek();
    }
    return std::make_unique<PathInExpression>(std::move(segments));
  }
};

class InfiniteLoopExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    auto block = p.parseBlockExpression();
    return std::make_unique<InfiniteLoopExpressionNode>(
      std::move(block),
      token.line,
      token.column
    );
  }
};

class PathOrStructExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parse pathorstructExpressionNode" << std::endl;
    auto pathInExpr = parse_path_in_expression(p, token);

    auto next = p.peek();
    // case 1: PathExpression
    if (!next || next->value != "{") {
      std::cout << "get pathExpression" << std::endl;
      return std::make_unique<PathExpressionNode>(
        std::move(pathInExpr),
        token.line, token.column
      );
    }

    // case 2: StructExpression
    std::cout << "get struct expression with token : " << next->value << std::endl;
    p.get();
    if (auto maybeRbrace = p.peek(); maybeRbrace && maybeRbrace->value == "}") {
      std::cout << "get empty struct expression" << std::endl;
      p.get();
      return std::make_unique<StructExpressionNode>(
        std::move(pathInExpr),
        token.line, token.column
      );
    }

    std::unique_ptr<StructExprFields> structFields = nullptr;
    std::unique_ptr<StructBase> structBase = nullptr;

    auto next2 = p.peek();
    if (!next2) throw std::runtime_error("Unexpected EOF in StructExpression body");

    if (next2->value == "..") {
      p.get();
      auto expr = p.parseExpression();
      structBase = std::make_unique<StructBase>(std::move(expr));
    } else {
      std::cout << "struct field token : " << next2->value << std::endl;
      std::vector<std::unique_ptr<StructExprField>> fields;
      structBase = nullptr;

      while (true) {
        auto fieldToken = p.get();
        if (!fieldToken || (fieldToken->type != TokenType::IDENTIFIER &&
                            fieldToken->type != TokenType::INTEGER_LITERAL)) {
          throw std::runtime_error("Expected identifier or tuple index in StructExprField");
        }

        if (auto colon = p.peek(); colon && colon->value == ":") {
          p.get(); // consume ':'
          auto expr = p.parseExpression();

          if (fieldToken->type == TokenType::IDENTIFIER) {
            Identifier id(fieldToken->value);
            fields.push_back(std::make_unique<StructExprField>(id, id, std::move(expr)));
          } else if (fieldToken->type == TokenType::INTEGER_LITERAL) {
            Identifier fake_id(fieldToken->value);
            integer_literal idx(fieldToken->value);
            fields.push_back(std::make_unique<StructExprField>(fake_id, idx, std::move(expr)));
          }
        } else {
          Identifier id(fieldToken->value);
          auto expr = std::make_unique<PathExpressionNode>(
            std::make_unique<PathInExpression>(
              std::vector<std::variant<PathInType, Identifier>>{id}),
            fieldToken->line, fieldToken->column
          );
          fields.push_back(std::make_unique<StructExprField>(id, id, std::move(expr)));
        }

        auto commaOrRbrace = p.peek();
        if (!commaOrRbrace) throw std::runtime_error("Unexpected EOF after StructExprField");
        if (commaOrRbrace->value == "}") break;

        if (commaOrRbrace->value == ",") {
          p.get();
          auto maybeNext = p.peek();
          if (maybeNext && maybeNext->value == "}") break;
          if (maybeNext && maybeNext->value == "..") {
            p.get();
            auto expr = p.parseExpression();
            structBase = std::make_unique<StructBase>(std::move(expr));
            break;
          }
          continue;
        } else {
          std::cerr << "missing ',' in structexpression" << std::endl;
          throw std::runtime_error("missing ',' in struct expression");
        }
      }

      structFields = std::make_unique<StructExprFields>(std::move(fields), std::move(structBase));
    }

    auto rbrace = p.get();
    if (!rbrace || rbrace->value != "}") throw std::runtime_error("Expected '}' at end of StructExpression");

    if (structFields)
      return std::make_unique<StructExpressionNode>(std::move(pathInExpr), std::move(structFields), token.line, token.column);
    if (structBase)
      return std::make_unique<StructExpressionNode>(std::move(pathInExpr), std::move(structBase), token.line, token.column);

    return std::make_unique<StructExpressionNode>(std::move(pathInExpr), token.line, token.column);
  }

 private:
  std::unique_ptr<PathInExpression> parse_path_in_expression(parser& p, const Token& firstToken) {
    std::vector<std::variant<PathInType, Identifier>> segments;
    if (firstToken.type == TokenType::IDENTIFIER) {
      segments.push_back(Identifier(firstToken.value));
    } else {
      throw std::runtime_error("Expected identifier at start of PathInExpression");
    }
    auto delim = p.peek();
    while (delim && delim->value == "::") {
      p.get(); // consume '::'
      auto segTok = p.get();
      if (!segTok || segTok->type != TokenType::IDENTIFIER)
        throw std::runtime_error("Expected identifier after :: in PathInExpression");
      segments.push_back(Identifier(segTok->value));
      delim = p.peek();
    }
    return std::make_unique<PathInExpression>(std::move(segments));
  }
};


class PredicateLoopExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing PredicateLoopexpression" << std::endl;
    auto conditions = parseConditions(p);

    auto block = p.parseBlockExpression();
    if (!block) {
      throw std::runtime_error("Expected block expression after while-condition at line " + std::to_string(token.line));
    }

    return std::make_unique<PredicateLoopExpressionNode>(
      std::move(conditions),
      std::move(block),
      token.line,
      token.column
    );
  }

 private:
  std::unique_ptr<Conditions> parseConditions(parser& p) {
    auto next = p.peek();
    if (next.has_value() && next->type == TokenType::RESERVED_KEYWORD && next->value == "let") {
      return std::make_unique<Conditions>(parseLetChain(p));
    }

    auto expr = p.parseExpression(0);
    if (!expr) throw std::runtime_error("Expected condition expression after while");

    if (isDisallowedCondition(expr)) {
      throw std::runtime_error("Disallowed expression type in while-condition");
    }

    return std::make_unique<Conditions>(std::move(expr));
  }

  LetChain parseLetChain(parser& p) {
    std::vector<std::unique_ptr<LetChainCondition>> conditions;

    while (true) {
      auto letTok = p.get();
      if (!letTok.has_value() || letTok->value != "let") throw std::runtime_error("Expected 'let' in let-chain");

      auto pattern = p.ParsePattern();
      if (!pattern) throw std::runtime_error("Expected pattern after let");

      auto eqTok = p.get();
      if (!eqTok.has_value() || eqTok->value != "=") throw std::runtime_error("Expected '=' after pattern in let-chain");

      auto expr = p.parseExpression(0);
      if (!expr) throw std::runtime_error("Expected scrutinee expression after '=' in let-chain");
      if (isStructExpression(expr)) {
        throw std::runtime_error("Struct expression not allowed in let-chain scrutinee");
      }

      auto excluded_conditions = ParseExcludedConditions(p);

      conditions.push_back(std::make_unique<LetChainCondition>(
        std::move(expr),
        std::move(pattern),
        std::move(excluded_conditions)
      ));

      auto peekTok = p.peek();
      if (!peekTok.has_value() || !(peekTok->type == TokenType::PUNCTUATION && peekTok->value == "&&"))  break;
      p.get();
    }
    return LetChain(std::move(conditions));
  }

  bool isStructExpression(const std::unique_ptr<ExpressionNode>& expr) {
    return is_type<StructExpressionNode, ExpressionNode>(expr);
  }

  bool isDisallowedCondition(const std::unique_ptr<ExpressionNode>& expr) {
    return is_type<StructExpressionNode, ExpressionNode>(expr)
      || is_type<AssignmentExpressionNode, ExpressionNode>(expr)
      || is_type<CompoundAssignmentExpressionNode, ExpressionNode>(expr)
      || is_type<LazyBooleanExpressionNode, ExpressionNode>(expr)
      || is_type<RangeExpr, ExpressionNode>(expr)
      || is_type<RangeFromExpr, ExpressionNode>(expr)
      || is_type<RangeInclusiveExpr, ExpressionNode>(expr);
  }
 private:
  std::unique_ptr<ExcludedConditions> ParseExcludedConditions(parser& p) {
    auto token = p.peek();
    if (token->type == PUNCTUATION && token->value == "struct") {
      StructExpressionParselet parselet;
      auto tok = p.get();
      auto struct_expr = parselet.parse(p, tok.value());
      return std::make_unique<ExcludedConditions>(std::unique_ptr<StructExpressionNode>(dynamic_cast<StructExpressionNode*>(struct_expr.release())));
    } else {
      auto left = p.parseExpression(0);
      token = p.peek();
      if (!token.has_value()) throw std::runtime_error("Unexpected end of input in ExcludedConditions after expression");

      if (token->type == TokenType::PUNCTUATION &&
        (token->value == "&&" || token->value == "||")) {
        auto op = p.get();
        auto right = p.parseExpression(0);
        LazyBooleanType type = token->value == "&&" ? LazyBooleanType::LAZY_AND : LazyBooleanType::LAZY_OR;
        return std::make_unique<ExcludedConditions>(
          std::make_unique<LazyBooleanExpressionNode>(
            type, std::move(left), std::move(right),
            op->line, op->column
          )
        );
      }

      if (token->type == TokenType::PUNCTUATION &&
        (token->value == "+=" || token->value == "-=" || token->value == "*=" ||
         token->value == "/=" || token->value == "%=" || token->value == "&=" ||
         token->value == "|=" || token->value == "^=" || token->value == "<<=" ||
         token->value == ">>=")) {
        auto op = p.get();
        auto right = p.parseExpression(0);
        OperationType type;
        if (token->value == "+=" ) type = OperationType::ADD;
        if (token->value == "-=" ) type = OperationType::MINUS;
        if (token->value == "*=" ) type = OperationType::MUL;
        if (token->value == "/=" ) type = OperationType::DIV;
        if (token->value == "%=" ) type = OperationType::MOD;
        if (token->value == "&=" ) type = OperationType::AND;
        if (token->value == "|=" ) type = OperationType::OR;
        if (token->value == "^=" ) type = OperationType::XOR;
        if (token->value == "<<=" ) type = OperationType::SHL;
        if (token->value == ">>=" ) type = OperationType::SHR;
        return std::make_unique<ExcludedConditions>(
          std::make_unique<CompoundAssignmentExpressionNode>(
            type, std::move(left), std::move(right),
            op->line, op->column
          )
        );
      }

      if (token->type == TokenType::PUNCTUATION && token->value == "=") {
        auto op = p.get();
        auto right = p.parseExpression(0);
        return std::make_unique<ExcludedConditions>(
          std::make_unique<AssignmentExpressionNode>(
            std::move(left), std::move(right),
            op->line, op->column
          )
        );
      }

      if (token->type == TokenType::PUNCTUATION && token->value == "..=") {
        auto op = p.get();
        auto right = p.parseExpression(0);
        return std::make_unique<ExcludedConditions>(
          std::make_unique<RangeInclusiveExpr>(
            std::move(left), std::move(right)
          )
        );
      }

      if (token->type == TokenType::PUNCTUATION && token->value == "..") {
        auto op = p.get();
        auto next = p.peek();
        if (!next.has_value() || next->type == TokenType::PUNCTUATION)
          return std::make_unique<ExcludedConditions>(
            std::make_unique<RangeFromExpr>(std::move(left))
          );
        auto right = p.parseExpression(0);
        return std::make_unique<ExcludedConditions>(
          std::make_unique<RangeExpr>(
            std::move(left), std::move(right)
          )
        );
      }

      throw std::runtime_error("ParseExcludedConditions: unexpected token '" + token->value + "'");
    }
  };
};

std::unique_ptr<ExpressionNode> parse_if(parser& p, const Token& token) {
  std::cout << "parsing if expression" << std::endl;
  if (p.peek()->value != "(") {
    std::cerr << "expected ( at the beginning of conditions in if expression" << std::endl;
    throw std::runtime_error("expected ( at the beginning of conditions in if expression");
  }
  auto cond = std::make_unique<Conditions>(p.parseExpression(0));
  auto thenBlock = p.parseBlockExpression();
  std::unique_ptr<BlockExpressionNode> elseBlock = nullptr;
  std::unique_ptr<ExpressionNode> elseIf = nullptr;

  auto next = p.peek();
  if (next && next->type == TokenType::STRICT_KEYWORD && next->value == "else") {
    std::cout << "parsing else in ifexpression" << std::endl;
    p.get();
    auto afterElse = p.peek();
    if (afterElse && afterElse->type == TokenType::STRICT_KEYWORD && afterElse->value == "if") {
      std::cout << "having else if" << std::endl;
      auto elseIfExpr = p.get();
      elseIf = std::move(parse_if(p, elseIfExpr.value()));
    } else {
      elseBlock = p.parseBlockExpression();
    }
  }
  std::cout << "finish parsing if expression" << std::endl;
  return std::make_unique<IfExpressionNode>(
    std::move(cond),
    std::move(thenBlock),
    std::move(elseBlock),
    std::move(elseIf),
    token.line,
    token.column
  );
}

class IfExpressionParselet : public PrefixParselet {
 public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing if expression" << std::endl;
    if (p.peek()->value != "(") {
      std::cerr << "expected ( at the beginning of conditions in if expression" << std::endl;
      throw std::runtime_error("expected ( at the beginning of conditions in if expression");
    }
    auto cond = std::make_unique<Conditions>(p.parseExpression(0));
    auto thenBlock = p.parseBlockExpression();

    std::unique_ptr<BlockExpressionNode> elseBlock = nullptr;
    std::unique_ptr<ExpressionNode> elseIf = nullptr;

    auto next = p.peek();
    if (next && next->type == TokenType::STRICT_KEYWORD && next->value == "else") {
      std::cout << "parsing else in ifexpression" << std::endl;
      p.get();
      auto afterElse = p.peek();
      if (afterElse && afterElse->type == TokenType::STRICT_KEYWORD && afterElse->value == "if") {
        std::cout << "having else if" << std::endl;
        auto elseIfExpr = p.get();
        elseIf = std::move(parse_if(p, elseIfExpr.value()));
      } else {
        elseBlock = p.parseBlockExpression();
      }
    }

    std::cout << "finish parsing if expression" << std::endl;

    return std::make_unique<IfExpressionNode>(
      std::move(cond),
      std::move(thenBlock),
      std::move(elseBlock),
      std::move(elseIf),
      token.line,
      token.column
    );
  }
};

class MatchExpressionParselet : public PrefixParselet {
public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    auto scrutinee = p.parseExpression(0);

    auto next = p.get();
    if (!next || next->type != TokenType::DELIMITER || next->value != "{") {
      throw std::runtime_error("Expected '{' after match scrutinee");
    }

    std::vector<std::unique_ptr<MatchArms::match_arms_item>> arms;

    while (true) {
      auto t = p.peek();
      if (!t) throw std::runtime_error("Unexpected end of input in match expression");
      if (t->type == TokenType::DELIMITER && t->value == "}") break;

      auto pattern = p.ParsePattern();

      std::unique_ptr<MatchArmGuard> guard = nullptr;
      auto maybeIf = p.peek();
      if (maybeIf && maybeIf->type == TokenType::STRICT_KEYWORD && maybeIf->value == "if") {
        p.get();
        auto guardExpr = p.parseExpression(0);
        guard = std::make_unique<MatchArmGuard>(std::move(guardExpr));
      }

      auto matchArm = std::make_unique<MatchArm>(std::move(pattern), guard ? std::move(guard->expression) : nullptr);

      auto arrow = p.get();
      if (!arrow || arrow->type != TokenType::PUNCTUATION || arrow->value != "=>") throw std::runtime_error("Expected '=>' in match arm");
      auto expr = p.parseExpression(0);

      auto maybeComma = p.peek();
      if (maybeComma && maybeComma->type == TokenType::PUNCTUATION && maybeComma->value == ",") {
        p.get();
      }

      auto item = std::make_unique<MatchArms::match_arms_item>();
      item->match_arm = std::move(matchArm);
      item->expression = std::move(expr);
      arms.push_back(std::move(item));
    }

    auto close = p.get();
    if (!close || close->type != TokenType::DELIMITER || close->value != "}") {
      throw std::runtime_error("Expected '}' at end of match expression");
    }

    auto matchArms = std::make_unique<MatchArms>(std::move(arms), nullptr);

    return std::make_unique<MatchExpressionNode>(
      std::move(scrutinee),
      std::move(matchArms),
      token.line,
      token.column
    );
  }
};

class ReturnExpressionParselet : public PrefixParselet {
public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    std::cout << "parsing return expression" << std::endl;
    auto next = p.peek();

    std::unique_ptr<ExpressionNode> expr = nullptr;
    if (next && !(next->value == ";" || next->value == "}")) {
      expr = p.parseExpression(0);
    }
    if (next && next->value == ";") {
      std::cout << "finish parsing return expression node" << std::endl;
      return std::make_unique<ReturnExpressionNode>(
        nullptr,
        token.line,
        token.column
      );
    }

    std::cout << "finish parsing return expression node" << std::endl;
    return std::make_unique<ReturnExpressionNode>(
      std::move(expr),
      token.line,
      token.column
    );
  }
};

class UnderscoreExpressionParselet : public PrefixParselet {
public:
  std::unique_ptr<ExpressionNode> parse(parser& p, const Token& token) override {
    return std::make_unique<UnderscoreExpressionNode>(
      token.line,
      token.column
    );
  }
};

/*
classes for Pratt Parsing (Infix)
*/
class ArithmeticOrLogicalExpressionNodeParselet : public InfixParselet {
 public:
  OperationType type;
  bool rightAssociative;

  ArithmeticOrLogicalExpressionNodeParselet(double prec, OperationType t, bool rightAssoc = false)
    : InfixParselet(prec), type(t), rightAssociative(rightAssoc) {}

  std::unique_ptr<ExpressionNode> parse(std::unique_ptr<ExpressionNode> left, const Token& token, parser& p) override {
    std::cout << "parsing arithmeticorlogicalexpressionnode" << std::endl;
    double nextPrec = precedence - (rightAssociative ? 1 : 0);
    auto right = p.parseExpression(nextPrec);
    return std::make_unique<ArithmeticOrLogicalExpressionNode>(type, std::move(left), std::move(right), token.line, token.column);
  }
};

class ComparisonExpressionNodeParselet : public InfixParselet {
 public:
  ComparisonExpressionNodeParselet(double precedence)
    : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    auto right = p.parseExpression(precedence);

    std::cout << "parsing comparison expression node" << std::endl;

    ComparisonType type;
    if (token.type == PUNCTUATION) {
      if (token.value == "==")       type = ComparisonType::EQ;
      else if (token.value == "!=")  type = ComparisonType::NEQ;
      else if (token.value == ">")   type = ComparisonType::GT;
      else if (token.value == "<")   type = ComparisonType::LT;
      else if (token.value == ">=")  type = ComparisonType::GEQ;
      else if (token.value == "<=")  type = ComparisonType::LEQ;
      else {
        throw std::runtime_error("Unexpected punctuation in ComparisonExpressionNodeParselet: " + token.value);
      }
    } else {
      throw std::runtime_error("Unexpected token type in ComparisonExpressionNodeParselet");
    }

    return std::make_unique<ComparisonExpressionNode>(
      type, std::move(left), std::move(right),
      token.line, token.column
    );
  }
};

class LazyBooleanExpressionParselet : public InfixParselet {
 public:
  LazyBooleanExpressionParselet(double precedence)
    : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    std::cout << "parsing lazy boolean expression" << std::endl;
    auto right = p.parseExpression(precedence);

    LazyBooleanType type;
    if (token.type == PUNCTUATION) {
      if (token.value == "&&")      type = LazyBooleanType::LAZY_AND;
      else if (token.value == "||") type = LazyBooleanType::LAZY_OR;
      else {
        throw std::runtime_error("Unexpected punctuation in LazyBooleanExpressionParselet: " + token.value);
      }
    } else {
      throw std::runtime_error("Unexpected token type in LazyBooleanExpressionParselet");
    }

    return std::make_unique<LazyBooleanExpressionNode>(
      type,
      std::move(left),
      std::move(right),
      token.line,
      token.column
    );
  }
};

class TypeCastExpressionParselet : public InfixParselet {
 public:
  TypeCastExpressionParselet(double precedence)
    : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    std::cout << "parsing type cast expression node" << std::endl;
    if (token.type != STRICT_KEYWORD || token.value != "as") throw std::runtime_error("Expected 'as' in TypeCastExpressionParselet");
    auto type = p.ParseType();
    return std::make_unique<TypeCastExpressionNode>(
      std::move(left),
      std::move(type),
      token.line,
      token.column
    );
  }
};

class AssignmentExpressionParselet : public InfixParselet {
 public:
  AssignmentExpressionParselet(double precedence)
    : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    if (token.type != PUNCTUATION || token.value != "=") throw std::runtime_error("Expected '=' in AssignmentExpressionParselet");

    auto right = p.parseExpression(precedence - 1);

    return std::make_unique<AssignmentExpressionNode>(
      std::move(left), std::move(right),
      token.line, token.column
    );
  }
};

class CompoundAssignmentExpressionParselet : public InfixParselet {
 public:
  CompoundAssignmentExpressionParselet(double precedence)
    : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    OperationType op;

    if (token.type != PUNCTUATION) throw std::runtime_error("Expected compound assignment operator");

    if (token.value == "+=") op = OperationType::ADD;
    else if (token.value == "-=") op = OperationType::MINUS;
    else if (token.value == "*=") op = OperationType::MUL;
    else if (token.value == "/=") op = OperationType::DIV;
    else if (token.value == "%=") op = OperationType::MOD;
    else if (token.value == "&=") op = OperationType::AND;
    else if (token.value == "|=") op = OperationType::OR;
    else if (token.value == "^=") op = OperationType::XOR;
    else if (token.value == "<<=") op = OperationType::SHL;
    else if (token.value == ">>=") op = OperationType::SHR;
    else throw std::runtime_error("Unknown compound assignment operator: " + token.value);

    auto right = p.parseExpression(precedence - 1);

    return std::make_unique<CompoundAssignmentExpressionNode>(
      op, std::move(left), std::move(right),
      token.line, token.column
    );
  }
};

class IndexExpressionParselet : public InfixParselet {
 public:
  IndexExpressionParselet(double precedence) : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    std::cout << "parsing indexexpression" << std::endl;
    auto base = std::move(left);
    auto index = p.parseExpression();

    auto closing = p.get();
    if (!closing || closing->type != TokenType::PUNCTUATION || closing->value != "]") throw std::runtime_error("Expected closing ']' in index expression");

    return std::make_unique<IndexExpressionNode>(
      std::move(base), std::move(index),
      token.line, token.column
    );
  }
};

class TupleIndexingExpressionParselet : public InfixParselet {
 public:
  TupleIndexingExpressionParselet(double p) : InfixParselet(p) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    auto indexToken = p.get();
    if (!indexToken || indexToken->type != TokenType::INTEGER_LITERAL) throw std::runtime_error("Expected integer literal after '.' for tuple indexing");
    integer_literal idx(indexToken->value);
    return std::make_unique<TupleIndexingExpressionNode>(
      std::move(left),
      idx,
      token.line,
      token.column
    );
  }
};


class CallExpressionParselet : public InfixParselet {
 public:
  CallExpressionParselet(double precedence) : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    std::vector<std::unique_ptr<ExpressionNode>> args;

    auto t = p.peek();
    if (!t.has_value()) throw std::runtime_error("Unexpected end of input in call expression");

    if (t->value != ")") {
      while (true) {
        args.push_back(p.parseExpression(0));

        auto next = p.peek();
        if (!next.has_value()) throw std::runtime_error("Unexpected end of input in argument list");

        if (next->value == ",") {
          p.get();
          next = p.peek();
          if (next->value == ")") break;
          continue;
        }
        if (next->value == ")") {
          break;
        }
      }
    }

    auto closing = p.get();
    if (!closing.has_value() || closing->type != TokenType::PUNCTUATION || closing->value != ")") {
      throw std::runtime_error("Expected ')' after arguments");
    }

    std::unique_ptr<CallParams> callParams = nullptr;
    if (!args.empty()) {
      callParams = std::make_unique<CallParams>(std::move(args));
    }
    std::cout << "get call expression" << std::endl;
    return std::make_unique<CallExpressionNode>(
      std::move(left), 
      std::move(callParams), 
      token.line, token.column
    );
  }
};

class MethodCallExpressionParselet : public InfixParselet {
public:
  MethodCallExpressionParselet(double precedence) : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    std::vector<std::unique_ptr<ExpressionNode>> args;
    std::cout << "parsing method call expression" << std::endl;

    auto next = p.get();
    if (!next.has_value() || next->type != IDENTIFIER) throw std::runtime_error("Expected identifier after '.' in method call");
    Identifier method_name(next->value);

    auto t = p.peek();
    if (!(t->type == TokenType::PUNCTUATION && t->value == ")")) {
      while (true) {
        args.push_back(p.parseExpression(0));

        auto next = p.peek();
        if (!next.has_value()) throw std::runtime_error("Unexpected end of input in argument list");

        if (next->type == TokenType::PUNCTUATION && next->value == ",") {
          p.get();
          continue;
        } else break;
      }
    }

    std::unique_ptr<CallParams> callParams = nullptr;
    if (!args.empty()) {
      callParams = std::make_unique<CallParams>(std::move(args));
    }

    return std::make_unique<MethodCallExpressionNode>(
      std::move(left),
      method_name,
      std::move(callParams),
      token.line,
      token.column
    );
  }
};

class FieldExpressionParselet : public InfixParselet {
public:
  FieldExpressionParselet(double precedence) : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    auto next = p.get();
    if (!next.has_value() || next->type != IDENTIFIER) {
      throw std::runtime_error("Expected identifier after '.' in field access");
    }
    Identifier field_name(next->value);
    return std::make_unique<FieldExpressionNode>(
      std::move(left),
      field_name,
      token.line,
      token.column
    );
  }
};

class DotExpressionParselet : public InfixParselet {
public:
  DotExpressionParselet(double precedence) : InfixParselet(precedence) {}

  std::unique_ptr<ExpressionNode> parse(
    std::unique_ptr<ExpressionNode> left,
    const Token& token,
    parser& p
  ) override {
    std::cout << "parsing dotexpression" << std::endl;
    auto next = p.get();
    if (!next.has_value()) {
      throw std::runtime_error("Unexpected end of input after '.'");
    }

    // tuple indexing
    if (next->type == TokenType::INTEGER_LITERAL) {
      integer_literal idx(next->value);
      return std::make_unique<TupleIndexingExpressionNode>(
        std::move(left), idx, token.line, token.column
      );
    }

    // method call or field
    if (next->type == TokenType::IDENTIFIER) {
      Identifier name(next->value);

      // check method call
      auto peekTok = p.peek();
      if (peekTok && peekTok->type == TokenType::PUNCTUATION && peekTok->value == "(") {
        std::cout << "parsing method call expression" << std::endl;
        p.get();
        std::vector<std::unique_ptr<ExpressionNode>> args;

        auto t = p.peek();
        if (!t.has_value()) throw std::runtime_error("Unexpected end of input in method call");

        if (!(t->type == TokenType::PUNCTUATION && t->value == ")")) {
          while (true) {
            args.push_back(p.parseExpression(0));

            auto nextArg = p.peek();
            if (!nextArg.has_value()) throw std::runtime_error("Unexpected end of input in argument list");

            if (nextArg->type == TokenType::PUNCTUATION && nextArg->value == ",") {
              p.get();
              if (p.peek()->value == ")") break;
              continue;
            } else break;
          }
        }

        auto closing = p.get();
        std::cout << "token of closing in parsing method call expression: " << closing->value << std::endl;
        if (!closing.has_value() || closing->type != TokenType::PUNCTUATION || closing->value != ")") {
          throw std::runtime_error("Expected ')' after arguments in method call");
        }

        std::unique_ptr<CallParams> callParams = nullptr;
        if (!args.empty()) {
          callParams = std::make_unique<CallParams>(std::move(args));
        }

        return std::make_unique<MethodCallExpressionNode>(
          std::move(left), name, std::move(callParams), token.line, token.column
        );
      }

      return std::make_unique<FieldExpressionNode>(
        std::move(left), name, token.line, token.column
      );
    }

    throw std::runtime_error("Unexpected token after '.'");
  }
};


template<typename Base, typename Derived>
std::unique_ptr<Base> upcast(std::unique_ptr<Derived> derived) {
  return std::unique_ptr<Base>(static_cast<Base*>(derived.release()));
}

/*
Parser
*/

  parser::parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {
    prefixParselets[{CHAR_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{STRING_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{RAW_STRING_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{BYTE_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{BYTE_STRING_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{RAW_BYTE_STRING_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{C_STRING_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{RAW_C_STRING_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{INTEGER_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{FLOAT_LITERAL, ""}] = std::make_shared<LiteralParselet>();
    prefixParselets[{PUNCTUATION, "-"}] = std::make_shared<NegationExpressionParselet>();
    prefixParselets[{PUNCTUATION, "!"}] = std::make_shared<NegationExpressionParselet>();
    prefixParselets[{PUNCTUATION, "("}] = std::make_shared<ParenExpressionParselet>();
    prefixParselets[{PUNCTUATION, "["}] = std::make_shared<ArrayExpressionParselet>();
    prefixParselets[{PUNCTUATION, "::"}] = std::make_shared<PathExpressionParselet>();
    prefixParselets[{PUNCTUATION, "{"}] = std::make_shared<BlockExpressionParselet>();
    prefixParselets[{IDENTIFIER, ""}] = std::make_shared<PathOrStructExpressionParselet>();
    prefixParselets[{PUNCTUATION, "<"}] = std::make_shared<PathExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "Self"}] = std::make_shared<PathExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "loop"}] = std::make_shared<InfiniteLoopExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "while"}] = std::make_shared<PredicateLoopExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "if"}] = std::make_shared<IfExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "match"}] = std::make_shared<MatchExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "return"}] = std::make_shared<ReturnExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "break"}] = std::make_shared<BreakExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "continue"}] = std::make_shared<ContinueExpressionParselet>();
    prefixParselets[{PUNCTUATION, "_"}] = std::make_shared<UnderscoreExpressionParselet>();
    prefixParselets[{STRICT_KEYWORD, "true"}] = std::make_shared<LiteralParselet>();
    prefixParselets[{STRICT_KEYWORD, "false"}] = std::make_shared<LiteralParselet>();
    prefixParselets[{PUNCTUATION, "&"}] = std::make_shared<BorrowExpressionParselet>();
    prefixParselets[{PUNCTUATION, "&&"}] = std::make_shared<BorrowExpressionParselet>();
    prefixParselets[{PUNCTUATION, "*"}] = std::make_shared<DereferenceExpressionParselet>();
    infixParselets[{PUNCTUATION, "("}]  = std::make_shared<CallExpressionParselet>(50.0);
    infixParselets[{PUNCTUATION, "."}]  = std::make_shared<DotExpressionParselet>(40.0);
    infixParselets[{PUNCTUATION, "["}]  = std::make_shared<IndexExpressionParselet>(30.0);
    infixParselets[{STRICT_KEYWORD, "as"}] = std::make_shared<TypeCastExpressionParselet>(39.0);
    infixParselets[{PUNCTUATION, "*"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(25.0, OperationType::MUL);
    infixParselets[{PUNCTUATION, "/"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(25.0, OperationType::DIV);
    infixParselets[{PUNCTUATION, "%"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(25.0, OperationType::MOD);
    infixParselets[{PUNCTUATION, "+"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(24.0, OperationType::ADD);
    infixParselets[{PUNCTUATION, "-"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(24.0, OperationType::MINUS);
    infixParselets[{PUNCTUATION, "<<"}] = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(23.0, OperationType::SHL);
    infixParselets[{PUNCTUATION, ">>"}] = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(23.0, OperationType::SHR);
    infixParselets[{PUNCTUATION, "<"}]  = std::make_shared<ComparisonExpressionNodeParselet>(22.0);
    infixParselets[{PUNCTUATION, "<="}] = std::make_shared<ComparisonExpressionNodeParselet>(22.0);
    infixParselets[{PUNCTUATION, ">"}]  = std::make_shared<ComparisonExpressionNodeParselet>(22.0);
    infixParselets[{PUNCTUATION, ">="}] = std::make_shared<ComparisonExpressionNodeParselet>(22.0);
    infixParselets[{PUNCTUATION, "&"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(22.0, OperationType::AND);
    infixParselets[{PUNCTUATION, "^"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(21.5, OperationType::XOR);
    infixParselets[{PUNCTUATION, "|"}]  = std::make_shared<ArithmeticOrLogicalExpressionNodeParselet>(21.0, OperationType::OR);
    infixParselets[{PUNCTUATION, "=="}] = std::make_shared<ComparisonExpressionNodeParselet>(20.0);
    infixParselets[{PUNCTUATION, "!="}] = std::make_shared<ComparisonExpressionNodeParselet>(20.0);
    infixParselets[{PUNCTUATION, "&&"}] = std::make_shared<LazyBooleanExpressionParselet>(17.0);
    infixParselets[{PUNCTUATION, "||"}] = std::make_shared<LazyBooleanExpressionParselet>(16.0);
    infixParselets[{PUNCTUATION, "="}]  = std::make_shared<AssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "+="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "-="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "*="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "/="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "%="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "&="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "|="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "^="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "<<="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, ">>="}] = std::make_shared<CompoundAssignmentExpressionParselet>(10.0);
    infixParselets[{PUNCTUATION, "["}] = std::make_shared<IndexExpressionParselet>(30.0);
    infixParselets[{PUNCTUATION, "."}] = std::make_shared<DotExpressionParselet>(40.0);
    infixParselets[{PUNCTUATION, "("}] = std::make_shared<CallExpressionParselet>(50.0);
  };

  //RangePattern → RangeExclusivePattern | RangeInclusivePattern | RangeFromPattern | RangeToExclusivePattern | RangeToInclusivePattern | ObsoleteRangePattern​1
  //RangeExclusivePattern → RangePatternBound .. RangePatternBound
  //RangeInclusivePattern → RangePatternBound ..= RangePatternBound
  //RangeFromPattern → RangePatternBound ..
  //RangeToExclusivePattern → .. RangePatternBound
  //RangeToInclusivePattern → ..= RangePatternBound
  //ObsoleteRangePattern → RangePatternBound ... RangePatternBound
  //RangePatternBound → LiteralPattern | PathExpression
  std::unique_ptr<RangePatternBound> parser::ParseRangePatternBound() {
    auto tok = peek();
    if (tok->type == PUNCTUATION && tok->value == "-") {
      get();
      tok = peek();
      if (tok->type == CHAR_LITERAL || tok->type == STRING_LITERAL || tok->type == RAW_STRING_LITERAL 
        || tok->type == C_STRING_LITERAL || tok->type == RAW_C_STRING_LITERAL || tok->type == INTEGER_LITERAL || tok->type == FLOAT_LITERAL 
        || (tok->type == STRICT_KEYWORD && (tok->value == "true" || tok->value == "false"))) {
        get();
        switch(tok->type) {
          case(CHAR_LITERAL) : {
            return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<char_literal>(tok->value), tok->line, tok->column)));
          }
          case(STRING_LITERAL) : {
            return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<string_literal>(tok->value), tok->line, tok->column)));
          }
          case(RAW_STRING_LITERAL) : {
            return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<raw_string_literal>(tok->value), tok->line, tok->column)));
          }
          case(C_STRING_LITERAL) : {
            return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<c_string_literal>(tok->value), tok->line, tok->column)));
          }
          case(RAW_C_STRING_LITERAL) : {
            return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<raw_c_string_literal>(tok->value), tok->line, tok->column)));
          }
          case(INTEGER_LITERAL) : {
            return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<integer_literal>(tok->value), tok->line, tok->column)));
          }
          case(FLOAT_LITERAL) : {
            return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<float_literal>(tok->value), tok->line, tok->column)));
          }
          case(STRICT_KEYWORD) : {
            if (tok->value == "true") return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<bool>(true), tok->line, tok->column)));
            else if (tok->value == "false") return std::make_unique<RangePatternBound>(
              std::make_unique<LiteralPattern>(true,
              std::make_unique<LiteralExpressionNode>(
              std::make_unique<bool>(false), tok->line, tok->column)));
            else throw std::runtime_error("Expected true or false in Literal ExpressionNode");
          }
        }
      }
    } else if (tok->type == CHAR_LITERAL || tok->type == STRING_LITERAL || tok->type == RAW_STRING_LITERAL 
            || tok->type == C_STRING_LITERAL || tok->type == RAW_C_STRING_LITERAL || tok->type == INTEGER_LITERAL || tok->type == FLOAT_LITERAL 
            || (tok->type == STRICT_KEYWORD && (tok->value == "true" || tok->value == "false"))) {
      switch(tok->type) {
        case(CHAR_LITERAL) : {
          return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<char_literal>(tok->value), tok->line, tok->column)));
        }
        case(STRING_LITERAL) : {
          return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<string_literal>(tok->value), tok->line, tok->column)));
        }
        case(RAW_STRING_LITERAL) : {
          return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<raw_string_literal>(tok->value), tok->line, tok->column)));
        }
        case(C_STRING_LITERAL) : {
          return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<c_string_literal>(tok->value), tok->line, tok->column)));
        }
        case(RAW_C_STRING_LITERAL) : {
          return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<raw_c_string_literal>(tok->value), tok->line, tok->column)));
        }
        case(INTEGER_LITERAL) : {
          return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<integer_literal>(tok->value), tok->line, tok->column)));
        }
        case(FLOAT_LITERAL) : {
          return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<float_literal>(tok->value), tok->line, tok->column)));
        }
        case(STRICT_KEYWORD) : {
          if (tok->value == "true") return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(true), tok->line, tok->column)));
          else if (tok->value == "false") return std::make_unique<RangePatternBound>(
            std::make_unique<LiteralPattern>(true,
            std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(false), tok->line, tok->column)));
          else throw std::runtime_error("Expected true or false in Literal ExpressionNode");
        }
      }
    } else {
      PathExpressionParselet parselet;
      auto node = parselet.parse(*this, *tok);

      auto pathNode = dynamic_cast<PathExpressionNode*>(node.release());
      if (!pathNode) {
        throw std::runtime_error("Internal error: Expected PathExpressionNode");
      }
      return std::make_unique<RangePatternBound>(std::unique_ptr<PathExpressionNode>(pathNode));
    }
    throw std::runtime_error("Parse RangePatternBound Error");
  };

  std::unique_ptr<RangePattern> parser::ParseRangePattern() {
    auto firstTok = peek();
    if (!firstTok) throw std::runtime_error("Unexpected EOF in range pattern");

    if (firstTok->type == PUNCTUATION && firstTok->value == "..") {
      get();

      auto next = peek();
      if (!next || next->type == DELIMITER || next->value == "," || next->value == "}") {
        auto pat = std::make_unique<RangeToExclusivePattern>();
        return std::make_unique<RangePattern>(std::move(pat));
      }

      auto bound = ParseRangePatternBound();
      auto pat = std::make_unique<RangeToExclusivePattern>(std::move(bound));
      return std::make_unique<RangePattern>(std::move(pat));
    }

    if (firstTok->type == PUNCTUATION && firstTok->value == "..=") {
      get(); // consume `..=`
      auto bound = ParseRangePatternBound();
      auto pat = std::make_unique<RangeToInclusivePattern>(std::move(bound));
      return std::make_unique<RangePattern>(std::move(pat));
    }

    auto lower = ParseRangePatternBound();
    auto opTok = peek();
    if (!opTok || opTok->type != PUNCTUATION) {
      throw std::runtime_error("Expected '..', '..=', or '...' after RangePatternBound");
    }

    if (opTok->value == "..") {
      get();
      auto maybeUpper = peek();
      if (!maybeUpper || maybeUpper->value == "," || maybeUpper->value == "}") {
        auto pat = std::make_unique<RangeFromPattern>(std::move(lower));
        return std::make_unique<RangePattern>(std::move(pat));
      } else {
        auto upper = ParseRangePatternBound();
        auto pat = std::make_unique<RangeExclusivePattern>(std::move(lower), std::move(upper));
        return std::make_unique<RangePattern>(std::move(pat));
      }
    }

    if (opTok->value == "..=") {
      get();
      auto upper = ParseRangePatternBound();
      auto pat = std::make_unique<RangeInclusivePattern>(std::move(lower), std::move(upper));
      return std::make_unique<RangePattern>(std::move(pat));
    }

    if (opTok->value == "...") {
      get();
      auto upper = ParseRangePatternBound();
      auto pat = std::make_unique<ObsoleteRangePattern>(std::move(lower), std::move(upper));
      return std::make_unique<RangePattern>(std::move(pat));
    }

    throw std::runtime_error("Invalid range pattern operator: " + opTok->value);
  }

  std::unique_ptr<PatternWithoutRange> parser::parsePatternWithoutRange() {
    auto t = peek();
    if (!t) throw std::runtime_error("unexpected EOF in PatternWithoutRange");
    std::cout << "the first token in parsing pattern without range : " << t.value().value << std::endl;
    // LiteralPattern → -? LiteralExpression
    if (t->type == TokenType::PUNCTUATION && t->value == "-") {
      get();
      auto litTok = get();
      if (!litTok || !(litTok->type == TokenType::CHAR_LITERAL || litTok->type == TokenType::STRING_LITERAL ||
                       litTok->type == TokenType::RAW_STRING_LITERAL || litTok->type == TokenType::C_STRING_LITERAL ||
                       litTok->type == TokenType::RAW_C_STRING_LITERAL || litTok->type == TokenType::INTEGER_LITERAL ||
                       litTok->type == TokenType::FLOAT_LITERAL)) {
        throw std::runtime_error("expected literal after '-'");
      }
      std::unique_ptr<LiteralExpressionNode> lit;
      switch(litTok->type) {
        case(CHAR_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<char_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(RAW_STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<raw_string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(C_STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<c_string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(RAW_C_STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<raw_c_string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(INTEGER_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<integer_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(FLOAT_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<float_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(STRICT_KEYWORD) : {
          if (litTok->value == "true") lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(true), litTok->line, litTok->column);
          else if (litTok->value == "false") lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(false), litTok->line, litTok->column);
          else throw std::runtime_error("Expected true or false in Literal ExpressionNode");
        }
      }
      return std::make_unique<PatternWithoutRange>(
        std::make_unique<LiteralPattern>(false, std::move(lit))
      );
    }

    // LiteralPattern without minus
    if (t->type == TokenType::CHAR_LITERAL || t->type == TokenType::STRING_LITERAL ||
        t->type == TokenType::RAW_STRING_LITERAL || t->type == TokenType::C_STRING_LITERAL ||
        t->type == TokenType::RAW_C_STRING_LITERAL || t->type == TokenType::INTEGER_LITERAL ||
        t->type == TokenType::FLOAT_LITERAL) {
      auto litTok = get();
      std::unique_ptr<LiteralExpressionNode> lit;
      switch(litTok->type) {
        case(CHAR_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<char_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(RAW_STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<raw_string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(C_STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<c_string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(RAW_C_STRING_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<raw_c_string_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(INTEGER_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<integer_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(FLOAT_LITERAL) : {
          lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<float_literal>(litTok->value), litTok->line, litTok->column);
        }
        case(STRICT_KEYWORD) : {
          if (litTok->value == "true") lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(true), litTok->line, litTok->column);
          else if (litTok->value == "false") lit = std::make_unique<LiteralExpressionNode>(
            std::make_unique<bool>(false), litTok->line, litTok->column);
          else throw std::runtime_error("Expected true or false in Literal ExpressionNode");
        }
      }
      return std::make_unique<PatternWithoutRange>(
        std::make_unique<LiteralPattern>(false, std::move(lit))
      );
    }

    // WildcardPattern → _
    if (t->type == TokenType::PUNCTUATION && t->value == "_") {
      get();
      return std::make_unique<PatternWithoutRange>(std::make_unique<WildCardPattern>());
    }

    // RestPattern → ..
    if (t->type == TokenType::PUNCTUATION && t->value == "..") {
      get();
      return std::make_unique<PatternWithoutRange>(std::make_unique<RestPattern>());
    }

    // ReferencePattern → (& | &&) mut? PatternWithoutRange
    if (t->type == TokenType::PUNCTUATION && (t->value == "&" || t->value == "&&")) {
      std::cout << "parsing reference type in parsePatternwithoutRange" << std::endl;
      get();
      int and_count = 1;
      and_count = 1 ? (t->value == "&") : 2;
      bool if_mut = false;
      auto maybe_mut = peek();
      if (maybe_mut && maybe_mut->value == "mut") {
        get();
        std::cout << "getting mut in reference pattern" << std::endl;
        if_mut = true;
      }
      auto pwr = parsePatternWithoutRange();
      return std::make_unique<PatternWithoutRange>(std::make_unique<ReferencePattern>(and_count, if_mut, std::move(pwr)));
    }

    //mut+idpattern
    if (t->value == "mut") {
      std::cout << "getting mutable pattern" << std::endl;
      get();
      t = peek();
      bool if_ref = false;
      bool if_mut = true;
    
      if (!t || t->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier in IdentifierPattern");
    
      auto id_tok = get();
      std::cout << "id_tok in parsing pattern no top alt: " << id_tok->value << std::endl;
      Identifier id(id_tok.value().value);
    
      std::unique_ptr<PatternNoTopAlt> sub_pattern = nullptr;
      auto next = peek();
      if (next && next->type == TokenType::PUNCTUATION && next->value == "@") {
        get();
        sub_pattern = ParsePatternNoTopAlt();
      }
      std::cout << "get identifier pattern" << std::endl;
      return std::make_unique<PatternWithoutRange>(
        std::make_unique<IdentifierPattern>(if_ref, if_mut, std::move(id), std::move(sub_pattern))
      );
    }

    //ref+mut+idpattern
    if (t->value == "ref") {
      get();
      t = peek();
      bool if_ref = true;
      bool if_mut = false;
    
      if (t->value == "mut") if_mut = true;
      t = peek();
      if (!t || t->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier in IdentifierPattern");
    
      auto id_tok = get();
      std::cout << "id_tok in parsing pattern no top alt: " << id_tok->value << std::endl;
      Identifier id(id_tok.value().value);
    
      std::unique_ptr<PatternNoTopAlt> sub_pattern = nullptr;
      auto next = peek();
      if (next && next->type == TokenType::PUNCTUATION && next->value == "@") {
        get();
        sub_pattern = ParsePatternNoTopAlt();
      }
      std::cout << "get identifier pattern" << std::endl;
      return std::make_unique<PatternWithoutRange>(
        std::make_unique<IdentifierPattern>(if_ref, if_mut, std::move(id), std::move(sub_pattern))
      );
    }

    // StructPattern / TupleStructPattern / GroupedPattern / TuplePattern
    if (t->type == TokenType::IDENTIFIER) {
      std::cout << "Get Identifier in patternWithoutRange : " << t->value << std::endl;
      int pre_pos = get_pos();
      try {
        auto pathSegments = std::vector<std::string>{t->value};
        get();
        auto sep = peek(); 
        while (sep && sep->type == TokenType::PUNCTUATION && sep->value == "::") {
          get();
          auto seg = get();
          if (!seg || seg->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier after ::");
          pathSegments.push_back(seg->value);
          sep = peek();
        }
        auto next = peek();
        if (next && next->type == TokenType::DELIMITER && next->value == "{") {
          std::vector<std::variant<PathInType, Identifier>> pathSegments;

          auto tok = get();
          if (!tok || tok->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier at start of StructPattern path");

          pathSegments.push_back(Identifier(tok->value));

          while (true) {
            auto delim = peek();
            if (delim && delim->type == TokenType::PUNCTUATION && delim->value == "::") {
              get();
              auto segTok = get();
              if (!segTok || segTok->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier after :: in StructPattern path");
              pathSegments.push_back(Identifier(segTok->value));
            } else { break; }
          }
        
          auto path = std::make_unique<PathInExpression>(std::move(pathSegments));
        
          get();
          std::vector<std::unique_ptr<StructPatternField>> fields;
          bool hasEtCetera = false;
        
          while (true) {
            auto f = peek();
            if (!f) throw std::runtime_error("Unexpected EOF in StructPattern");
          
            if (f->type == TokenType::DELIMITER && f->value == "}") { get(); break; }
          
            if (f->type == TokenType::PUNCTUATION && f->value == "..") {
              get();
              hasEtCetera = true;
              continue;
            }
          
            auto idTok = get();
            if (!idTok || idTok->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier in StructPatternField");
          
            std::unique_ptr<Pattern> subPattern = nullptr;
            auto atTok = peek();
            if (atTok && atTok->type == TokenType::PUNCTUATION && atTok->value == "@") {
              get();
              subPattern = std::move(ParsePattern());
            }
          
            fields.push_back(std::make_unique<StructPatternField>(Identifier{idTok->value}, std::move(subPattern)));
          
            auto comma = peek();
            if (comma && comma->type == TokenType::PUNCTUATION && comma->value == ",") get();
          }
        
          return std::make_unique<PatternWithoutRange>(
            std::make_unique<StructPattern>(std::move(path), std::move(fields), hasEtCetera)
          );
        } else if (next && next->type == TokenType::DELIMITER && next->value == "(") {
          std::vector<std::variant<PathInType, Identifier>> pathSegments;

          auto tok = get();
          if (!tok || tok->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier at start of TupleStructPattern path");

          pathSegments.push_back(Identifier(tok->value));

          while (true) {
            auto delim = peek();
            if (delim && delim->type == TokenType::PUNCTUATION && delim->value == "::") {
              get();
              auto segTok = get();
              if (!segTok || segTok->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier after :: in TupleStructPattern path");
              pathSegments.push_back(Identifier(segTok->value));
            } else { break; }
          }
        
          auto path = std::make_unique<PathInExpression>(std::move(pathSegments));
        
          get(); //'('
          std::vector<std::unique_ptr<Pattern>> patterns;
        
          while (true) {
            auto p = peek();
            if (!p) throw std::runtime_error("Unexpected EOF in TupleStructPattern");
            if (p->type == TokenType::DELIMITER && p->value == ")") { get(); break; }
            patterns.push_back(std::move(ParsePattern()));
            auto comma = peek();
            if (comma && comma->type == TokenType::PUNCTUATION && comma->value == ",") {
              get();
              continue;
            }
          }
        
          return std::make_unique<PatternWithoutRange>(
            std::make_unique<TupleStructPattern>(std::move(path), std::move(patterns))
          );
        } else {
          auto token = get();
          PathExpressionParselet path_expression_parselet;
          auto node = path_expression_parselet.parse(*this, token.value());
          auto ptr = dynamic_cast<PathExpressionNode*>(node.get());
          return std::make_unique<PatternWithoutRange>(std::move(std::make_unique<PathPattern>(std::unique_ptr<PathExpressionNode>(ptr))));
        }
      } catch(const std::exception& e) { 
        roll_back(pre_pos);
        t = peek();
        std::cout << "parse tuple and struct pattern error : " << e.what() << std::endl;
      };
      bool if_ref = false;
      bool if_mut = false;
    
      if (!t || t->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier in IdentifierPattern");
    
      auto id_tok = get();
      std::cout << "id_tok in parsing pattern no top alt: " << id_tok->value << std::endl;
      Identifier id(id_tok.value().value);
    
      std::unique_ptr<PatternNoTopAlt> sub_pattern = nullptr;
      auto next = peek();
      if (next && next->type == TokenType::PUNCTUATION && next->value == "@") {
        get();
        sub_pattern = ParsePatternNoTopAlt();
      }
      std::cout << "get identifier patterm" << std::endl;
      return std::make_unique<PatternWithoutRange>(
        std::make_unique<IdentifierPattern>(if_ref, if_mut, std::move(id), std::move(sub_pattern))
      );
    }

    if (t->type == TokenType::STRICT_KEYWORD && (t->value == "mut" || t->value == "ref")) {
      bool if_ref = false;
      bool if_mut = false;

      if (t->type == TokenType::STRICT_KEYWORD && t->value == "mut") {
        if_mut = true;
        get();
        t = peek();
      }

      if (t->type == TokenType::STRICT_KEYWORD && t->value == "ref") {
        if_ref = true;
        get();
        t = peek();
      }
    
      if (!t || t->type != TokenType::IDENTIFIER) throw std::runtime_error("Expected identifier in IdentifierPattern");
    
      auto id_tok = get();
      Identifier id(id_tok.value().value);
    
      std::unique_ptr<PatternNoTopAlt> sub_pattern = nullptr;
      auto next = peek();
      if (next && next->type == TokenType::PUNCTUATION && next->value == "@") {
        get();
        sub_pattern = ParsePatternNoTopAlt();
      }
      std::cout << "get identifier patterm" << std::endl;
      return std::make_unique<PatternWithoutRange>(
        std::make_unique<IdentifierPattern>(if_ref, if_mut, std::move(id), std::move(sub_pattern))
      );
    }

    // TuplePattern / GroupedPattern → '(' ... ')'
    if (t->type == TokenType::DELIMITER && t->value == "(") {
      get();
      auto next = peek();
      if (next && next->type == TokenType::DELIMITER && next->value == ")") {
        get();
        return std::make_unique<PatternWithoutRange>(std::make_unique<TuplePattern>(std::vector<std::unique_ptr<Pattern>>{}, false));
      }
      auto first = ParsePattern();
      auto comma = peek();
      if (comma && comma->type == TokenType::PUNCTUATION && comma->value == ",") {
        // tuple
        std::vector<std::unique_ptr<Pattern>> items;
        items.push_back(std::move(first));
        while (true) {
          get();
          auto nxt = peek();
          if (nxt && nxt->type == TokenType::DELIMITER && nxt->value == ")") { get(); break; }
          items.push_back(std::move(ParsePattern()));
        }
        return std::make_unique<PatternWithoutRange>(std::make_unique<TuplePattern>(std::move(items), false));
      } else {
        // grouped
        if (!peek() || peek()->type != TokenType::DELIMITER || peek()->value != ")")
          throw std::runtime_error("Expected ')' in GroupedPattern");
        get();
        return std::make_unique<PatternWithoutRange>(std::make_unique<GroupedPattern>(std::move(first)));
      }
    }

    // SlicePattern → '[' ... ']'
    if (t->type == TokenType::DELIMITER && t->value == "[") {
      get();
      std::vector<std::unique_ptr<Pattern>> items;
      while (true) {
        auto nxt = peek();
        if (!nxt) throw std::runtime_error("Unexpected EOF in SlicePattern");
        if (nxt->type == TokenType::DELIMITER && nxt->value == "]") { get(); break; }
        items.push_back(std::move(ParsePattern()));
        auto comma = peek();
        if (comma && comma->type == TokenType::PUNCTUATION && comma->value == ",") get();
      }
      return std::make_unique<PatternWithoutRange>(std::make_unique<SlicePattern>(std::move(items)));
    }

    throw std::runtime_error("Unknown pattern starting token: " + t->value);
  }

  //PatternWithoutRange → LiteralPattern | IdentifierPattern | WildcardPattern | RestPattern | ReferencePattern | StructPattern
  //                    | TupleStructPattern | TuplePattern | GroupedPattern | SlicePattern | PathPattern
  //RangePattern → RangeExclusivePattern | RangeInclusivePattern | RangeFromPattern | RangeToExclusivePattern | RangeToInclusivePattern | ObsoleteRangePattern​1
  //RangeExclusivePattern → RangePatternBound .. RangePatternBound
  //RangeInclusivePattern → RangePatternBound ..= RangePatternBound
  //RangeFromPattern → RangePatternBound ..
  //RangeToExclusivePattern → .. RangePatternBound
  //RangeToInclusivePattern → ..= RangePatternBound
  //ObsoleteRangePattern → RangePatternBound ... RangePatternBound
  //RangePatternBound → LiteralPattern | PathExpression
  std::unique_ptr<PatternNoTopAlt> parser::ParsePatternNoTopAlt() {
    auto pre_pos = get_pos();
    try {
      auto range = ParseRangePattern();
      if (range) {
        return std::make_unique<PatternNoTopAlt>(std::move(range));
      }
    } catch (const std::exception& e) {
      roll_back(pre_pos);
      std::cerr << "[ParseRangePattern failed] " << e.what() << std::endl;
    }

    try {
      auto pwr = parsePatternWithoutRange();
      if (!pwr) throw std::runtime_error("parsePatternWithoutRange() returned null");
      return std::make_unique<PatternNoTopAlt>(std::move(pwr));
    } catch (const std::exception& e) {
      roll_back(pre_pos);
      std::cerr << "[ParsePatternWithoutRange failed] " << e.what() << std::endl;
      throw std::runtime_error("Both RangePattern and PatternWithoutRange failed");
    }
  }

  std::unique_ptr<ReferenceTypeNode> parser::ParseReferenceType() {
    std::cout << "parsing reference type" << std::endl;
    auto amp = get();
    if (!amp) throw std::runtime_error("Unexpected EOF after '&'");

    bool is_mut = false;
    auto next = peek();
    if (next && next->value == "mut") {
      get();
      std::cout << "getting mut in reference type" << std::endl;
      is_mut = true;
    }

    auto inner_type = ParseType();

    return std::make_unique<ReferenceTypeNode>(std::move(inner_type), is_mut, amp->line, amp->column);
  };

  std::unique_ptr<TypeNode> parser::ParseType() {
    std::cout << "ParseType" << std::endl;
    auto tok = peek();
    std::cout << "first token when parsing type: " << tok->value << std::endl;
    if (!tok) throw std::runtime_error("Unexpected EOF while parsing Type");

    int line = tok->line;
    int column = tok->column;

    if (tok->value == "(") {
      auto tupleNode = ParseTupleType();
      if (tupleNode->types.size() == 1) {
        return std::make_unique<ParenthesizedTypeNode>(std::move(tupleNode->types[0]), line, column);
      }
      return tupleNode;
    }

    if (tok->value == "[") {
      std::cout << "get [" << std::endl;
      auto saved = *tok;
      get();
      auto innerType = ParseType();
      auto next = peek();
      if (next && next->value == ";") {
        get();
        auto expr = parseExpression();
        std::cout << "getArrayType" << std::endl;
        next = get();
        if (!next || next->value != "]") throw std::runtime_error("Expected ] in Array Type");
        return std::make_unique<ArrayTypeNode>(std::move(innerType), std::move(expr), next->line, next->column);
      } else {
        putback(saved);
        return ParseSliceType();
      }
    }

    if (tok->value == "!") return ParseNeverType();
    if (tok->value == "_") return ParseInferredType();
    if (tok->value == "<") return ParseQualifiedPathInType();
    if (tok->value == "&") return ParseReferenceType();

    auto typePath = ParseTypePath();
    std::cout << "getting type path : " << typePath->toString() << std::endl;
    return std::make_unique<TypePathNode>(std::move(typePath), line, column);
  }

  //Statement → ; | Item | LetStatement | ExpressionStatement
  //LetStatement → let PatternNoTopAlt ( : Type )? ( = Expression | = Expression except LazyBooleanExpression or end with a } else BlockExpression)? ;

  std::unique_ptr<LetStatement> parser::ParseLetStatement() {
    std::cout << "begin parsing letstatemnt" << std::endl;
    auto next_token = get();
    if (next_token == std::nullopt || next_token->type != TokenType::STRICT_KEYWORD || next_token->value != "let") {
      throw std::runtime_error("Expected 'let' at beginning of let-statement");
    } 
    std::unique_ptr<PatternNoTopAlt> pattern = ParsePatternNoTopAlt();
    if (!pattern) {
      throw std::runtime_error("Expected pattern after 'let'");
    }
    std::cout << "get pattern in let statement: " << pattern->toString() << std::endl;
    std::unique_ptr<TypeNode> type = nullptr;
    next_token = peek();
    if (next_token && next_token->type == PUNCTUATION && next_token->value == ":") {
      get(); // ":"
      type = ParseType();
      if (!type) {
        throw std::runtime_error("Expected type after ':'");
      }
    }
    std::unique_ptr<ExpressionNode> expr = nullptr;
    next_token = peek();
    if (next_token && next_token->type == TokenType::PUNCTUATION && next_token->value == "=") {
      get(); // "="
      expr = parseExpression();
      if (!expr) {
        throw std::runtime_error("Expected expression after '='");
      } else {
        next_token = get(); //';'或"else"
        if (!next_token) { throw std::runtime_error("Expected something after expression"); }
        if (next_token && next_token->type == TokenType::PUNCTUATION && next_token->value == ";") {
          int ll = expr->get_l(), cc = expr->get_c();
          return std::make_unique<LetStatement>(std::move(pattern), std::move(type), std::move(expr), nullptr);
        } else {//先检查expression，再parse blockexpression
          if (!is_type<BlockExpressionNode, ExpressionNode>(expr)) { throw std::runtime_error("Expected expression except LazyBooleanExpression or end with a }"); }
          std::unique_ptr<ExpressionNode> expr2 = parseExpression();
          if (!is_type<BlockExpressionNode, ExpressionNode>(expr2)) { throw std::runtime_error("Expected BlockExpression after 'else'"); }
          next_token = get();
          if (!next_token || next_token->type != TokenType::PUNCTUATION || next_token->value != ";") {
            throw std::runtime_error("Expected ';' after block expression");
          }
          int ll = expr->get_l(), cc = expr->get_c();
          std::unique_ptr<BlockExpressionNode> BlockExpr(static_cast<BlockExpressionNode*>(expr2.release()));

          next_token = get();
          if (!next_token || next_token->type != TokenType::PUNCTUATION || next_token->value != ";") {
            throw std::runtime_error("Expected ';' at end of let-statement");
          }
          return std::make_unique<LetStatement>(std::move(pattern), std::move(type), std::move(expr), std::move(BlockExpr));
        }
      }
    }
    throw std::runtime_error("Parse LetStatement Error");
  };

  std::unique_ptr<TraitNode> parser::ParseTraitItem() {
    std::cout << "parsing trait item" << std::endl;
    int startLine = peek()->line;
    int startColumn = peek()->column;

    bool isUnsafe = false;
    if (auto t = peek(); t && t->type == TokenType::STRICT_KEYWORD && t->value == "unsafe") {
      isUnsafe = true;
      get();
    }

    auto t = get();
    if (!t || t->type != TokenType::STRICT_KEYWORD || t->value != "trait") throw std::runtime_error("'trait' expected");

    t = get();
    if (!t || t->type != TokenType::IDENTIFIER) throw std::runtime_error("trait name expected");
    std::string traitName = t->value;

    std::unique_ptr<TypeNode> typeParamBounds = nullptr;
    if (auto next = peek(); next && next->type == TokenType::PUNCTUATION && next->value == ":") {
      get();
      typeParamBounds = std::move(ParseType());
    }

    t = get();
    if (!t || t->type != TokenType::PUNCTUATION && t->value == "{") throw std::runtime_error("'{' expected");

    std::vector<std::unique_ptr<AssociatedItemNode>> items;

    while (true) {
      auto peekTok = peek();
      if (!peekTok) {
        throw std::runtime_error("Unexpected EOF while parsing implementation block");
      }
      if (peekTok->value == "}") { get(); break; }

      if (peekTok->value == "const") {
        auto constItem = ParseConstItem();
        items.push_back(
          std::make_unique<AssociatedItemNode>(std::move(constItem), peekTok->line, peekTok->column)
        );
      }
      else if (peekTok->value == "fn") {
        auto funcItem = ParseFunctionItem();
        funcItem->impl_type_name = traitName;
        items.push_back(
          std::make_unique<AssociatedItemNode>(std::move(funcItem), peekTok->line, peekTok->column)
        );
      }
      else {
        throw std::runtime_error("Unexpected token in implementation block: " + peekTok->value);
      }
    }

    std::cout << "next token : " << peek()->value << std::endl;

    return std::make_unique<TraitNode>(isUnsafe, traitName, std::move(typeParamBounds),
                                      std::move(items), startLine, startColumn);
  };

  std::unique_ptr<ItemNode> parser::ParseItem() {
    std::cout << "ParseItem" << std::endl;
    auto tok = peek();
    if (!tok) return nullptr;

    if (tok->type == TokenType::STRICT_KEYWORD) {
      if (tok->value == "fn") {
        std::cout << "parsing function" << std::endl;
        try {
          return ParseFunctionItem();
        } catch (const std::exception& e) {
          std::cerr << "Error while parsing function: " << e.what() << std::endl;
          throw;
        }
      } else if (tok->value == "struct") {
        auto v = ParseStructItem();
        std::unique_ptr<ItemNode> node;
        if (auto* p = std::get_if<std::unique_ptr<TupleStructNode>>(&v)) {
          node = std::move(*p);
        } else if (auto* p = std::get_if<std::unique_ptr<StructStructNode>>(&v)) {
          node = std::move(*p);
        }
        std::cout << "getting struct" << std::endl;
        return node;
      } else if (tok->value == "enum") {
        return ParseEnumItem();
      } else if (tok->value == "const") {
        return ParseConstItem();
      } else if (tok->value == "impl") {
        auto pre_pos = get_pos();
        try {
          return ParseInherentImplItem();
        } catch (const std::exception& e1) {
          std::cerr << "[parser] Inherent impl parse failed: " << e1.what() << "\n";
          roll_back(pre_pos);
        
          try {
            return ParseTraitImplItem();
          } catch (const std::exception& e2) {
            std::cerr << "[parser] Trait impl parse failed: " << e2.what() << "\n";
            throw std::runtime_error("Failed to parse either inherent or trait impl");
          }
        }
      } else if (tok->value == "mod") {
        return ParseModuleItem();
      } else if (tok->value == "trait") {
        return ParseTraitItem();
      } else if (tok->value == "unsafe") {
        tok = get();
        auto next = peek();
        putback(tok.value());
        if (next->value == "impl") {
          return ParseTraitImplItem();
        } else if (next->value == "trait") {
          return ParseTraitItem();
        }
      }
    }
    throw std::runtime_error("Unknown item" + tok->value);
  };

  std::unique_ptr<ModuleNode> parser::ParseModuleItem() {
    auto mod_tok = get();
    if (!mod_tok || mod_tok->type != TokenType::STRICT_KEYWORD || mod_tok->value != "mod") {
      throw std::runtime_error("Expected 'mod' at module item");
    }
    auto id_tok = get();
    if (!id_tok || id_tok->type != IDENTIFIER) {
      throw std::runtime_error("Expected identifier after 'mod'");
    }
    std::string module_name = id_tok->value;
    auto next = peek();
    if (!next) {
      throw std::runtime_error("Unexpected end after module name");
    }

    if (next->type == TokenType::PUNCTUATION && next->value == ";") {
      get();
      return std::make_unique<ModuleNode>(module_name, next->line, next->column, std::vector<std::unique_ptr<ItemNode>>{});
    } 
    else if (next->type == TokenType::PUNCTUATION && next->value == "{") {
      get();
      std::vector<std::unique_ptr<ItemNode>> items;
      while (true) {
        auto maybe = peek();
        if (!maybe) throw std::runtime_error("Unexpected EOF in module body");
        if (maybe->type == TokenType::PUNCTUATION && maybe->value == "}") { get(); break; }
        auto item = ParseItem();
        if (item) items.push_back(std::move(item));
      }
      return std::make_unique<ModuleNode>(module_name, next->line, next->column, std::move(items));
    }
    throw std::runtime_error("Expected ';' or '{' after module name");
  };

  /*functions used in parsing functions*/
  FunctionQualifier parser::parseFunctionQualifier() {
    auto next = peek();
    FunctionQualifier fq;

    while (next && next->type == TokenType::STRICT_KEYWORD) {
      if (next->value == "const") {
        fq.is_const = true; get();
      } else if (next->value == "async") {
        fq.is_async = true; get();
      } else if (next->value == "unsafe") {
        fq.is_unsafe = true; get();
      } else if (next->value == "extern") {
        fq.has_extern = true; get();
      } else { break; }
      next = peek();
    }

    if (next && (next->type == TokenType::STRING_LITERAL || next->type == TokenType::RAW_STRING_LITERAL)) {
      fq.abi = next->value; get();
    }

    return fq;
  }

  //FunctionParameters → SelfParam ,? | ( SelfParam , )? FunctionParam ( , FunctionParam )* ,?
  std::unique_ptr<FunctionParameter> parser::ParseFunctionParameters() {
    auto tok = get();
    if (!tok || tok->value != "(") { throw std::runtime_error("Expected '(' at start of function parameter list"); }

    std::unique_ptr<SelfParam> self_param = nullptr;
    std::vector<std::unique_ptr<FunctionParam>> params;

    tok = peek();
    if (tok && tok->value == ")") {
      get(); //  ')'
      return std::make_unique<FunctionParameter>(2, std::move(params));
    }
    
    bool has_self = false;
    auto pre_pos = get_pos();
    while (tok->value != ")") {
      if (tok->value == "self") {
        has_self = true;
      }
      get();
      tok = peek();
    }
    roll_back(pre_pos);
    tok = peek();
    if (has_self && tok && (tok->value == "self" || tok->value == "&" || tok->value == "mut")) {
      has_self = true;
      bool if_prefix = false;
      bool if_mut = false;

      if (tok->value == "&") {
        if_prefix = true;
        get(); // &
        tok = peek();
      }

      if (tok && tok->value == "mut") {
        if_mut = true;
        get(); // mut
        tok = peek();
      }

      if (!tok || tok->value != "self") {
        std::cout << "has mut but is not self" << std::endl;
        has_self = false;
      }
      get();

      // TypedSelf: mut? self : Type
      tok = peek();
      if (tok && tok->value == ":") {
        get(); //  :
        auto type = ParseType();
        auto typed_self = std::make_unique<TypedSelf>(if_mut, std::move(type));
        self_param = std::make_unique<SelfParam>(std::move(typed_self));
      } else {
        auto shorthand_self = std::make_unique<ShorthandSelf>(if_prefix, if_mut);
        self_param = std::make_unique<SelfParam>(std::move(shorthand_self));
      }

      tok = peek();
      if (tok && tok->value == ",") {
        get(); // ','
      }
      if (!has_self) roll_back(pre_pos);
    }

    tok = peek();
    while (tok && tok->value != ")") {
      auto pattern = ParsePatternNoTopAlt();
      tok = peek();

      if (tok && tok->value == ":") {
        get(); // ':'
        tok = peek();
        if (tok && tok->value == "...") {
          get();
          params.emplace_back(std::make_unique<FunctionParam>(std::make_unique<ellipsis>()));
        } else {
          auto type = ParseType();
          auto fpp = std::make_unique<FunctionParamPattern>(std::move(pattern), std::move(type));
          params.emplace_back(std::make_unique<FunctionParam>(std::move(fpp)));
        }
      } else {
        auto type = ParseType();
        params.emplace_back(std::make_unique<FunctionParam>(std::move(type)));
      }

      tok = peek();
      if (tok && tok->value == ",") {
        get(); // ','
        tok = peek();
        continue;
      } else { break; }
    }

    tok = get();
    if (!tok || tok->value != ")") {
      throw std::runtime_error("Expected ')' at end of function parameter list");
    }

    if (has_self && params.empty()) {
      return std::make_unique<FunctionParameter>(1, std::move(self_param), std::move(params));
    } else if (has_self || !params.empty()) {
      return std::make_unique<FunctionParameter>(2, std::move(self_param), std::move(params));
    } else {
      return std::make_unique<FunctionParameter>(2, std::move(params));
    }
  }

  std::unique_ptr<FunctionReturnType> parser::ParseFunctionReturnType() {
    auto return_type = ParseType();
    return std::make_unique<FunctionReturnType>(std::move(return_type));
  }

  std::unique_ptr<FunctionNode> parser::ParseFunctionItem() {
    std::cout << "ParseFunctionItem" << std::endl;
    FunctionQualifier fq = parseFunctionQualifier();
    auto fn_tok = get();
    if (!fn_tok || fn_tok->value != "fn") {
      throw std::runtime_error("Expected 'fn' keyword");
    }
    auto id_tok = get();
    if (!id_tok || id_tok->type != TokenType::IDENTIFIER) {
      throw std::runtime_error("Expected function identifier after 'fn'");
    }
    std::string identifier = id_tok->value;
    std::cout << "parsing function with id: " << identifier << std::endl;
 
    auto func = std::make_unique<FunctionNode>(fq, identifier, id_tok->line, id_tok->column);

    auto next = peek();
    if (!next || next->value != "(") {
      throw std::runtime_error("Expected '(' after function name");
    }
    func->function_parameter = ParseFunctionParameters();
    next = peek();

    if (next && next->value == "->") {
      std::cout << "has return type" << std::endl;
      get(); //consume "->"
      func->return_type = ParseFunctionReturnType();
    }
    next = peek();

    if (next && next->value == "{") {
      std::cout << "parse block expression in function item" << std::endl;
      try {
        func->block_expression = std::move(parseBlockExpression());
      } catch (const std::exception& e) {
        std::cerr << "[Parse BlockExpression Error]: " << e.what() << std::endl;
        throw std::runtime_error("error in parsing block expression");
      }
    } else if (next && next->value == ";") {
      std::cout << "finish parsing unimplemented function" << std::endl;
      get();
    } else {
      throw std::runtime_error("Expected function body or ';'");
    }

    return func;
  }

  std::unique_ptr<FunctionNode> parser::ParseFunctionItemInImpl(const std::string& impl_type_name) {
    std::cout << "ParseFunctionItemInImpl" << std::endl;
    FunctionQualifier fq = parseFunctionQualifier();

    auto fn_tok = get();
    if (!fn_tok || fn_tok->value != "fn") {
      throw std::runtime_error("Expected 'fn' keyword");
    }

    auto id_tok = get();
    if (!id_tok || id_tok->type != TokenType::IDENTIFIER) {
      throw std::runtime_error("Expected function identifier after 'fn'");
    }
    std::string identifier = id_tok->value;

    auto func = std::make_unique<FunctionNode>(fq, identifier, id_tok->line, id_tok->column);

    auto next = peek();
    if (!next || next->value != "(") {
      throw std::runtime_error("Expected '(' after function name");
    }
    func->function_parameter = ParseFunctionParameters();

    next = peek();
    if (next && next->value == "->") {
      std::cout << "has return type" << std::endl;
      get();
      func->return_type = ParseFunctionReturnType();
    }

    next = peek();
    if (next && next->value == "{") {
      std::cout << "parse block expression" << std::endl;
      func->block_expression = std::move(parseBlockExpression());
    } else if (next && next->value == ";") {
      get();
    } else {
      throw std::runtime_error("Expected function body or ';'");
    }
    func->impl_type_name = impl_type_name;
    return func;
  }

  std::unique_ptr<StructStructNode> parser::ParseStructStruct(std::string id) {
    std::cout << "parsing struct struct" << std::endl;
    // 判断 { ... } 或 ;
    auto next = peek();
    auto node = std::make_unique<StructStructNode>(id, next->line, next->column);

    if (!next) throw std::runtime_error("Unexpected EOF after struct declaration");

    if (next->value == ";") {
      get(); // ';'
      
      return node;
    }

    if (next->value == "{") {
      get(); // '{'
      std::vector<std::unique_ptr<StructField>> fields;
      while (true) {
        auto tok = peek();
        if (!tok) throw std::runtime_error("Unexpected EOF in struct fields");
        if (tok->value == "}") { get(); break; }

        // StructField: IDENTIFIER : Type
        auto id = get();
        if (!id || id->type != TokenType::IDENTIFIER) {
          throw std::runtime_error("Expected identifier in struct field");
        }
        auto colon = get();
        if (!colon || colon->value != ":") {
          throw std::runtime_error("Expected ':' in struct field");
        }
        auto typeNode = ParseType();
        std::cout << "getting type : " << typeNode->toString() << std::endl;
        fields.push_back(std::make_unique<StructField>(id->value, std::move(typeNode)));

        auto sep = peek();
        if (sep && sep->value == ",") { get(); continue; }
      }
      node->struct_fields = std::make_unique<StructFieldNode>(std::move(fields), next->line, next->column);
      return node;
    }
    std::cout << "parsing structstruct error" << std::endl;
    throw std::runtime_error("Expected '{' or ';' in struct declaration");
  }

  std::unique_ptr<TupleStructNode> parser::ParseTupleStruct(std::string id) {
    auto next = peek();
    auto node = std::make_unique<TupleStructNode>(id, next->line, next->column);

    if (!next || next->value != "(") { throw std::runtime_error("Expected '(' in tuple struct"); }
    get(); // '('

    std::vector<std::unique_ptr<TupleField>> fields;
    while (true) {
      auto tok = peek();
      if (!tok) throw std::runtime_error("Unexpected EOF in tuple struct");
      if (tok->value == ")") { get(); break; }

      //TupleField → Type
      auto typeNode = ParseType();
      fields.push_back(std::make_unique<TupleField>(std::move(typeNode)));

      auto sep = peek();
      if (sep && sep->value == ",") { get(); continue; }
    }

    node->tuple_fields = std::make_unique<TupleFieldNode>(
      std::move(fields), next->line, next->column
    );

    auto semi = get();
    if (!semi || semi->value != ";") { throw std::runtime_error("Expected ';' after tuple struct"); }

    return node;
  }

  std::variant<std::unique_ptr<TupleStructNode>, std::unique_ptr<StructStructNode>> parser::ParseStructItem() {
    auto tok = get();
    if (!tok || tok->value != "struct") { throw std::runtime_error("Expected 'struct'"); }

    auto next = peek();
    if (!next) { throw std::runtime_error("Unexpected end after 'struct'"); }

    if (next->type != TokenType::IDENTIFIER) { throw std::runtime_error("Expected identifier in struct"); }

    auto id_token = get();
    std::string id = id_token->value;
    next = peek();

    if (next->value == "{") { 
      std::cout << "parsing struct struct" << std::endl;
      return ParseStructStruct(id);
    } else if (next->value == "(") {
      std::cout << "parsing tuple struct" << std::endl;
      return ParseTupleStruct(id);
    } else if (next->value == ";") {
      std::cout << "parsing struct struct" << std::endl;
      return ParseStructStruct(id);
    } else {
      std::cout << "invalid token after struct name" << std::endl;
      throw std::runtime_error("Expected '{', '(' or ';' after struct name");
    }
  }

  std::unique_ptr<TupleFieldNode> parser::ParseTupleFields() {
    auto startTok = peek();
    if (!startTok) { throw std::runtime_error("Unexpected EOF while parsing tuple fields"); }
    int line = startTok->line;
    int column = startTok->column;

    std::vector<std::unique_ptr<TupleField>> tupleFields;

    while (true) {
      auto tok = peek();
      if (!tok || tok->value == ")") break;
      auto typeNode = ParseType();
      tupleFields.push_back(std::make_unique<TupleField>(std::move(typeNode)));

      tok = peek();
      if (!tok) { throw std::runtime_error("Unexpected EOF while parsing tuple fields"); }
      if (tok->value == ",") {
        get();
        if (peek() && peek()->value == ")") break;
        continue;
      }
      else if (tok->value == ")") break;
      else throw std::runtime_error("Expected ',' or ')' in tuple fields");
    }

    return std::make_unique<TupleFieldNode>(std::move(tupleFields), line, column);
  }

  std::unique_ptr<StructFieldNode> parser::ParseStructFields() {
    auto startTok = peek();
    if (!startTok) {
      throw std::runtime_error("Unexpected EOF while parsing struct fields");
    }
    int line = startTok->line;
    int column = startTok->column;

    std::vector<std::unique_ptr<StructField>> structFields;

    while (true) {
      auto tok = peek();
      if (!tok || tok->value == "}") { break; }

      auto idTok = get();
      if (!idTok || idTok->type != TokenType::IDENTIFIER) {
        throw std::runtime_error("Expected identifier in struct field");
      }
      std::string identifier = idTok->value;

      tok = peek();
      if (!tok || tok->value != ":") {
        throw std::runtime_error("Expected ':' after identifier in struct field");
      }
      get(); // ':'

      auto typeNode = ParseType();

      structFields.push_back(std::make_unique<StructField>(identifier, std::move(typeNode)));

      tok = peek();
      if (!tok) {
        throw std::runtime_error("Unexpected EOF while parsing struct fields");
      }
      if (tok->value == ",") {
        get();
        if (peek() && peek()->value == "}") break;
        continue;
      }
      else if (tok->value == "}") { break; }
      else {
        throw std::runtime_error("Expected ',' or '}' after struct field");
      }
    }
    return std::make_unique<StructFieldNode>(std::move(structFields), line, column);
  }


  std::unique_ptr<EnumVariantNode> parser::ParseEnumVariant() {
    auto startTok = peek();
    if (!startTok) {
      throw std::runtime_error("Unexpected EOF while parsing EnumVariant");
    }
    int line = startTok->line;
    int column = startTok->column;

    auto nameTok = get();
    if (!nameTok || nameTok->type != TokenType::IDENTIFIER) {
      throw std::runtime_error("Expected identifier in enum variant");
    }

    auto variantNode = std::make_unique<EnumVariantNode>(nameTok->value);

    auto tok = peek();
    if (tok) {
      if (tok->value == "(") {
        get();
        auto tupleNode = std::make_unique<EnumVariantTupleNode>(tok->line, tok->column);

        if (peek() && peek()->value != ")") {
          tupleNode->tuple_field = ParseTupleFields();
        }
        tok = peek();
        if (!tok || tok->value != ")") {
          throw std::runtime_error("Expected ')' after tuple fields in enum variant");
        }
        get(); // ')'
        variantNode->enum_variant_tuple = std::move(tupleNode);
      } 
      else if (tok->value == "{") {
        get(); // '{'
        auto structNode = std::make_unique<EnumVariantStructNode>(tok->line, tok->column);

        if (peek() && peek()->value != "}") {
            structNode->struct_field = ParseStructFields();
        }
        tok = peek();
        if (!tok || tok->value != "}") {
          throw std::runtime_error("Expected '}' after struct fields in enum variant");
        }
        get(); // '}'

        variantNode->enum_variant_struct = std::move(structNode);
      }
    }

    tok = peek();
    if (tok && tok->value == "=") {
      get();
      auto expr = parseExpression();
      variantNode->discriminant = std::make_unique<EnumVariantDiscriminantNode>(std::move(expr), tok->line, tok->column);
    }

    return variantNode;
  }


  std::unique_ptr<EnumVariantsNode> parser::ParseEnumVariants() {
    auto startTok = peek();
    int line = startTok ? startTok->line : 0;
    int column = startTok ? startTok->column : 0;

    auto node = std::make_unique<EnumVariantsNode>(line, column);

    bool expectVariant = true;
    while (true) {
      auto tok = peek();
      if (!tok) { throw std::runtime_error("Unexpected EOF while parsing enum variants"); }
      if (tok->value == "}") { break; }

      if (!expectVariant && tok->value != ",") {
          throw std::runtime_error("Expected ',' or '}' after enum variant");
      }

      if (expectVariant) {
        node->enum_variants.push_back(ParseEnumVariant());
        expectVariant = false;
      } else {
        if (tok->value == ",") {
          get();
          auto next = peek();
          if (next && next->value == "}") { break; }
          expectVariant = true;
        }
      }
    }
    return node;
  }

  std::unique_ptr<EnumerationNode> parser::ParseEnumItem() {
    auto tok = get();
    if (!tok || tok->value != "enum") { throw std::runtime_error("Expected 'enum'"); }

    auto idTok = get();
    if (!idTok || idTok->type != TokenType::IDENTIFIER) { throw std::runtime_error("Expected identifier after 'enum'"); }

    auto node = std::make_unique<EnumerationNode>(idTok->value, idTok->line, idTok->column);

    auto brace = get();
    if (!brace || brace->value != "{") { throw std::runtime_error("Expected '{' after enum name"); }

    auto next = peek();
    if (next && next->value != "}") { node->enum_variants = ParseEnumVariants(); }

    auto close = get();
    if (!close || close->value != "}") { throw std::runtime_error("Expected '}' after enum variants"); }

    return node;
  };

  std::unique_ptr<ConstantItemNode> parser::ParseConstItem() {
    auto tok = get();
    if (!tok || tok->value != "const") { throw std::runtime_error("Expected 'const' at beginning of constant item"); }
    int line = tok->line;
    int column = tok->column;

    auto idTok = get();
    if (!idTok) { throw std::runtime_error("Unexpected EOF after 'const'"); }

    std::unique_ptr<ConstantItemNode> node;
    if (idTok->type == TokenType::IDENTIFIER) {
      node = std::make_unique<ConstantItemNode>(idTok->value, line, column);
    } else if (idTok->value == "_") {
      node = std::make_unique<ConstantItemNode>(line, column);
    } else {
      throw std::runtime_error("Expected identifier or '_' after 'const'");
    }

    auto colonTok = get();
    if (!colonTok || colonTok->value != ":") {
      throw std::runtime_error("Expected ':' after const name");
    }

    node->type = ParseType();

    auto eqTok = get();
    if (!eqTok || eqTok->value != "=") {
      throw std::runtime_error("Expected '=' after const type");
    }
    node->expression = parseExpression();
    auto semiTok = get();
    if (!semiTok || semiTok->value != ";") {
      throw std::runtime_error("Expected ';' after const expression");
    }
    return node;
  }

  std::unique_ptr<InherentImplNode> parser::ParseInherentImplItem() {
    std::cout << "parsing inherent implementation node" << std::endl;
    auto tok = get();
    if (!tok || tok->value != "impl") {
      throw std::runtime_error("Expected 'impl' at beginning of implementation");
    }
    int line = tok->line;
    int column = tok->column;

    auto type = ParseType();
    if (!type) {
      throw std::runtime_error("Expected type after 'impl'");
    }

    auto lbraceTok = get();
    if (!lbraceTok || lbraceTok->value != "{") {
      throw std::runtime_error("Expected '{' after type in implementation");
    }

    std::vector<std::unique_ptr<AssociatedItemNode>> items;

    auto implNode = std::make_unique<InherentImplNode>(std::move(type), std::move(items), line, column);

    while (true) {
      auto peekTok = peek();
      if (!peekTok) {
        throw std::runtime_error("Unexpected EOF while parsing implementation block");
      }
      if (peekTok->value == "}") { get(); break; }

      if (peekTok->value == "const") {
        auto constItem = ParseConstItem();
        implNode->associated_item.push_back(
          std::make_unique<AssociatedItemNode>(std::move(constItem), peekTok->line, peekTok->column)
        );
      } else if (peekTok->value == "fn") {
        auto funcItem = ParseFunctionItem();
        funcItem->impl_type_name = implNode->type->toString();
        implNode->associated_item.push_back(
          std::make_unique<AssociatedItemNode>(std::move(funcItem), peekTok->line, peekTok->column)
        );
      } else {
        throw std::runtime_error("Unexpected token in implementation block: " + peekTok->value);
      }
    }

    return implNode;
  }

  std::unique_ptr<TraitImplNode> parser::ParseTraitImplItem() {
    std::cout << "parsing trait implementation node\n";

    bool isUnsafe = false;
    auto tok = peek();
    if (tok && tok->value == "unsafe") {
      isUnsafe = true;
      get();
    }
    
    tok = get();
    if (!tok || tok->value != "impl") {
      throw std::runtime_error("Expected 'impl' at beginning of trait implementation");
    }
    int line = tok->line;
    int column = tok->column;

    bool isNegative = false;
    auto nextTok = peek();
    if (nextTok && nextTok->value == "!") {
      isNegative = true;
      get();
    }

    auto traitType = ParseTypePath();
    if (!traitType) {
      throw std::runtime_error("Expected trait name (TypePath) after 'impl'");
    }

    auto forTok = get();
    if (!forTok || forTok->value != "for") {
      throw std::runtime_error("Expected 'for' after trait name in trait implementation");
    }

    auto targetType = ParseType();
    if (!targetType) {
      throw std::runtime_error("Expected type after 'for' in trait implementation");
    }

    auto lbraceTok = get();
    if (!lbraceTok || lbraceTok->value != "{") {
      throw std::runtime_error("Expected '{' after 'for Type' in trait implementation");
    }

    std::vector<std::unique_ptr<AssociatedItemNode>> items;

    while (true) {
      auto peekTok = peek();
      if (!peekTok) {
        throw std::runtime_error("Unexpected EOF while parsing trait implementation block");
      }

      if (peekTok->value == "}") {
        get();
        break;
      }

      if (peekTok->value == "const") {
        auto constItem = ParseConstItem();
        items.push_back(std::make_unique<AssociatedItemNode>(std::move(constItem), peekTok->line, peekTok->column));
      } else if (peekTok->value == "fn") {
        auto funcItem = ParseFunctionItem();
        funcItem->impl_type_name = targetType->toString();
        items.push_back(std::make_unique<AssociatedItemNode>(std::move(funcItem), peekTok->line, peekTok->column));
      } else {
        throw std::runtime_error("Unexpected token in trait implementation block: " + peekTok->value);
      }
    }

    return std::make_unique<TraitImplNode>(isUnsafe, isNegative, std::move(traitType), std::move(targetType), std::move(items), line, column);
  }

  bool is_ExpressionWithoutBlock(const ExpressionNode* expr) {
    if (!expr) return false;

    return dynamic_cast<const ExpressionWithoutBlockNode*>(expr) ||
           dynamic_cast<const LiteralExpressionNode*>(expr) ||
           dynamic_cast<const PathExpressionNode*>(expr) ||
           dynamic_cast<const OperatorExpressionNode*>(expr) ||
           dynamic_cast<const GroupedExpressionNode*>(expr) ||
           dynamic_cast<const ArrayExpressionNode*>(expr) ||
           dynamic_cast<const IndexExpressionNode*>(expr) ||
           dynamic_cast<const TupleExpressionNode*>(expr) ||
           dynamic_cast<const TupleIndexingExpressionNode*>(expr) ||
           dynamic_cast<const CallExpressionNode*>(expr) ||
           dynamic_cast<const MethodCallExpressionNode*>(expr) ||
           dynamic_cast<const FieldExpressionNode*>(expr) ||
           dynamic_cast<const RangeExpressionNode*>(expr) ||
           dynamic_cast<const ReturnExpressionNode*>(expr) ||
           dynamic_cast<const UnderscoreExpressionNode*>(expr) || 
           dynamic_cast<const LazyBooleanExpressionNode*>(expr) ||
           dynamic_cast<const StructExpressionNode*>(expr) || 
           dynamic_cast<const ArithmeticOrLogicalExpressionNode*>(expr) ||
           dynamic_cast<const TypeCastExpressionNode*>(expr) ||
           dynamic_cast<const NegationExpressionNode*>(expr) || 
           dynamic_cast<const DereferenceExpressionNode*>(expr);
  }

  std::unique_ptr<ExpressionStatement> parser::parseExpressionStatement() {
    auto startTok = peek();
    if (!startTok) {
      throw std::runtime_error("Unexpected EOF while parsing ExpressionStatement");
    }
    std::cout << "try parsing expressionstatement with token: " << startTok->value << std::endl;
    int line = startTok->line;
    int column = startTok->column;

    auto expr = parseExpression();

    if (is_ExpressionWithoutBlock(expr.get())) {
      auto next = peek();
      if (next->value != ";") {
        throw std::runtime_error("Expected ';' after expression without block");
      } else {
        get();
      }
    }

    std::cout << "successfully parsed expressionstatement" << std::endl;

    return std::make_unique<ExpressionStatement>(std::move(expr));
  }

  std::unique_ptr<StatementNode> parser::ParseStatement() {
    auto tok = peek();
    if (!tok) {
      throw std::runtime_error("Unexpected EOF while parsing Statement");
    }
    int line = tok->line;
    int column = tok->column;

    if (tok->value == ";") {
      get();
      return std::make_unique<StatementNode>(StatementType::SEMICOLON, nullptr, nullptr, nullptr, line, column);
    }

    if (IsItemStart(*tok)) {
      auto item = ParseItem();
      return std::make_unique<StatementNode>(StatementType::ITEM, std::move(item), nullptr, nullptr, line, column);
    }

    if (tok->value == "let") {
      auto letStmt = ParseLetStatement();
      return std::make_unique<StatementNode>(StatementType::LETSTATEMENT, nullptr, std::move(letStmt), nullptr, line, column);
    }

    auto exprStmt = parseExpressionStatement();
    return std::make_unique<StatementNode>(StatementType::EXPRESSIONSTATEMENT, nullptr, nullptr, std::move(exprStmt), line, column);
  }

  std::unique_ptr<ParenthesizedTypeNode> parser::ParseParenthesizedType() {
    auto lparenTok = get();
    if (!lparenTok || lparenTok->value != "(") {
      throw std::runtime_error("Expected '(' at start of ParenthesizedType");
    }
    int line = lparenTok->line;
    int column = lparenTok->column;

    auto innerType = ParseType();

    auto rparenTok = get();
    if (!rparenTok || rparenTok->value != ")") {
      throw std::runtime_error("Expected ')' after ParenthesizedType");
    }

    return std::make_unique<ParenthesizedTypeNode>(std::move(innerType), line, column);
  }

  std::unique_ptr<TupleTypeNode> parser::ParseTupleType() {
    auto lparenTok = get();
    if (!lparenTok || lparenTok->value != "(") {
      throw std::runtime_error("Expected '(' at start of TupleType");
    }
    int line = lparenTok->line;
    int column = lparenTok->column;

    std::vector<std::unique_ptr<TypeNode>> types;

    if (peek()->value == ")") {
      get();
      return std::make_unique<TupleTypeNode>(std::move(types), line, column);
    }

    while (true) {
      types.push_back(ParseType());
      auto next = peek();
      if (!next) throw std::runtime_error("Unexpected EOF in TupleType");

      if (next->value == ",") {
        get();
        if (peek()->value == ")") { break; }
        continue;
      }
      break;
    }

    auto rparenTok = get();
    if (!rparenTok || rparenTok->value != ")") {
      throw std::runtime_error("Expected ')' at end of TupleType");
    }

    return std::make_unique<TupleTypeNode>(std::move(types), line, column);
  }

  std::unique_ptr<NeverTypeNode> parser::ParseNeverType() {
    auto tok = get();
    if (!tok || tok->value != "!") {
      throw std::runtime_error("Expected '!' for NeverType");
    }
    return std::make_unique<NeverTypeNode>(tok->line, tok->column);
  }

  std::unique_ptr<ArrayTypeNode> parser::ParseArrayType() {
    try {
      auto lbrackTok = get();
      if (!lbrackTok || lbrackTok->value != "[") {
        throw std::runtime_error("Expected '[' at start of ArrayType");
      }
      int line = lbrackTok->line;
      int column = lbrackTok->column;
      std::cout << "Begin parsing type" << std::endl;
      auto innerType = ParseType();
      std::cout << "Begin parsing type" << std::endl;
      auto semiTok = get();
      if (!semiTok || semiTok->value != ";") {
        throw std::runtime_error("Expected ';' in ArrayType");
      }
      std::cout << "Begin parsing expression" << std::endl;
      auto expr = parseExpression();
      std::cout << "Finish parsing expression" << std::endl;
      auto rbrackTok = get();
      if (!rbrackTok || rbrackTok->value != "]") {
        throw std::runtime_error("Expected ']' at end of ArrayType");
      }
      std::cout << "get array type" << std::endl;
      return std::make_unique<ArrayTypeNode>(std::move(innerType), std::move(expr), line, column);
    } catch (const std::exception& e) {
      std::cerr << "[ParseArrayType Error] : " << e.what() << std::endl;
    }
    throw std::runtime_error("Parse Array Type falied");
  }

  std::unique_ptr<SliceTypeNode> parser::ParseSliceType() {
    auto lbrackTok = get();
    if (!lbrackTok || lbrackTok->value != "[") {
      throw std::runtime_error("Expected '[' at start of SliceType");
    }
    int line = lbrackTok->line;
    int column = lbrackTok->column;

    auto innerType = ParseType();

    auto rbrackTok = get();
    if (!rbrackTok || rbrackTok->value != "]") {
      throw std::runtime_error("Expected ']' at end of SliceType");
    }

    return std::make_unique<SliceTypeNode>(std::move(innerType), line, column);
  }

std::unique_ptr<TypePathFn> parser::ParseTypePathFn() {
    auto lparen = get();
    if (!lparen || lparen->value != "(") throw std::runtime_error("Expected '(' at start of TypePathFn");

    std::vector<std::unique_ptr<TypeNode>> inputs;
    if (peek() && peek()->value != ")") {
      inputs.push_back(ParseType());
      while (peek() && peek()->value == ",") {
        get();
        if (peek() && peek()->value == ")") break;
        inputs.push_back(ParseType());
      }
    }

    auto rparen = get();
    if (!rparen || rparen->value != ")") throw std::runtime_error("Expected ')' at end of TypePathFn inputs");

    std::unique_ptr<TypeNode> returnType = nullptr;
    if (peek() && peek()->value == "->") {
      get();
      returnType = ParseType();
      if (!returnType) throw std::runtime_error("Expected TypeNoBounds after '->'");
    }

    std::unique_ptr<TypePathFnInputs> inputsNode = nullptr;
    if (!inputs.empty()) inputsNode = std::make_unique<TypePathFnInputs>(std::move(inputs));

    return std::make_unique<TypePathFn>(std::move(inputsNode), std::move(returnType));
  }

  std::unique_ptr<PathIdentSegment> parser::ParsePathIdentSegment() {
    auto tok = get();
    if (!tok) throw std::runtime_error("Unexpected EOF while parsing PathIdentSegment");

    if (tok->type == TokenType::IDENTIFIER) { 
      std::cout << "get identifier in pathIdentSegment : " << tok->value << std::endl;
      return std::make_unique<PathIdentSegment>(tok->value);
    }

    if (tok->value == "super") return std::make_unique<PathIdentSegment>(PathIdentSegment::PathIdentSegmentType::super);
    if (tok->value == "self") return std::make_unique<PathIdentSegment>(PathIdentSegment::PathIdentSegmentType::self);
    if (tok->value == "Self") return std::make_unique<PathIdentSegment>(PathIdentSegment::PathIdentSegmentType::Self);
    if (tok->value == "crate") return std::make_unique<PathIdentSegment>(PathIdentSegment::PathIdentSegmentType::crate);
    if (tok->value == "$crate") return std::make_unique<PathIdentSegment>(PathIdentSegment::PathIdentSegmentType::$crate);

    throw std::runtime_error("Invalid token in PathIdentSegment: " + tok->value);
  }


  std::unique_ptr<TypePathSegment> parser::ParseTypePathSegment() {
    auto startTok = peek();
    if (!startTok) {
        throw std::runtime_error("Unexpected EOF while parsing TypePathSegment");
    }
    int line = startTok->line;
    int column = startTok->column;

    auto pathIdentSegment = ParsePathIdentSegment();

    auto next = peek();
    std::unique_ptr<TypePathFn> typePathFn = nullptr;

    if (next && next->value == "(") {
        typePathFn = ParseTypePathFn();
    }

    return std::make_unique<TypePathSegment>(std::move(pathIdentSegment), std::move(typePathFn));
  }

  std::unique_ptr<TypePath> parser::ParseTypePath() {
    auto startTok = peek();
    if (!startTok) { throw std::runtime_error("Unexpected EOF while parsing TypePath"); }

    std::vector<std::unique_ptr<TypePathSegment>> segments;

    if (startTok->value == "::") { get(); }

    segments.push_back(ParseTypePathSegment());

    while (true) {
      auto next = peek();
      if (!next || next->value != "::") break;
      get();
      segments.push_back(ParseTypePathSegment());
    }
    return std::make_unique<TypePath>(std::move(segments));
  }



  std::unique_ptr<InferredTypeNode> parser::ParseInferredType() {
    auto tok = get();
    if (!tok || tok->value != "_") {
      throw std::runtime_error("Expected '_' for InferredType");
    }
    return std::make_unique<InferredTypeNode>(tok->line, tok->column);
  }

  std::unique_ptr<QualifiedPathInTypeNode> parser::ParseQualifiedPathInType() {
    auto ltTok = get();
    if (!ltTok || ltTok->value != "<") {
      throw std::runtime_error("Expected '<' at start of QualifiedPathType");
    }
    int line = ltTok->line;
    int column = ltTok->column;

    auto innerType = ParseType();

    std::unique_ptr<TypePath> typePath;
    if (peek()->value == "as") {
      get();
      typePath = ParseTypePath();
    }

    auto gtTok = get();
    if (!gtTok || gtTok->value != ">") {
      throw std::runtime_error("Expected '>' to close QualifiedPathType");
    }

    std::vector<std::unique_ptr<TypePathSegment>> segments;
    while (true) {
      auto tok = peek();
      if (!tok || tok->value != "::") break;
      get();
      segments.push_back(std::move(ParseTypePathSegment()));
    }

    return std::make_unique<QualifiedPathInTypeNode>(std::move(innerType), std::move(typePath), std::move(segments), line, column);
  }

  /*
  Parse ExpressionNode
  */
  std::unique_ptr<ExpressionNode> parser::parseExpression(int ctxPrecedence) {
    auto prefixToken = get();
    if (!prefixToken) throw std::runtime_error("Expected prefix for expression");
    std::cout << "get token : " << prefixToken.value().value << std::endl;
    Token t = prefixToken.value();
    std::cout << "Parse Expression Prefix: " << t.value << std::endl;
    auto prefixIt = prefixParselets.find({t.type, t.value});
    if (prefixIt == prefixParselets.end()) {
      prefixIt = prefixParselets.find({t.type, ""});
    }

    if (prefixIt == prefixParselets.end()) {
      std::cerr << "no parselet for prefix : " << t.value << std::endl;
      throw std::runtime_error("no corrensponding prefixparselet");
    }

    auto left = prefixIt->second->parse(*this, t);

    if (auto* left_expr = dynamic_cast<PredicateLoopExpressionNode*>(left.get())) {
      return left;
    }

    if (auto* left_expr = dynamic_cast<IfExpressionNode*>(left.get())) {
      return left;
    }

    while (true) {
      auto lookahead = peek();
      if (!lookahead) break;

      Token la = lookahead.value();

      auto infixIt = infixParselets.find({la.type, la.value});
      if (infixIt == infixParselets.end()) infixIt = infixParselets.find({la.type, ""});
      if (infixIt == infixParselets.end()) break;
      auto& infixParselet = infixIt->second;
      if (infixParselet->precedence <= ctxPrecedence) break;
      get();
      left = infixParselet->parse(std::move(left), la, *this);
    }

    return left;
  }

  //ExpressionWithoutBlock → LiteralExpression | PathExpression | OperatorExpression | GroupedExpression | ArrayExpression | AwaitExpression
  //| IndexExpression | TupleExpression | TupleIndexingExpression | StructExpression | CallExpression | MethodCallExpression
  //| FieldExpression | ClosureExpression | AsyncBlockExpression | ContinueExpression | BreakExpression
  //| RangeExpression | ReturnExpression | UnderscoreExpression | MacroInvocation
  std::unique_ptr<ExpressionWithoutBlockNode> parser::parseExpressionWithoutBlock(int ctxPrecedence) {
    auto prefixToken = get();
    int line = prefixToken->line;
    int column = prefixToken->column;
    if (!prefixToken) throw std::runtime_error("Expected prefix for expression");
    std::cout << "prefix token when parsing expressionwithoutblock : " << prefixToken->value << std::endl;

    Token t = prefixToken.value();
    auto prefixIt = prefixParselets.find({t.type, t.value});
    if (prefixIt == prefixParselets.end()) {
      prefixIt = prefixParselets.find({t.type, ""});
    }
    if (prefixIt == prefixParselets.end()) throw std::runtime_error("No prefix parselet for token: " + t.value);

    auto left = prefixIt->second->parse(*this, t);

    while (true) {
      auto lookahead = peek();
      if (!lookahead) break;

      Token la = lookahead.value();

      auto infixIt = infixParselets.find({la.type, la.value});
      if (infixIt == infixParselets.end()) infixIt = infixParselets.find({la.type, ""});
      if (infixIt == infixParselets.end()) break;
      auto& infixParselet = infixIt->second;
      if (infixParselet->precedence <= ctxPrecedence) break;
      get();
      left = infixParselet->parse(std::move(left), la, *this);
    }
    
    if (!left) {
      throw std::runtime_error("Internal error: Expected ExpressionWithoutBlockNode");
    }

    ExpressionNode* raw = left.get();

    if (auto* p = dynamic_cast<LiteralExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<LiteralExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<PathExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<PathExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<OperatorExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<OperatorExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<GroupedExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<GroupedExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<ArrayExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<ArrayExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<IndexExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<IndexExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<TupleExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<TupleExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<TupleIndexingExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<TupleIndexingExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<StructExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<StructExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<CallExpressionNode*>(raw)) {
      std::cout << "getting call expression in expression without block" << std::endl;
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<CallExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<MethodCallExpressionNode*>(raw)) {
      std::cout << "getting methodcall expression in expression without block" << std::endl;
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<MethodCallExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<FieldExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<FieldExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<RangeExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<RangeExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<ReturnExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<ReturnExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<UnderscoreExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<UnderscoreExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<LazyBooleanExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<LazyBooleanExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<ArithmeticOrLogicalExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<ArithmeticOrLogicalExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<BreakExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<BreakExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<TypeCastExpressionNode*>(raw)) {
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<TypeCastExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<NegationExpressionNode*>(raw)) {
      std::cout << "get negation expression in expressionwithoutblock" << std::endl;
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<NegationExpressionNode>(p), line, column);
    }
    if (auto* p = dynamic_cast<DereferenceExpressionNode*>(raw)) {
      std::cout << "get dereference expression in expressionwithoutblock" << std::endl;
      left.release();
      return std::make_unique<ExpressionWithoutBlockNode>(std::unique_ptr<DereferenceExpressionNode>(p), line, column);
    }

    throw std::runtime_error("Internal error: parsed expression type not supported by ExpressionWithoutBlockNode");
  };

  std::unique_ptr<BlockExpressionNode> parser::parseBlockExpression() {
    auto open = get();
    if (!open.has_value() || open->value != "{") {
      throw std::runtime_error("Expected '{' to start block");
    }
    std::cout << "begin parsing block expression" << std::endl;
    std::vector<std::unique_ptr<StatementNode>> statements;
    std::unique_ptr<ExpressionWithoutBlockNode> expr = nullptr;

    auto pre_pos = get_pos();

    try {
      while (true) {
        auto next = peek();
        if (!next.has_value()) {
          throw std::runtime_error("Unexpected end of input inside block");
        }
        if (next->value == "}") break;

        auto pos_before_parsing = get_pos();

        bool if1 = true;
        bool if2 = true;

        //try parsing statement
        try {
          std::cout << "ParseStatement in BlockExpression" << std::endl;
          auto stmt = ParseStatement();
          if (stmt) {
            std::cout << "inserting statement to blockexpression. Current num : " << statements.size() << std::endl;
            statements.push_back(std::move(stmt));
            continue;
          }
        } catch (const std::exception& e) {
          std::cerr << "ParseStatement failed at line "
                    << next->line << ", col " << next->column
                    << ": " << e.what() << std::endl;
          std::cout << "roll back after failing parsing statement" << std::endl;
          roll_back(pos_before_parsing);
          if1 = false;
        }

        //try parsing expression wothout block
        try {
          expr = parseExpressionWithoutBlock();
          if (!expr) {
            throw std::runtime_error("expression parsing failed");
          } else {
            std::cout << "getting expression without block, block expression parsing finished" << std::endl;
            break;
          }
        } catch (const std::exception& e) {
          std::cerr << "parseExpressionWithoutBlock failed at line "
                    << next->line << ", col " << next->column
                    << ": " << e.what() << std::endl;
          std::cout << "roll back after failing parsing expression" << std::endl;
          roll_back(pos_before_parsing);
          if2 = false;
        }

        if (!if1 && !if2) {
          std::cerr << "error in parsing block expression: unknown thing" << std::endl;
          throw std::runtime_error("unable to parse something in blockexpression");
        }
      }
    } catch (const std::exception& e) {
      std::cerr << "[ParseBlockExpressionError] : " << e.what() << std::endl;
      throw std::runtime_error("error in parsing block expression");
    }
    auto close = get();
    if (!close.has_value() || close->value != "}") {
      throw std::runtime_error("Expected '}' to close block");
    }
    std::cout << "finish parsing blockexpression" << std::endl;
    std::cout << "next token: " << peek()->value << std::endl;
    return std::make_unique<BlockExpressionNode>(
      std::move(statements),
      std::move(expr),
      open->line, open->column
    );
  }

  std::unique_ptr<Pattern> parser::ParsePattern() {
    auto first = ParsePatternNoTopAlt();
    if (!first) throw std::runtime_error("Failed to parse first PatternNoTopAlt");

    std::vector<std::unique_ptr<PatternNoTopAlt>> alts;
    alts.push_back(std::move(first));

    while (true) {
      auto next = peek();
      if (!next || next->type != TokenType::PUNCTUATION || next->value != "|") break;
      get();

      auto alt = ParsePatternNoTopAlt();
      if (!alt) throw std::runtime_error("Failed to parse PatternNoTopAlt after '|'");
      alts.push_back(std::move(alt));
    }

    return std::make_unique<Pattern>(std::move(alts));
  }

  std::vector<std::unique_ptr<ASTNode>> parser::parse() {
    std::vector<std::unique_ptr<ASTNode>> ast;
    while (true) {
      auto tok = peek();
      if (!tok) break;

      std::unique_ptr<ASTNode> node;

      try { std::cout << "try parsing item with token: " << tok->value << std::endl; node = std::move(upcast<ASTNode>(ParseItem())); }
      catch (const std::exception& e) { 
        std::cerr << "Caught exception: " << e.what() << std::endl;
        node = nullptr; 
      }

      if (tok->value == "struct" || tok->value == "fn" || tok->value == "impl" || tok->value == "enum" || tok->value == "const") {
        if (!node) throw std::runtime_error("error in parsing item");
      }

      if (!node) {
        std::cout << "try parsing statement with token: " << tok->value << std::endl;
        try { node = std::move(upcast<ASTNode>(ParseStatement())); }
        catch (const std::exception& e) { 
          std::cerr << "Caught exception: " << e.what() << std::endl;
          node = nullptr; 
        }
      }

      if (!node) {
        std::cout << "try parsing expression with token: " << tok->value << std::endl;
        try { node = std::move(upcast<ASTNode>(parseExpression())); }
        catch (const std::exception& e) { 
          std::cerr << "Caught exception: " << e.what() << std::endl;
          node = nullptr; 
        }
      }

      if (!node) throw std::runtime_error("Cannot parse token at line " + std::to_string(tok->line));
      std::cout << "push_back ASTNode" << std::endl;
      ast.push_back(std::move(node));
    }
    std::cout << "size of ast in function parse : " << ast.size() << std::endl;
    return ast;
  }

  std::string TypePathNode::toString() const {
    if (!type_path || type_path->segments.empty()) return "<null>";

    std::string result;
    bool first = true;
    for (const auto& seg : type_path->segments) {
      if (!first) result += "::";
      if (seg->path_ident_segment) {
        if (seg->path_ident_segment->type == PathIdentSegment::ID) {
          result += seg->path_ident_segment->identifier.value_or("<null>");
        } else {
          switch (seg->path_ident_segment->type) {
            case PathIdentSegment::super: result += "super"; break;
            case PathIdentSegment::self: result += "self"; break;
            case PathIdentSegment::Self: result += "Self"; break;
            case PathIdentSegment::crate: result += "crate"; break;
            case PathIdentSegment::$crate: result += "$crate"; break;
            default: result += "<unknown>"; break;
          }
        }
      }

      if (seg->type_path_fn) {
        result += "(";
        if (seg->type_path_fn->type_path_fn_inputs) {
          const auto& types = seg->type_path_fn->type_path_fn_inputs->types;
          for (size_t i = 0; i < types.size(); ++i) {
            result += types[i] ? types[i]->toString() : "<null>";
            if (i + 1 < types.size()) result += ", ";
          }
        }
        result += ")";
        if (seg->type_path_fn->type_no_bounds) {
          result += " -> " + seg->type_path_fn->type_no_bounds->toString();
        }
      }

      first = false;
    }

    return result;
  }

#endif