#ifndef PARSER_HPP
#define PARSER_HPP
#include <vector>
#include <memory>
#include <string>
#include <variant>
#include <optional>

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

//节点的类型
enum NodeType {
  NullStatement, OuterAttribute, Module, InnerAttribute, Function, Struct, Enumeration, ConstantItem, Trait, Implementation, GenPara, AssociatedItem, StructStruct,
  TupleStruct, NodeType_StructField, NodeType_TupleField, EnumVariant, EnumVariants, EnumVariantTuple, EnumVariantStruct, EnumVariantDiscriminant
};

//rust中的数据类型
enum Type {

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

};

/*
class of types
*/
class TypeNode : ASTNode {
 public:
  TypeNode(NodeType t, int l, int c) : ASTNode(t, l, c) {};
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
class ModuleNode : TypeNode {
 public:
  std::string id;
  bool isDeclaration;

  std::vector<InnerAttributeNode> InnerAttributes;
  std::vector<ItemType> items;

  ModuleNode(std::string i, int l, int c) : TypeNode(NodeType::Module, l ,c) {
    id = i;
  }; //声明式module

  ModuleNode(std::string , int l, int c, std::vector<InnerAttributeNode> InnerAttribute, std::vector<ItemType> item)
            : TypeNode(NodeType::Module, l, c), InnerAttributes(std::move(InnerAttribute)), items(std::move(item)) {};
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
  Type type;

  TypedSelf(bool im, Type t) : if_mut(im), type(t) {};
};

//OuterAttribute* ( ShorthandSelf | TypedSelf )
struct SelfParam {
  std::vector<OuterAttributeNode> outer_attributes;
  std::variant<ShorthandSelf, TypedSelf> self;

  SelfParam(std::vector<OuterAttributeNode> oa, ShorthandSelf s) : outer_attributes(std::move(oa)), self(s) {};
  SelfParam(std::vector<OuterAttributeNode> oa, TypedSelf s) : outer_attributes(std::move(oa)), self(s) {};
};

//...
struct ellipsis {
  std::string ellip = "...";

  ellipsis() = default;
};

//FunctionParamPattern → PatternNoTopAlt : ( Type | ... )
struct FunctionParamPattern {

};

//OuterAttribute* ( FunctionParamPattern | ... | Type​4 )
struct FunctionParam {
  std::vector<OuterAttributeNode> outer_attributes;
  std::variant<FunctionParamPattern, ellipsis, Type> info;

  FunctionParam(std::vector<OuterAttributeNode> oa) : outer_attributes(std::move(oa)), info(ellipsis()) {};
  FunctionParam(std::vector<OuterAttributeNode> oa, FunctionParamPattern fpp) : outer_attributes(std::move(oa)), info(fpp) {};
  FunctionParam(std::vector<OuterAttributeNode> oa, Type t) : outer_attributes(std::move(oa)), info(t) {};
};

// ->Type
struct FunctionReturnType {
  Type type;

  FunctionReturnType(Type t) : type(t) {};
};

//1.  SelfParam
//2.  SelfParam? FunctionParam*
struct FunctionParameter {
  int type = 0; // 1 for only SelfParam, 2 for having FunctionParam
  std::optional<SelfParam> self_param = std::nullopt;
  std::vector<FunctionParam> function_params;

  bool check() {
    if (type == 1) {
      if (self_param == std::nullopt || function_params.size() != 0) return false;
      else return true;
    } else if (type == 2) {
      if (function_params.size() < 1) return false;
      else return true;
    } else return false;
  }

  FunctionParameter() = default;
  FunctionParameter(int t, std::vector<FunctionParam> fp) : type(t), function_params(std::move(fp)) {};
  FunctionParameter(int t, SelfParam sp, std::vector<FunctionParam> fp) : type(t), self_param(sp), function_params(std::move(fp)) {};
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
class FunctionNode : public TypeNode {
 public:
  FunctionQualifier function_qualifier;
  std::string identifier;
  std::unique_ptr<GenParaNode> generic_params = nullptr;
  FunctionParameter function_parameter;
  std::optional<FunctionReturnType> return_type = std::nullopt;
  std::optional<WhereClause> where_clause = std::nullopt;
  std::unique_ptr<BlockExpression> block_expression = nullptr;

