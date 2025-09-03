#ifndef PARSER_HPP
#define PARSER_HPP
#include <vector>
#include <memory>
#include <string>
#include <variant>
#include <optional>
#include <unordered_set>

using ItemType = std::variant<
    std::unique_ptr<ModuleNode>,
    std::unique_ptr<FunctionNode>,
    std::unique_ptr<StructStructNode>,
    std::unique_ptr<TupleStructNode>,
    std::unique_ptr<EnumerationNode>,
    std::unique_ptr<ConstantItemNode>,
    std::unique_ptr<TraitNode>,
    std::unique_ptr<ImplementationNode>,
    std::unique_ptr<GenParaNode>,
    std::unique_ptr<AssociatedItemNode>
>;

const std::unordered_set<std::string> keywords = {
  "as", "break", "const", "continue", "crate", "else", "enum", "extern",
  "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
  "move", "mut", "pub", "ref", "return", "self", "Self", "static", "use",
  "where", "while", "struct", "super", "trait", "true", "type", "unsafe",
  "async", "await", "dyn",
  "abstract", "become", "box", "do", "final", "macro", "override", "priv",
  "typeof", "unsized", "virtual", "yield", "try", "gen",
  "'static", "macro_rules", "raw", "safe", "union"
};

const std::unordered_set<std::string> IntSuffixes = {"i8", "i16", "i32", "i64", "i128", "isize"};
const std::unordered_set<std::string> UintSuffixes = {"u8", "u16", "u32", "u64", "u128", "usize"};
const std::unordered_set<std::string> FloatSuffixes = {"f32", "f64"};

//节点的类型
enum NodeType {
  NullStatement, OuterAttribute, Module, InnerAttribute, Function, Struct, Enumeration, ConstantItem, Trait, Implementation, GenPara, AssociatedItem, StructStruct,
  TupleStruct, NodeType_StructField, NodeType_TupleField, EnumVariant, EnumVariants, EnumVariantTuple, EnumVariantStruct, EnumVariantDiscriminant, LiteralExpression, BlockExpression,
  ExpressionWithoutBlock, OperatorExpression, BorrowExpression, DereferenceExpression, NegationExpression, ArithmeticOrLogicalExpression, ComparisonExpression, LazyBooleanExpression,
  TypeCastExpression, AssignmentExpression, CompoundAssignmentExpression, GroupedExpression, ArrayExpression, IndexExpression, TupleExpression, TupeIndexingExpression, StructExpression,
  CallExpression, MethodCallExpression, FieldExpression, InfiniteLoopExpression, PredicateLoopExpression, LoopExpression, IfExpression, MatchExpression, ReturnExpression, UnderscoreExpression,
  ParenthesizedType, TypePath_node, TupleType, NeverType, ArrayType, SliceType, InferredType, QualifiedPathInType
};

//rust中的数据类型
enum Type {
};

//运算类型
enum OperationType {
  ADD, MINUS, MUL, DIV, MOD, AND, OR, XOR, SHL, SHR
};

//比较类型
enum ComparisonType {
  EQ, NEQ, GT, LT, GEQ, LEQ
};

//Token的类型
enum class TokenType {
  Identifier,
  Keyword,
  CharLiteral,
  StringLiteral,
};

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
  virtual void print(int indent = 0) const = 0;
};

/*
class of expression
*/
class ExpressionNode : ASTNode {
 public:
  ExpressionNode(NodeType t, int l, int c) : ASTNode(t, l, c) {};

  NodeType get_type() { return type; }

  virtual ~ExpressionNode() = default;
};

/*
class of items
*/
class ItemNode : ASTNode {
 public:
  ItemNode(NodeType t, int l, int c) : ASTNode(t, l, c) {};
};

/*
class of types
*/
class TypeNode : ASTNode {
 public:
  TypeNode(NodeType t, int l, int c) : ASTNode(t, l, c) {};
};

//ParenthesizedType → ( Type )
class ParenthesizedTypeNode : TypeNode {
 public:
  std::unique_ptr<TypeNode> type;

  ParenthesizedTypeNode(std::unique_ptr<TypeNode> t, int l, int c) : type(std::move(t)), TypeNode(NodeType::ParenthesizedType, l, c) {};
};  

//TypePath → ::? TypePathSegment ( :: TypePathSegment )*
class TypePathNode : TypeNode {
 public:
  std::unique_ptr<TypePath> type_path;

  TypePathNode(std::unique_ptr<TypePath> tp, int l, int c) : type_path(std::move(tp)), TypeNode(NodeType::TypePath_node, l, c) {};
};

//TupleType → ( ) | ( ( Type , )+ Type? )
class TupleTypeNode : TypeNode {
 public:
  std::vector<std::unique_ptr<TypeNode>> types;

  TupleTypeNode(std::vector<std::unique_ptr<TypeNode>> t, int l, int c) : types(std::move(t)), TypeNode(NodeType::TupleType, l, c) {};
};  

