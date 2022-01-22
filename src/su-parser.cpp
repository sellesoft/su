//TODO remove returning arrays of expressions and pass them by reference and modify instead
//     or just use global arrays like we use a global string in assembling
//TODO optimize the tree by bypassing nodes for expressions
//	   like, if an expression goes straight to being an int, we don't need several layers of 
//     nodes determining that



// full backus naur stuff
// <program>       :: = <function>
// <function>      :: = "int" <id> "(" ")" "{" { <block item> } "}"
// <block item>    :: = <statement> | <declaration>
// <declaration>   :: = "int" <id> [ = <exp> ] ";"
// <statement>     :: = "return" <exp> ";" | <exp> ";" | "if" "(" <exp> ")" <statement> [ "else" <statement> ] 
// <exp>           :: = <id> "=" <exp> | <conditional>
// <conditional>   :: = <logical or> [ "?" <exp> ":" <conditional> ]
// <logical or>    :: = <logical and> { "||" <logical and> } 
// <logical and>   :: = <bitwise or> { "&&" <bitwise or> } 
// <bitwise or>    :: = <bitwise xor> { "|" <bitwise xor> }
// <bitwise xor>   :: = <bitwise and> { "^" <bitwise and> }
// <bitwise and>   :: = <equality> { "&" <equality> }
// <equality>      :: = <relational> { ("!=" | "==") <relational> }
// <relational>    :: = <bitwise shift> { ("<" | ">" | "<=" | ">=") <bitwise shift> }
// <bitwise shift> :: = <additive> { ("<<" | ">>" ) <additive> }
// <additive>      :: = <term> { ("+" | "-") <term> }
// <term>          :: = <factor> { ("*" | "/" | "%") <factor> }
// <factor>        :: = "(" <exp> ")" | <unary> <factor> | <int> | <id>
// <unary>         :: = "!" | "~" | "-"


// <program>       :: = <function>
// <function>      :: = "int" <id> "(" ")" "{" { <block item> } "}"
// <block item>    :: = <statement> | <declaration>
// <declaration>   :: = "int" <id> [ = <exp> ] ";"
// <statement>     :: = "return" <exp> ";" | <exp> ";" | "if" "(" <exp> ")" <statement> [ "else" <statement> ] 
// <exp>           :: = <id> "=" <exp> | <conditional>
// <



//master token
token curt;
array<token> tokens;

b32 parse_failed = false;

//These defines are mostly for conveinence and clarity as to what im doing
//#define token_next() curt = tokens.next()
#define token_last curt = tokens.prev()

void token_next(u32 count = 1) {
	curt = tokens.next(count);
}

#define token_peek tokens.peek()
#define token_look_back(i) tokens.lookback(i)
#define curr_atch(tok) (curt.type == tok)

#define ParseFail(error)\
std::cout << "\nError: " << error << "\n caused by token '" << curt.str << "' on line " << curt.line << std::endl;  parse_failed = true;

#define Expect(Token_Type)\
if(curt.type == Token_Type) 

#define ExpectOneOf(exp_type)\
if(exp_type.has(curt.type)) 

#define ElseExpect(Token_Type)\
else if (curt.type == Token_Type) 

#define ExpectSignature(...) if(check_signature(__VA_ARGS__))
#define ElseExpectSignature(...)  else if(check_signature(__VA_ARGS__))

#define ExpectFail(error)\
else { ParseFail(error); DebugBreakpoint; } //TODO make it so this breakpoint only happens in debug mode or whatever

#define ExpectFailCode(failcode)\
else { failcode }

local map<Token_Type, ExpressionType> binaryOps{
	{Token_Multiplication,     Expression_BinaryOpMultiply},
	{Token_Division,           Expression_BinaryOpDivision},
	{Token_Negation,           Expression_BinaryOpMinus},
	{Token_Plus,               Expression_BinaryOpPlus},
	{Token_AND,                Expression_BinaryOpAND},
	{Token_OR,                 Expression_BinaryOpOR},
	{Token_LessThan,           Expression_BinaryOpLessThan},
	{Token_GreaterThan,        Expression_BinaryOpGreaterThan},
	{Token_LessThanOrEqual,    Expression_BinaryOpLessThanOrEqual},
	{Token_GreaterThanOrEqual, Expression_BinaryOpGreaterThanOrEqual},
	{Token_Equal,              Expression_BinaryOpEqual},
	{Token_NotEqual,           Expression_BinaryOpNotEqual},
	{Token_BitAND,             Expression_BinaryOpBitAND},
	{Token_BitOR,              Expression_BinaryOpBitOR},
	{Token_BitXOR,		       Expression_BinaryOpBitXOR},
	{Token_BitShiftLeft,       Expression_BinaryOpBitShiftLeft},
	{Token_BitShiftRight,      Expression_BinaryOpBitShiftRight},
	{Token_Modulo,             Expression_BinaryOpModulo},
};