  FunctionNode(FunctionQualifier fq, std::string id, int l, int c) : function_qualifier(fq), identifier(id), TypeNode(NodeType::Function, l, c) {};
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
  Type type;

  StructField(Visibility vis, std::string id, Type t) : visibility(vis), identifier(id), type(t) {};
};

//StructFields → StructField ( , StructField )* ,?
class StructFieldNode : public ASTNode {
 public:
  std::vector<StructField> struct_fields;

  StructFieldNode(std::vector<StructField> sf, int l, int c) : struct_fields(std::move(sf)), ASTNode(NodeType::NodeType_StructField, l, c) {};
};

//TupleField → OuterAttribute* Visibility? Type
struct TupleField {
  std::optional<Visibility> visibility = std::nullopt;
  Type type;

  TupleField(Visibility vis, Type t) : visibility(vis), type(t) {};
  TupleField(Type t) : type(t) {};
};

//TupleFields → TupleField ( , TupleField )* ,?
class TupleFieldNode : public ASTNode {
 public:
  std::vector<TupleField> tuple_fields;

  TupleFieldNode(std::vector<TupleField> tf, int l, int c) : tuple_fields(std::move(tf)), ASTNode(NodeType::NodeType_TupleField, l, c) {};
};

/*
1.  StructStruct: struct IDENTIFIER GenericParams? WhereClause? ( { StructFields? } | ; )
2.  TupleStruct: struct IDENTIFIER GenericParams? ( TupleFields? ) WhereClause? ;
*/
class StructStructNode : public TypeNode {
 public:
  std::string identifier;
  std::optional<GenParaNode> generic_param = std::nullopt;
  std::optional<WhereClause> where_clause = std::nullopt;
  std::unique_ptr<StructFieldNode> struct_fields = nullptr;

  StructStructNode(std::string id, int l, int c) : identifier(id), TypeNode(NodeType::StructStruct, l, c) {};
};
class TupleStructNode : public TypeNode {
 public:
  std::string identifier;
  std::optional<GenParaNode> generic_param = std::nullopt;
  std::optional<WhereClause> where_clause = std::nullopt;
  std::unique_ptr<TupleFieldNode> tuple_fields = nullptr;

  TupleStructNode(std::string id, int l, int c) : identifier(id), TypeNode(NodeType::TupleStruct, l, c) {};
};

/*
classes for Enumeration
*/

//EnumVariantTuple → ( TupleFields? )
class EnumVariantTupleNode : public ASTNode {
 public: 
  std::optional<std::unique_ptr<TupleFieldNode>> tuple_field = std::nullopt;

  EnumVariantTupleNode(int l, int c) : ASTNode(NodeType::EnumVariantTuple, l, c) {};
};

//EnumVariantStruct → { StructFields? }
class EnumVariantStructNode : public ASTNode {
 public: 
  std::optional<std::unique_ptr<StructFieldNode>> struct_field = std::nullopt;

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
  std::optional<Visibility> visibility = std::nullopt;
  std::string identifier;
  std::optional<std::variant<EnumVariantTupleNode, EnumVariantStructNode>> tuple_or_struct_node = std::nullopt;
  std::optional<std::unique_ptr<EnumVariantDiscriminantNode>> discriminant = std::nullopt;

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
class EnumerationNode : public TypeNode {
 public:
  std::string identifier;
  std::optional<GenParaNode> generic_params;
  std::optional<WhereClause> where_clause;
  std::optional<EnumVariantsNode> enum_variants;

  EnumerationNode(std::string id, int l, int c) : identifier(id), TypeNode(NodeType::Enumeration, l, c) {};
};

class ConstantItemNode : public TypeNode {
  enum ConstantType {
    ID, _
  };
  ConstantType type;
  std::optional<std::string> identifier;
  Type type;
  std::optional<std::unique_ptr<ExpressionNode>> expression = std::nullopt;