//NeverType → !
class NeverTypeNode : TypeNode { 
 public:
  NeverTypeNode(int l, int c) : TypeNode(NodeType::NeverType, l, c) {};
};

//ArrayType → [ Type ; Expression ]
class ArrayTypeNode : TypeNode {
 public:
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<ExpressionNode> expression;

  ArrayTypeNode(std::unique_ptr<TypeNode> t, std::unique_ptr<ExpressionNode> e, int l, int c) 
              : type(std::move(t)), expression(std::move(e)), TypeNode(NodeType::ArrayType, l, c) {};
};

//SliceType → [ Type ]
class SliceTypeNode : TypeNode {
 public:
  std::unique_ptr<TypeNode> type;

  SliceTypeNode(std::unique_ptr<TypeNode> t, int l, int c) : type(std::move(t)), TypeNode(NodeType::SliceType, l, c) {};
};

//InferredType → _
class InferredTypeNode : TypeNode { 
 public:
  InferredTypeNode(int l, int c) : TypeNode(NodeType::InferredType, l, c) {};
};

//QualifiedPathInType → QualifiedPathType ( :: TypePathSegment )+
//QualifiedPathType → < Type ( as TypePath )? >
class QualifiedPathInTypeNode : TypeNode {
 public:
  std::unique_ptr<TypeNode> type;
  std::unique_ptr<TypePath> type_path;
  std::vector<std::unique_ptr<TypePathSegment>> type_path_segments;

  QualifiedPathInTypeNode(std::unique_ptr<TypeNode> t, std::unique_ptr<TypePath> tp, std::vector<std::unique_ptr<TypePathSegment>> tps, int l, int c)
                        : type(std::move(t)), type_path(std::move(tp)), type_path_segments(std::move(tps)), TypeNode(NodeType::QualifiedPathInType, l, c) {};
};

/*
null statement
*/
class NullStatementNode : ASTNode {
  NullStatementNode(int l, int c) : ASTNode(NodeType::NullStatement, l, c) {}
};

/*
classes of attributes
actually they are not nodes, but to distinguish them from type, they pretend to be nodes
*/
class OuterAttributeNode : ASTNode {
 public:
  std::string attr;
  OuterAttributeNode(int l, int c) : ASTNode(NodeType::OuterAttribute, l, c) {}
};

class InnerAttributeNode : ASTNode {
  std::string attr;

  InnerAttributeNode(std::string a, int l, int c) : ASTNode(NodeType::InnerAttribute, l, c) {
    attr = a;
  }
};

/*
1.  mod ID ;
2.  mod ID { InnerAttributes, items }
*/
class ModuleNode : ItemNode {
 public:
  std::string id;
  bool isDeclaration;

  std::vector<InnerAttributeNode> InnerAttributes;
  std::vector<ItemType> items;

  ModuleNode(std::string i, int l, int c) : ItemNode(NodeType::Module, l ,c) {
    id = i;
  }; //声明式module

  ModuleNode(std::string , int l, int c, std::vector<InnerAttributeNode> InnerAttribute, std::vector<ItemType> item)
            : ItemNode(NodeType::Module, l, c), InnerAttributes(std::move(InnerAttribute)), items(std::move(item)) {};
};

/*
structs used in function node
*/
//const? async?​ ItemSafety?​ ( extern Abi? )?
struct FunctionQualifier {
  bool is_const = false;
  bool is_async = false;
  enum class Safety { None, Safe, Unsafe };
  Safety safety = Safety::None;
  bool has_extern = false;        
  std::optional<std::string> abi = std::nullopt; 

  FunctionQualifier() = default;

  FunctionQualifier(bool c, bool a, Safety s, bool e, std::optional<std::string> abi_str)
      : is_const(c), is_async(a), safety(s), has_extern(e), abi(std::move(abi_str)) {}
};

//( & | & Lifetime )? mut? self
struct ShorthandSelf {
  bool if_prefix = false; //是否有( & | & Lifetime )
  std::optional<Lifetime> lifetime;
  bool if_mut = false;

  ShorthandSelf(std::string s, bool im) : if_prefix(true), lifetime(Lifetime(s)), if_mut(im) {};
};

//mut? self : Type
struct TypedSelf {
  bool if_mut = false;
  std::unique_ptr<TypeNode> type;

  TypedSelf(bool im, std::unique_ptr<TypeNode> t) : if_mut(im), type(std::move(t)) {};
};

//OuterAttribute* ( ShorthandSelf | TypedSelf )
struct SelfParam {
  std::vector<OuterAttributeNode> outer_attributes;
  std::variant<std::unique_ptr<ShorthandSelf>, std::unique_ptr<TypedSelf>> self;

  SelfParam(std::vector<OuterAttributeNode> oa, std::unique_ptr<ShorthandSelf> s) : outer_attributes(std::move(oa)), self(std::move(s)) {};
  SelfParam(std::vector<OuterAttributeNode> oa, std::unique_ptr<TypedSelf> s) : outer_attributes(std::move(oa)), self(std::move(s)) {};
};