local map<Token_Type, ExpressionType> unaryOps{
	{Token_BitNOT,     Expression_UnaryOpBitComp},
	{Token_LogicalNOT, Expression_UnaryOpLogiNOT},
	{Token_Negation,   Expression_UnaryOpNegate},
};

local array<Token_Type> typeTokens{
	Token_Signed32,
	Token_Signed64,
	Token_Unsigned32,
	Token_Unsigned64,
	Token_Float32,
	Token_Float64,
};

template<class... T>
inline b32 check_signature(T... in) {
	int i = 0;
	return ((tokens.peek(i++).type == in) && ...);
}

template<typename... T> inline b32
next_match(T... in) {
	return ((tokens.peek(1).type == in) || ...);
}


Function*    function;
Scope*       scope;
Declaration* declaration;
Statement*   statement;
Expression*  expression;

Arena arena;

inline Node* new_function(string& identifier, const string& node_str = "") {
	function = (Function*)arena.add(Function());
	function->identifier = identifier;
	function->node.type    = NodeType_Function;
	function->node.comment = node_str;
	return &function->node;
}

inline Node* new_scope(const string& node_str = "") {
	scope = (Scope*)arena.add(Scope());
	scope->node.type    = NodeType_Scope;
	scope->node.comment = node_str;
	return &scope->node;
}

inline Node* new_declaration(const string& node_str = "") {
	declaration = (Declaration*)arena.add(Declaration());
	declaration->node.type    = NodeType_Declaration;
	declaration->node.comment = node_str;
	return &declaration->node;
}

inline Node* new_statement(StatementType type, const string& node_str = ""){
	statement = (Statement*)arena.add(Statement());
	statement->type = type;
	statement->node.type    = NodeType_Statement;
	statement->node.comment = node_str;
	return &statement->node;
}

inline Node* new_expression(string& str, ExpressionType type, const string& node_str = "") {
	expression = (Expression*)arena.add(Expression());
	expression->expstr = str;
	expression->type   = type;
	expression->node.type    = NodeType_Expression;
	expression->node.comment = node_str;
	return &expression->node;
}

enum ParseState {
	psGlobal,      	// <program>       :: = <function>
	psFunction,		// <function>      :: = "int" <id> "(" ")" <scope>
	psScope,        // <scope>         :: = "{" { (<declaration> | <statement> | <scope>) } "}"
	psDeclaration,	// <declaration>   :: = "int" <id> [ = <exp> ] ";"
	psStatement,	// <statement>     :: = "return" <exp> ";" | <exp> ";" | <scope> | "if" "(" <exp> ")" <statement> [ "else" <statement> ] 
	psExpression,	// <exp>           :: = <id> "=" <exp> | <conditional>
	psConditional,	// <conditional>   :: = <logical or> | "if" "(" <exp> ")" <exp> "else" <exp> 
	psLogicalOR,	// <logical or>    :: = <logical and> { "||" <logical and> } 
	psLogicalAND,	// <logical and>   :: = <bitwise or> { "&&" <bitwise or> } 
	psBitwiseOR,	// <bitwise or>    :: = <bitwise xor> { "|" <bitwise xor> }
	psBitwiseXOR,	// <bitwise xor>   :: = <bitwise and> { "^" <bitwise and> }
	psBitwiseAND,	// <bitwise and>   :: = <equality> { "&" <equality> }
	psEquality,		// <equality>      :: = <relational> { ("!=" | "==") <relational> }
	psRelational,	// <relational>    :: = <bitwise shift> { ("<" | ">" | "<=" | ">=") <bitwise shift> }
	psBitshift,		// <bitwise shift> :: = <additive> { ("<<" | ">>" ) <additive> }
	psAdditive,		// <additive>      :: = <term> { ("+" | "-") <term> }
	psTerm,			// <term>          :: = <factor> { ("*" | "/" | "%") <factor> }
	psFactor,		// <factor>        :: = "(" <exp> ")" | <unary> <factor> | <int> | <id>
	psUnary,        // <unary>         :: = "!" | "~" | "-"
}; 