  ConstantItemNode(std::string id, int l, int c) : type(ConstantType::ID), identifier(id), TypeNode(NodeType::ConstantItem, l, c) {};
  ConstantItemNode(int l, int c) : type(ConstantType::_), TypeNode(NodeType::ConstantItem, l, c) {};
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
  std::vector<Type> types;

  TypePathFnInputs(std::vector<Type> t) : types(std::move(t)) {};
};

//TypePathFn → ( TypePathFnInputs? ) ( -> TypeNoBounds )?
class TypePathFn {
 public:
  std::optional<TypePathFnInputs> type_path_fn_inputs = std::nullopt;

  TypePathFn(TypePathFnInputs tpfi) : type_path_fn_inputs(tpfi) {};
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
  Type type;
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
class AssociatedItemNode : TypeNode {
  Visibility visibility;
  std::variant<std::unique_ptr<ConstantItemNode>, std::unique_ptr<FunctionNode>> associated_item;

  AssociatedItemNode(Visibility vis, std::unique_ptr<ConstantItemNode> con, int l, int c) : visibility(vis), associated_item(std::move(con)), TypeNode(NodeType::AssociatedItem, l, c) {};
  AssociatedItemNode(Visibility vis, std::unique_ptr<FunctionNode> con, int l, int c) : visibility(vis), associated_item(std::move(con)), TypeNode(NodeType::AssociatedItem, l, c) {};
};

//Trait → unsafe? trait IDENTIFIER GenericParams? ( : TypeParamBounds? )? WhereClause? { InnerAttribute*   AssociatedItem* }
class TraitNode : public TypeNode {
 public:
  bool is_safe = false;
  std::string identifier;
  std::vector<std::unique_ptr<GenParaNode>> generic_params;
  std::unique_ptr<AssociatedItemNode> associated_item;

  TraitNode(bool safe, std::string id, std::vector<std::unique_ptr<GenParaNode>> gp, std::unique_ptr<AssociatedItemNode> a, int l, int c)
          : is_safe(safe), generic_params(std::move(gp)), associated_item(std::move(a)), TypeNode(NodeType::Trait, l, c) {};
};

/*
class for Implementation node
*/

//InherentImpl → impl GenericParams? Type WhereClause? {  InnerAttribute*  AssociatedItem*}

//Implementation → InherentImpl | TraitImpl
class ImplementationNode : public TypeNode {
 public:
  std::vector<std::unique_ptr<GenParaNode>> generic_params;
  Type type;
  WhereClause where_clause;
  std::unique_ptr<AssociatedItemNode> associated_item;

  ImplementationNode(std::vector<std::unique_ptr<GenParaNode>> gp, Type t, WhereClause wc, std::unique_ptr<AssociatedItemNode> ai, int l, int c) 
                    : generic_params(std::move(gp)), type(t), where_clause(wc), associated_item(std::move(ai)), TypeNode(NodeType::Implementation, l, c) {};
};

//GenericParams → < ( GenericParam ( , GenericParam )* ,? )? >
class GenParaNode : public TypeNode {
 public:
  std::vector<std::unique_ptr<GenericParam>> generic_params;

  GenParaNode(std::vector<std::unique_ptr<GenericParam>> gp, int l, int c) : generic_params(std::move(gp)), TypeNode(NodeType::GenPara, l, c) {};
};


/*
class of statements
*/

//LetStatement → OuterAttribute* let PatternNoTopAlt ( : Type )? ( = Expression | = Expressionexcept LazyBooleanExpression or end with a } else BlockExpression)? ;
class LetStatement {

};

//ExpressionStatement → ExpressionWithoutBlock ; | ExpressionWithBlock ;?
class ExpressionStatement {

};

//Statement →  ; | Item  | LetStatement | ExpressionStatement
class StatementNode : ASTNode {
 public:
  enum StatementType {
    SEMICOLON, ITEM, LETSTATEMENT, EXPRESSIONSTATEMENT
  };
  StatementType type;
  std::optional<std::unique_ptr<TypeNode>> item = std::nullopt; //item
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
#endif