//...
struct ellipsis {
  std::string ellip = "...";

  ellipsis() = default;
};

//FunctionParamPattern → PatternNoTopAlt : ( Type | ... )
struct FunctionParamPattern {
 public:
  std::unique_ptr<PatternNoTopAlt> pattern;
  std::unique_ptr<TypeNode> type;

  FunctionParamPattern(std::unique_ptr<PatternNoTopAlt> p, std::unique_ptr<TypeNode> t) : pattern(std::move(p)), type(std::move(t)) {};
};

//OuterAttribute* ( FunctionParamPattern | ... | Type​4 )
struct FunctionParam {
  std::vector<OuterAttributeNode> outer_attributes;
  std::variant<std::unique_ptr<FunctionParamPattern>, std::unique_ptr<ellipsis>, std::unique_ptr<TypeNode>> info;

  FunctionParam(std::vector<OuterAttributeNode> oa) : outer_attributes(std::move(oa)), info(std::make_unique<ellipsis>(ellipsis())) {};
  FunctionParam(std::vector<OuterAttributeNode> oa, std::unique_ptr<FunctionParamPattern> fpp) : outer_attributes(std::move(oa)), info(std::move(fpp)) {};
  FunctionParam(std::vector<OuterAttributeNode> oa, std::unique_ptr<TypeNode> t) : outer_attributes(std::move(oa)), info(std::move(t)) {};
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
  std::vector<FunctionParam> function_params;

  bool check() {
    if (type == 1) {
      if (self_param == nullptr || function_params.size() != 0) return false;
      else return true;
    } else if (type == 2) {
      if (function_params.size() < 1) return false;
      else return true;
    } else return false;
  }

  FunctionParameter() = default;
  FunctionParameter(int t, std::vector<FunctionParam> fp) : type(t), function_params(std::move(fp)) {};
  FunctionParameter(int t, std::unique_ptr<SelfParam> sp, std::vector<FunctionParam> fp) : type(t), self_param(std::move(sp)), function_params(std::move(fp)) {};
};

//WhereClause → where ( WhereClauseItem , )* WhereClauseItem?
struct WhereClause {

};

//{ InnerAttribute* Statements? }
struct BlockExpression {
  std::vector<InnerAttributeNode> inner_attribute;
  std::vector<std::unique_ptr<StatementNode>> statements;

  BlockExpression(std::vector<InnerAttributeNode> inner_attr, std::vector<std::unique_ptr<StatementNode>> stmts)
                  : inner_attribute(std::move(inner_attr)), statements(std::move(stmts)) {};
};

/*
FunctionQualifier fn identifier GenericParams? ( FunctionParameters? ) FunctionReturnType? WhereClause? ( BlockExpression | ; )
*/
class FunctionNode : public ItemNode {
 public:
  FunctionQualifier function_qualifier;
  std::string identifier;
  std::unique_ptr<GenParaNode> generic_params = nullptr;
  FunctionParameter function_parameter;
  std::optional<FunctionReturnType> return_type = std::nullopt;
  std::optional<WhereClause> where_clause = std::nullopt;
  std::unique_ptr<BlockExpressionNode> block_expression = nullptr;

  FunctionNode(FunctionQualifier fq, std::string id, int l, int c) : function_qualifier(fq), identifier(id), ItemNode(NodeType::Function, l, c) {};
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

//StructField → OuterAttribute* Visibility? IDENTIFIER : Type
struct StructField {
  Visibility visibility;
  std::string identifier;
  std::unique_ptr<TypeNode> type;

  StructField(Visibility vis, std::string id, std::unique_ptr<TypeNode> t) : visibility(vis), identifier(id), type(std::move(t)) {};
};

//StructFields → StructField ( , StructField )* ,?
class StructFieldNode : public ASTNode {
 public:
  std::vector<std::unique_ptr<StructField>> struct_fields;

  StructFieldNode(std::vector<std::unique_ptr<StructField>> sf, int l, int c) : struct_fields(std::move(sf)), ASTNode(NodeType::NodeType_StructField, l, c) {};
};

//TupleField → OuterAttribute* Visibility? Type
struct TupleField {
  std::optional<Visibility> visibility = std::nullopt;
  std::unique_ptr<TypeNode> type;

  TupleField(Visibility vis, std::unique_ptr<TypeNode> t) : visibility(vis), type(std::move(t)) {};
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
  std::unique_ptr<GenParaNode> generic_param;
  std::unique_ptr<WhereClause> where_clause;
  std::unique_ptr<StructFieldNode> struct_fields;