Node* debugprogramnode = 0;

#define EarlyOut goto emergency_exit
Node* parser(ParseState state, Node* node) {
	if (parse_failed) return 0;
	
	switch (state) {
		
		case psGlobal: { ////////////////////////////////////////////////////////////////////// @Global
			while (!(curt.type == Token_EOF || next_match(Token_EOF))) {
				ExpectOneOf(typeTokens) {
					token_next();
					Expect(Token_Identifier){
						if (next_match(Token_OpenParen)) {
							new_function(curt.str, toStr("function ", curt.str));
							insert_last(node, &function->node);
							parser(psFunction, &function->node);
						}
					}
					Expect(Token_Identifier) {
						token_next();
						Expect(Token_OpenParen) {
							
						}
					}
				}ExpectFail("yeah i dont know right now");
				token_next();
			}
		}break;
		
		case psFunction: { //////////////////////////////////////////////////////////////////// @Function
			token_next();
			Expect(Token_OpenParen) { 
				token_next();
				//function parameter checking will go here
				Expect(Token_CloseParen) {
					token_next();
					Expect(Token_OpenBrace) {
						parser(psScope, node);
					}ExpectFail("expected {");
				}ExpectFail("expected )");
			}ExpectFail("expected (");
		}break;
		
		case psScope: { /////////////////////////////////////////////////////////////////////// @Scope
			Node* me = new_scope("scope");
			insert_last(node, &scope->node);
			while (!next_match(Token_CloseBrace)) {
				token_next();
				ExpectOneOf(typeTokens) {
					new_declaration("decl " + tokens.peek().str);
					insert_last(me, &declaration->node);
					parser(psDeclaration, &declaration->node);
				}
				ElseExpect(Token_OpenBrace) {
					parser(psScope, me);
				}
				else {
					parser(psStatement, me);
				}
				if (next_match(Token_EOF)) { ParseFail("Unexpected EOF"); EarlyOut; }
			}
			token_next();
		}break;
		
		case psDeclaration: {////////////////////////////////////////////////////////////////// @Declaration
			Token_Type type = curt.type;
			token_next();
			Expect(Token_Identifier) {
				declaration->identifier = curt.str;
				token_next();
				Expect(Token_Assignment) {
					new_expression(curt.str, ExpressionGuard_Assignment, ExTypeStrings[ExpressionGuard_Assignment]);
					token_next();
					insert_last(node, &expression->node);
					Node* ret = parser(psExpression, &expression->node);
					token_next();
					Expect(Token_Semicolon) { return ret; }
					ExpectFail("missing ; after variable assignment")
				}
				ElseExpect(Token_Semicolon) {}
				ExpectFail("missing semicolon or assignment after variable declaration");
			}
		}break;
		
		case psStatement: {//////////////////////////////////////////////////////////////////// @Statement
			switch (curt.type) {
				case Token_If: {
					Node* ifno = new_statement(Statement_Conditional, "if statement");
					insert_last(node, ifno);
					token_next();
					Expect(Token_OpenParen) {
						token_next();
						parser(psExpression, ifno);
						if (!statement->node.first_child) {
							ParseFail("missing expression for if statement");
						}
						token_next();
						Expect(Token_CloseParen) {
							token_next();
							Expect(Token_OpenBrace) {
								parser(psStatement, ifno);
							}
							else {
								ExpectOneOf(typeTokens) {
									ParseFail("can't declare a variable in an unscoped if statement");
									return 0;
								}
								parser(psStatement, ifno);
								Expect(Token_Semicolon) { }
								ExpectFail("expected a ;");
							}
							if (next_match(Token_Else)) {
								token_next();
								parser(psStatement, ifno);
							}
						}ExpectFail("expected )");
					}ExpectFail("expected (");
				}break;
				
				case Token_Else: {
					new_statement(Statement_Conditional, "else statement");
					insert_last(node, &statement->node);
					token_next();
					parser(psStatement, &statement->node);
				}break;
				
				case Token_Literal:
				case Token_Identifier: {
					new_statement(Statement_Expression, "exp statement");
					insert_last(node, &statement->node);
					parser(psExpression, &statement->node);
					token_next();
					Expect(Token_Semicolon) {} 
					ExpectFail("Expected a ;");
				}break;
				
				case Token_Return: {
					new_statement(Statement_Return, "return statement");
					insert_last(node, &statement->node);
					token_next();
					parser(psExpression, &statement->node);
					token_next();
					Expect(Token_Semicolon) {}
					ExpectFail("expected a ;");
					
				}break;
				
				case Token_OpenBrace: {
					parser(psScope, node);
				}break;
			}
		}break;
		
		case psExpression: {/////////////////////////////////////////////////////////////////// @Expression
			switch (curt.type) {
				case Token_Identifier: {
					if (next_match(Token_Assignment)) {
						new_expression(curt.str, Expression_IdentifierLHS, toStr(ExTypeStrings[Expression_IdentifierLHS], " ", curt.str));     
						insert_last(node, &expression->node);
						token_next();
						new_expression(curt.str, Expression_BinaryOpAssignment, ExTypeStrings[Expression_BinaryOpAssignment]);
						insert_last(node, &expression->node);
						token_next();
						new_expression(curt.str, ExpressionGuard_Assignment);
						
						Node* ret = parser(psExpression, &expression->node);
						insert_last(node, ret);
						return ret;
					}
					else {
						new_expression(curt.str, ExpressionGuard_HEAD);
						Node* ret = parser(psConditional, &expression->node);
						insert_last(node, ret);
						return ret;
					}
				}break;
				default: {
					Node* me  = new_expression(curt.str, ExpressionGuard_HEAD);
					Node* ret = parser(psConditional, node);
					//NodeInsertChild(node, ret, ret->type, ret->debug_str);
					return ret;
				}break;
				
			}
		}break;
		
		case psConditional: {////////////////////////////////////////////////////////////////// @Conditional
			Node* ret = parser(psLogicalOR, node);
			if (!next_match(Token_QuestionMark))
				return ret;
			
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psAdditive, me);
			while (next_match(Token_QuestionMark)) {
				token_next();
				new_expression(curt.str, Expression_TernaryConditional);
				token_next();
				//NodeInsertChild(me, &expression->node, NodeType_Expression, ExTypeStrings[Expression_TernaryConditional]); token_next();
				ret = parser(psExpression, &expression->node);
				insert_last(me, ret);
				token_next();
				Expect(Token_Colon) {
					token_next();
					new_expression(curt.str, ExpressionGuard_Conditional);
					//NodeInsertChild(me, &expression->node, NodeType_Expression, ExTypeStrings[ExpressionGuard_Conditional]);
					ret = parser(psLogicalOR, &expression->node);
					insert_last(me, ret);
				}ExpectFail("Expected : for ternary conditional")
			}
			return me;
		}break;
		
		case psLogicalOR: {//////////////////////////////////////////////////////////////////// @Logical OR
			Node* ret = parser(psLogicalAND, node);
			if (!next_match(Token_OR))
				return ret;
			
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psLogicalAND, me);
			while (next_match(Token_OR)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psLogicalAND, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psLogicalAND: {/////////////////////////////////////////////////////////////////// @Logical AND
			Node* ret = parser(psBitwiseOR, node);
			if (!next_match(Token_AND))
				return ret;
			
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psBitwiseOR, me);
			while (next_match(Token_AND)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psBitwiseOR, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psBitwiseOR: {//////////////////////////////////////////////////////////////////// @Bitwise OR
			Node* ret = parser(psBitwiseXOR, node);
			if (!next_match(Token_BitOR))
				return ret;
			
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psBitwiseXOR, me);
			while (next_match(Token_BitOR)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psBitwiseXOR, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psBitwiseXOR: {/////////////////////////////////////////////////////////////////// @Bitwise XOR
			Node* ret = parser(psBitwiseAND, node);
			if (!next_match(Token_BitXOR))
				return ret;
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psBitwiseAND, me);
			while (next_match(Token_BitXOR)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psBitwiseAND, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psBitwiseAND: {/////////////////////////////////////////////////////////////////// @Bitwise AND
			Node* ret = parser(psEquality, node);
			if (!next_match(Token_BitAND))
				return ret;
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psEquality, me);
			while (next_match(Token_BitAND)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psEquality, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psEquality: {///////////////////////////////////////////////////////////////////// @Equality
			Node* ret = parser(psRelational, node);
			if (!next_match(Token_NotEqual, Token_Equal))
				return ret;
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psRelational, me);
			while (next_match(Token_NotEqual, Token_Equal)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psRelational, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psRelational: {/////////////////////////////////////////////////////////////////// @Relational
			Node* ret = parser(psBitshift, node);
			if (!next_match(Token_LessThan, Token_GreaterThan, Token_LessThanOrEqual, Token_GreaterThanOrEqual))
				return ret;
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psBitshift, me);
			while (next_match(Token_LessThan, Token_GreaterThan, Token_LessThanOrEqual, Token_GreaterThanOrEqual)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psBitshift, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psBitshift: {///////////////////////////////////////////////////////////////////// @Bitshift
			Node* ret = parser(psAdditive, node);
			if (!next_match(Token_BitShiftLeft, Token_BitShiftRight))
				return ret;
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psAdditive, me);
			while (next_match(Token_BitShiftLeft, Token_BitShiftRight)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psAdditive, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psAdditive: {///////////////////////////////////////////////////////////////////// @Additive
			Node* ret = parser(psTerm, node);
			if (!next_match(Token_Plus, Token_Negation))
				return ret;
			token_next();
			Node* me = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psTerm, me);
			while (next_match(Token_Plus, Token_Negation)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psTerm, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psTerm: {///////////////////////////////////////////////////////////////////////// @Term
			Node* ret = parser(psFactor, node);
			if (!next_match(Token_Multiplication, Token_Division, Token_Modulo))
				return ret;
			token_next();
			Node* me  = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]); 
			change_parent(me, ret);
			insert_last(node, me);
			token_next();
			ret = parser(psFactor, me);
			while (next_match(Token_Multiplication, Token_Division, Token_Modulo)) {
				token_next();
				Node* me2 = new_expression(curt.str, *binaryOps.at(curt.type), ExTypeStrings[*binaryOps.at(curt.type)]);
				token_next();
				ret = parser(psFactor, node);
				change_parent(me2, me);
				change_parent(me2, ret);
				insert_last(node, me2);
				me = me2;
			}
			return me;
		}break;
		
		case psFactor: {/////////////////////////////////////////////////////////////////////// @Factor
			switch (curt.type) {
				case Token_Literal: {
					new_expression(curt.str, Expression_IntegerLiteral, toStr(ExTypeStrings[Expression_IntegerLiteral], " ", curt.str));
					insert_last(node, &expression->node);
					return &expression->node;
				}break;
				
				case Token_OpenParen: {
					token_next();
					Node* ret = parser(psExpression, &expression->node);
					token_next();
					Expect(Token_CloseParen) { return ret; }
					ExpectFail("expected a )");
				}break;
				
				case Token_Identifier: {
					new_expression(curt.str, Expression_IdentifierRHS, toStr(ExTypeStrings[Expression_IdentifierRHS], " ", curt.str));
					insert_last(node, &expression->node);
					return &expression->node;
				}break;
				
				default: {
					ExpectOneOf(unaryOps) {
						new_expression(curt.str, *unaryOps.at(curt.type), ExTypeStrings[*unaryOps.at(curt.type)]);
						insert_last(node, &expression->node);
						token_next();
						Node* ret = &expression->node;
						parser(psFactor, &expression->node);
						//NodeInsertChild(node, ret, ret->type, ret->debug_str);
						return ret;
					}
					ExpectFail("unexpected token found in factor");
				}break;
			}
		}break;
	}
	
	
	emergency_exit: //TODO maybe not necessary
	return 0;
}

// <program> ::= <function>
b32 suParser::parse(array<token>& tokens_in, Program& mother) {
	arena.init(Kilobytes(10));
	
	tokens = tokens_in;
	curt = tokens[0];
	
	mother.node.comment = "program";
	debugprogramnode = &mother.node;
	
	parser(psGlobal, &mother.node);
	
	return 0;//parse_failed;
}