  StructStructNode(std::string id, int l, int c) : identifier(id), ItemNode(NodeType::StructStruct, l, c) {};
};
class TupleStructNode : public ItemNode {
 public:
  std::string identifier;
  std::unique_ptr<GenParaNode> generic_param;
  std::unique_ptr<WhereClause> where_clause;
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

//EnumVairant:EnumVariant → OuterAttribute* Visibility? IDENTIFIER ( EnumVariantTuple | EnumVariantStruct )? EnumVariantDiscriminant?
class EnumVariantNode : public ASTNode {
 public:
  std::unique_ptr<Visibility> visibility;
  std::string identifier;
  std::optional<std::variant<EnumVariantTupleNode, EnumVariantStructNode>> tuple_or_struct_node = std::nullopt;
  std::unique_ptr<EnumVariantDiscriminantNode> discriminant;

  EnumVariantNode(std::string s, int l, int c) : identifier(s), ASTNode(NodeType::EnumVariant, l, c) {};
};

//EnumVariants → EnumVariant ( , EnumVariant )* ,?
class EnumVariantsNode : public ASTNode {
 public:
  std::vector<std::unique_ptr<EnumVariantNode>> enum_variants;

  EnumVariantsNode(int l, int c) : ASTNode(NodeType::EnumVariants, l, c) {};
  
  EnumVariantsNode(std::vector<std::unique_ptr<EnumVariantNode>> variants, int l, int c)
      : enum_variants(std::move(variants)), ASTNode(NodeType::EnumVariants, l, c) {}
};

//Enumeration → enum IDENTIFIER GenericParams? WhereClause? { EnumVariants? }
class EnumerationNode : public ItemNode {
 public:
  std::string identifier;
  std::unique_ptr<GenParaNode> generic_params;
  std::unique_ptr<WhereClause> where_clause;
  std::unique_ptr<EnumVariantsNode> enum_variants;

  EnumerationNode(std::string id, int l, int c) : identifier(id), ItemNode(NodeType::Enumeration, l, c) {};
};

class ConstantItemNode : public ItemNode {
  enum ConstantType {
    ID, _
  };
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
};

//TypePathFnInputs → Type ( , Type )* ,?
class TypePathFnInputs {
 public:
  std::vector<std::unique_ptr<TypeNode>> types;

  TypePathFnInputs(std::vector<std::unique_ptr<TypeNode>> t) : types(std::move(t)) {};
};

//TypePathFn → ( TypePathFnInputs? ) ( -> TypeNoBounds )?
class TypePathFn {
 public:
  std::unique_ptr<TypePathFnInputs> type_path_fn_inputs;

  TypePathFn(std::unique_ptr<TypePathFnInputs> tpfi) : type_path_fn_inputs(std::move(tpfi)) {};
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
  PathIdentSegment path_ident_segment;
  
};

//TypePath → ::? TypePathSegment ( :: TypePathSegment )*
class TypePath {

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
  Visibility visibility;
  std::variant<std::unique_ptr<ConstantItemNode>, std::unique_ptr<FunctionNode>> associated_item;

  AssociatedItemNode(Visibility vis, std::unique_ptr<ConstantItemNode> con, int l, int c) : visibility(vis), associated_item(std::move(con)), ItemNode(NodeType::AssociatedItem, l, c) {};
  AssociatedItemNode(Visibility vis, std::unique_ptr<FunctionNode> con, int l, int c) : visibility(vis), associated_item(std::move(con)), ItemNode(NodeType::AssociatedItem, l, c) {};
};

//Trait → unsafe? trait IDENTIFIER GenericParams? ( : TypeParamBounds? )? WhereClause? { InnerAttribute*   AssociatedItem* }
class TraitNode : public ItemNode {
 public:
  bool is_safe = false;
  std::string identifier;
  std::vector<std::unique_ptr<GenParaNode>> generic_params;
  std::unique_ptr<AssociatedItemNode> associated_item;

  TraitNode(bool safe, std::string id, std::vector<std::unique_ptr<GenParaNode>> gp, std::unique_ptr<AssociatedItemNode> a, int l, int c)
          : is_safe(safe), generic_params(std::move(gp)), associated_item(std::move(a)), ItemNode(NodeType::Trait, l, c) {};
};

/*
class for Implementation node
*/

//InherentImpl → impl GenericParams? Type WhereClause? {  InnerAttribute*  AssociatedItem*}

//Implementation → InherentImpl | TraitImpl
class ImplementationNode : public ItemNode {
 public:
  std::vector<std::unique_ptr<GenParaNode>> generic_params;
  std::unique_ptr<TypeNode> type;
  WhereClause where_clause;
  std::unique_ptr<AssociatedItemNode> associated_item;

  ImplementationNode(std::vector<std::unique_ptr<GenParaNode>> gp, std::unique_ptr<TypeNode> t, WhereClause wc, std::unique_ptr<AssociatedItemNode> ai, int l, int c) 
                    : generic_params(std::move(gp)), type(std::move(t)), where_clause(wc), associated_item(std::move(ai)), ItemNode(NodeType::Implementation, l, c) {};
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

//LetStatement → OuterAttribute* let PatternNoTopAlt ( : Type )? ( = Expression | = Expressionexcept LazyBooleanExpression or end with a } else BlockExpression)? ;
class LetStatement {

};

//ExpressionStatement → ExpressionWithoutBlock ; | ExpressionWithBlock ;?
class ExpressionStatement {
 public:
  std::unique_ptr<ExpressionNode> expression;

  ExpressionStatement(std::unique_ptr<ExpressionNode> e) : expression(std::move(e)) {};
};

//Statement →  ; | Item  | LetStatement | ExpressionStatement
class StatementNode : ASTNode {
 public:
  enum StatementType {
    SEMICOLON, ITEM, LETSTATEMENT, EXPRESSIONSTATEMENT
  };
  StatementType type;
  std::optional<std::unique_ptr<ItemNode>> item = std::nullopt; //item
  std::optional<std::unique_ptr<LetStatement>> let_statement = std::nullopt; //letstatement
  std::optional<std::unique_ptr<ExpressionStatement>> expr_statement = std::nullopt; //ExpressionStatement
  bool check() {
    switch (type) {
      case(SEMICOLON) : {
        if (item == std::nullopt && let_statement == std::nullopt && expr_statement == std::nullopt) {
          return true;
        }
        return false;
      }
      case(ITEM) : {
        if (item != std::nullopt && let_statement == std::nullopt && expr_statement == std::nullopt) {
          return true;
        }
        return false;
      }
      case(LETSTATEMENT) : {
        if (item == std::nullopt && let_statement != std::nullopt && expr_statement == std::nullopt) {
          return true;
        }
        return false;
      }
      case(EXPRESSIONSTATEMENT) : {
        if (item == std::nullopt && let_statement == std::nullopt && expr_statement != std::nullopt) {
          return true;
        }
        return false;
      }
      default: return false;
    }
  };
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

//LiteralExpression → CHAR_LITERAL | STRING_LITERAL | RAW_STRING_LITERAL | C_STRING_LITERAL | RAW_C_STRING_LITERAL | INTEGER_LITERAL | FLOAT_LITERAL | true | false
class LiteralExpressionNode : ExpressionNode {
 public:
  std::variant<std::unique_ptr<char_literal>, std::unique_ptr<string_literal>, std::unique_ptr<raw_string_literal>, std::unique_ptr<c_string_literal>, 
               std::unique_ptr<raw_c_string_literal>, std::unique_ptr<integer_literal>, std::unique_ptr<float_literal>, std::unique_ptr<bool>> literal;
  template <typename T>
  LiteralExpressionNode(std::unique_ptr<T> lit, int l, int c) : literal(std::move(lit)), ExpressionNode(NodeType::LiteralExpression, l, c) {}
};

//ExpressionWithoutBlock → LiteralExpression | PathExpression | OperatorExpression | GroupedExpression | ArrayExpression | AwaitExpression
//                        | IndexExpression | TupleExpression | TupleIndexingExpression | StructExpression | CallExpression | MethodCallExpression
//                        | FieldExpression | ClosureExpression | AsyncBlockExpression | ContinueExpression | BreakExpression
//                        | RangeExpression | ReturnExpression | UnderscoreExpression | MacroInvocation
class ExpressionWithoutBlockNode : ExpressionNode {

};

//BlockExpression → { InnerAttribute*  Statements? }
//Statements → Statement+ | Statement+ ExpressionWithoutBlock | ExpressionWithoutBlock
class BlockExpressionNode : ExpressionNode {
 public:
  bool if_empty = false;
  std::vector<std::unique_ptr<StatementNode>> statement;
  std::optional<std::unique_ptr<ExpressionWithoutBlockNode>> expression_without_block = std::nullopt;

  BlockExpressionNode(int l, int c) : if_empty(true), ExpressionNode(NodeType::BlockExpression, l, c) {};
  BlockExpressionNode(std::vector<std::unique_ptr<StatementNode>> s, std::optional<std::unique_ptr<ExpressionWithoutBlockNode>> e, int l, int c)
                    : statement(std::move(s)), expression_without_block(std::move(e)), ExpressionNode(NodeType::BlockExpression, l, c) {};
};

//OperatorExpression → BorrowExpression | DereferenceExpression | NegationExpression | ArithmeticOrLogicalExpression | ComparisonExpression 
//                    | LazyBooleanExpression | TypeCastExpression | AssignmentExpression | CompoundAssignmentExpression

class OperatorExpressionNode : ExpressionNode {
 public:
  std::variant<std::unique_ptr<BorrowExpressionNode>, std::unique_ptr<DereferenceExpressionNode>, std::unique_ptr<NegationExpressionNode>, std::unique_ptr<ArithmeticOrLogicalExpressionNode>, 
              std::unique_ptr<ComparisonExpressionNode>, std::unique_ptr<LazyBooleanExpressionNode>, std::unique_ptr<TypeCastExpressionNode>, std::unique_ptr<AssignmentExpressionNode>, 
              std::unique_ptr<CompoundAssignmentExpressionNode>> operator_expression;

  template <typename T>
  OperatorExpressionNode(T expr, int l, int c) : operator_expression(std::move(expr)), ExpressionNode(NodeType::OperatorExpression, l, c) {};
};

//BorrowExpression → ( & | && ) Expression | ( & | && ) mut Expression | ( & | && ) raw const Expression | ( & | && ) raw mut Expression
class BorrowExpressionNode : ExpressionNode {
 public:
  int AmpersandCount = 0; //1 or 2
  bool if_mut = false; //有无mut
  bool if_const = false;
  bool if_raw = false;
  std::unique_ptr<ExpressionNode> expression;

  BorrowExpressionNode(int count, bool im, bool ic, bool ir, std::unique_ptr<ExpressionNode> expr, int l, int c) 
                      : AmpersandCount(count), if_mut(im), if_const(ic), if_raw(ir), expression(std::move(expr)), ExpressionNode(NodeType::BorrowExpression, l, c) {};
};

//DereferenceExpression → * Expression
class DereferenceExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;

  DereferenceExpressionNode(std::unique_ptr<ExpressionNode> expr, int l, int c) 
                          : expression(std::move(expr)), ExpressionNode(NodeType::DereferenceExpression, l, c) {};
};

//NegationExpression → - Expression | ! Expression
class NegationExpressionNode : ExpressionNode {
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
class ArithmeticOrLogicalExpressionNode : ExpressionNode {
 public:
  OperationType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  ArithmeticOrLogicalExpressionNode(OperationType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::ArithmeticOrLogicalExpression, l, c) {};
};

//ComparisonExpression → Expression == Expression | Expression != Expression | Expression > Expression | Expression < Expression | Expression >= Expression | Expression <= Expression
class ComparisonExpressionNode : ExpressionNode {
 public:
  ComparisonType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  ComparisonExpressionNode(ComparisonType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::ArithmeticOrLogicalExpression, l, c) {};
};

//LazyBooleanExpression → Expression || Expression | Expression && Expression
class LazyBooleanExpressionNode : ExpressionNode {
 public:
  enum LazyBooleanType {
    OR, AND
  };
  LazyBooleanType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  LazyBooleanExpressionNode(LazyBooleanType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::LazyBooleanExpression, l, c) {};
};

//TypeCastExpression → Expression as TypeNoBounds
class TypeCastExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;
  std::unique_ptr<TypeNode> type;

  TypeCastExpressionNode(std::unique_ptr<ExpressionNode> expr, std::unique_ptr<TypeNode> t, int l, int c) 
                      : expression(std::move(expr)), type(std::move(t)), ExpressionNode(NodeType::TypeCastExpression, l, c) {};
};

//AssignmentExpression → Expression = Expression
class AssignmentExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression1, expression2;

  AssignmentExpressionNode(std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                                  : expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::AssignmentExpression, l, c) {};
};

//CompoundAssignmentExpression → Expression += Expression | Expression -= Expression | Expression *= Expression | Expression /= Expression | Expression %= Expression
//                              | Expression &= Expression | Expression |= Expression | Expression ^= Expression | Expression <<= Expression | Expression >>= Expression
class CompoundAssignmentExpressionNode : ExpressionNode {
 public: 
  OperationType type;
  std::unique_ptr<ExpressionNode> expression1, expression2;

  CompoundAssignmentExpressionNode(OperationType t, std::unique_ptr<ExpressionNode> expr1, std::unique_ptr<ExpressionNode> expr2, int l, int c)
                        : type(t), expression1(std::move(expr1)), expression2(std::move(expr2)), ExpressionNode(NodeType::CompoundAssignmentExpression, l, c) {};
};

//GroupedExpression → ( Expression )
class GroupedExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;

  GroupedExpressionNode(std::unique_ptr<ExpressionNode> expr, int l, int c) : expression(std::move(expr)), ExpressionNode(NodeType::GroupedExpression, l, c) {};
};

//ArrayExpression → [ ArrayElements? ]
//ArrayElements → Expression ( , Expression )* ,? | Expression ; Expression
class ArrayExpressionNode : ExpressionNode {
 public:
  bool if_empty = true;
  enum ArrayExpressionType {
    LITERAL, REPEAT
  };
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
};

class PathExpressionNode : ExpressionNode {
 public:
  std::variant<std::unique_ptr<PathInExpression>, std::unique_ptr<QualifiedPathInExpression>> path;

  template<typename T>
  PathExpressionNode(T p) : path(std::move(p)) {};
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
    if (struct_base->check() == false) return false;
    return struct_expr_fields.size() >= 1 ;
  }
};

//StructExpression → PathInExpression { ( StructExprFields | StructBase )? }
class StructExpressionNode : ExpressionNode {
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

//CallExpression → Expression ( CallParams? )
class CallExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;
  std::unique_ptr<CallParams> call_params = nullptr;

  CallExpressionNode(std::unique_ptr<ExpressionNode> expr, std::unique_ptr<CallParams> cp, int l, int c) 
                  : expression(std::move(expr)), call_params(std::move(cp)), ExpressionNode(NodeType::CallExpression, l, c) {};
};

//CallParams → Expression ( , Expression )* ,?
class CallParams {
 public:
  std::vector<std::unique_ptr<ExpressionNode>> expressions;

  CallParams(std::vector<std::unique_ptr<ExpressionNode>> expr) : expressions(std::move(expr)) {};
};

//MethodCallExpression → Expression . PathExprSegment ( CallParams? )
class MethodCallExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression = nullptr;
  std::variant<PathInType, Identifier> path_expr_segment;
  std::unique_ptr<CallParams> call_params;

  template<typename T>
  MethodCallExpressionNode(std::unique_ptr<ExpressionNode> expr, T pes, std::unique_ptr<CallParams> cp, int l, int c) 
                        : expression(std::move(expr)), path_expr_segment(pes), call_params(std::move(cp)), ExpressionNode(NodeType::MethodCallExpression, l, c) {};
};

//FieldExpression → Expression . IDENTIFIER
class FieldExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression = nullptr;
  Identifier identifier;

  FieldExpressionNode(std::unique_ptr<ExpressionNode> expr, Identifier id, int l , int c) : expression(std::move(expr)), identifier(id), ExpressionNode(NodeType::FieldExpression, l, c) {};
};


class LiteralPattern {
 public:
  bool if_minus = false;
  std::unique_ptr<LiteralExpressionNode> literal = nullptr;

  LiteralPattern(bool im, std::unique_ptr<LiteralExpressionNode> l) : if_minus(im), literal(std::move(l)) {};
};

class IdentifierPattern {
 public:
  bool if_ref = false;
  bool if_mut = false;
  Identifier identifier;
  std::unique_ptr<PatternNoTopAlt> pattern_no_top_alt;

  IdentifierPattern(bool ir, bool im, Identifier id, std::unique_ptr<PatternNoTopAlt> pnta) : if_ref(ir), if_mut(im), pattern_no_top_alt(std::move(pnta)), identifier(id) {};
};

class WildCardPattern;
class RestPattern;

//ReferencePattern → ( & | && ) mut? PatternWithoutRange
class ReferencePattern {
 public:
  int and_count = 0; //1 or 2
  bool if_mut = false;
  std::unique_ptr<PatternWithoutRange> pattern_without_range;

  ReferencePattern(int ac, bool im, std::unique_ptr<PatternWithoutRange> pwr) : and_count(ac), if_mut(im), pattern_without_range(std::move(pwr)) {};
};

//StructPattern → PathInExpression { StructPatternElements? }

//StructPatternElements → StructPatternFields ( , | , StructPatternEtCetera )? | StructPatternEtCetera

//StructPatternFields → StructPatternField ( , StructPatternField )*

//StructPatternField → ( TUPLE_INDEX : Pattern | IDENTIFIER : Pattern | ref? mut? IDENTIFIER )

//StructPatternEtCetera → ..

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
};

//TupleStructPattern → PathInExpression ( TupleStructItems? )

//TupleStructItems → Pattern ( , Pattern )* ,?
class TupleStructPattern {
 public:
  std::unique_ptr<PathInExpression> path;
  std::vector<std::unique_ptr<Pattern>> patterns;

  TupleStructPattern(std::unique_ptr<PathInExpression> p, std::vector<std::unique_ptr<Pattern>> ps) : path(std::move(p)), patterns(std::move(ps)) {};
};

//TuplePattern → ( TuplePatternItems? )

//TuplePatternItems → Pattern , | RestPattern | Pattern ( , Pattern )+ ,?
class TuplePattern {
 public:
  std::vector<std::unique_ptr<Pattern>> patterns;
  bool if_rest = false;

  TuplePattern(std::vector<std::unique_ptr<Pattern>> p, bool ir) : patterns(std::move(p)), if_rest(ir) {};

  bool check() {
    return !if_rest && patterns.size() > 0 || if_rest && patterns.size() == 0;
  }
};

//GroupedPattern → ( Pattern )
class GroupedPattern {
 public:
  std::unique_ptr<Pattern> pattern;

  GroupedPattern(std::unique_ptr<Pattern> p) : pattern(std::move(p)) {};
};

//SlicePattern → [ SlicePatternItems? ]

//SlicePatternItems → Pattern ( , Pattern )* ,?
class SlicePattern {
 public:
  std::vector<std::unique_ptr<Pattern>> patterns;

  SlicePattern(std::vector<std::unique_ptr<Pattern>> p) : patterns(std::move(p)) {};
};

//PathPattern → PathExpression
class PathPattern {
 public:
  std::unique_ptr<PathExpressionNode> path;

  PathPattern(std::unique_ptr<PathExpressionNode> p) : path(std::move(p)) {};
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
};

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
};  

class RangeExclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> start;
  std::unique_ptr<RangePatternBound> end;

  RangeExclusivePattern(std::unique_ptr<RangePatternBound> s, std::unique_ptr<RangePatternBound> e) : start(std::move(s)), end(std::move(e)) {};
};

class RangeInclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> start;
  std::unique_ptr<RangePatternBound> end;

  RangeInclusivePattern(std::unique_ptr<RangePatternBound> s, std::unique_ptr<RangePatternBound> e) : start(std::move(s)), end(std::move(e)) {}
};

class RangeFromPattern {
 public:
  std::unique_ptr<RangePatternBound> range_pattern_bound;

  RangeFromPattern(std::unique_ptr<RangePatternBound> s) : range_pattern_bound(std::move(s)) {}
};

class RangeToExclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> range_pattern_bound;

  explicit RangeToExclusivePattern(std::unique_ptr<RangePatternBound> e) : range_pattern_bound(std::move(e)) {}
};

class RangeToInclusivePattern {
 public:
  std::unique_ptr<RangePatternBound> range_pattern_bound;

  RangeToInclusivePattern(std::unique_ptr<RangePatternBound> e) : range_pattern_bound(std::move(e)) {}
};

class ObsoleteRangePattern {
 public:
  std::unique_ptr<RangePatternBound> start;
  std::unique_ptr<RangePatternBound> end;

  ObsoleteRangePattern(std::unique_ptr<RangePatternBound> s, std::unique_ptr<RangePatternBound> e) : start(std::move(s)), end(std::move(e)) {}
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
};

//PatternNoTopAlt → PatternWithoutRange | RangePattern
class PatternNoTopAlt {
 public:
  std::variant<std::unique_ptr<PatternWithoutRange>, std::unique_ptr<RangePattern>> pattern;

  template<typename T> 
  PatternNoTopAlt(T p) : pattern(std::move(p)) {};
};


//Pattern → |? PatternNoTopAlt ( | PatternNoTopAlt )*
class Pattern {
 public:
  std::vector<std::unique_ptr<PatternNoTopAlt>> patterns;

  Pattern(std::vector<std::unique_ptr<PatternNoTopAlt>> p) : patterns(std::move(p)) {};
};

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
};

//InfiniteLoopExpression → loop BlockExpression
class InfiniteLoopExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<BlockExpressionNode> block_expression;

  InfiniteLoopExpressionNode(std::unique_ptr<BlockExpressionNode> be, int l, int c) : block_expression(std::move(be)), ExpressionNode(NodeType::InfiniteLoopExpression, l, c) {};
};

//PredicateLoopExpression → while Conditions BlockExpression
class PredicateLoopExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<Conditions> conditions;
  std::unique_ptr<BlockExpressionNode> block_expression;

  PredicateLoopExpressionNode(std::unique_ptr<Conditions> con, std::unique_ptr<BlockExpressionNode> be, int l, int c) 
                            : conditions(std::move(con)), block_expression(std::move(be)), ExpressionNode(NodeType::PredicateLoopExpression, l, c) {};
};

//LoopExpression → InfiniteLoopExpression | PredicateLoopExpression
class LoopExpression : ExpressionNode {
 public:
  std::variant<std::unique_ptr<InfiniteLoopExpressionNode>, std::unique_ptr<PredicateLoopExpressionNode>> loop_expression;

  template<typename T>
  LoopExpression(T le, int l, int c) : loop_experssion(std::move(le)), ExpressionNode(NodeType::LoopExpression, l, c) {};
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
class RangeFullExpr;

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
class RangeExpressionNode {
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
  RangeExpressionNode(T v) : value(std::move(v)) {}
};

//IfExpression → if Conditions BlockExpression ( else ( BlockExpression | IfExpression ) )?
class IfExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<Conditions> conditions;
  std::unique_ptr<BlockExpressionNode> block_expression;
  std::unique_ptr<BlockExpressionNode> else_block;
  std::unique_ptr<IfExpressionNode> else_if;

  IfExpressionNode(std::unique_ptr<Conditions> con, std::unique_ptr<BlockExpressionNode> be, std::unique_ptr<BlockExpressionNode> eb, std::unique_ptr<IfExpressionNode> ei, int l, int c)
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

class MatchExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> scrutinee;
  std::unique_ptr<MatchArms> match_arms;

  MatchExpressionNode(std::unique_ptr<ExpressionNode> expr, std::unique_ptr<MatchArms> ma, int l, int c)
                  : scrutinee(std::move(expr)), match_arms(std::move(ma)), ExpressionNode(NodeType::MatchExpression, l, c) {};
};

//ReturnExpression → return Expression?
class ReturnExpressionNode : ExpressionNode {
 public:
  std::unique_ptr<ExpressionNode> expression;

  ReturnExpressionNode(std::unique_ptr<ExpressionNode> expr, int l, int c) : expression(std::move(expr)), ExpressionNode(NodeType::ReturnExpression, l, c) {};
};  

//UnderscoreExpression → _
class UnderscoreExpressionNode : ExpressionNode {
 public:
  UnderscoreExpressionNode(int l, int c) : ExpressionNode(NodeType::UnderscoreExpression, l, c) {};
};
#endif