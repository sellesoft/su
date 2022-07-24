#ifdef FIX_MY_IDE_PLEASE
#include "core/memory.h"

#define KIGU_STRING_ALLOCATOR deshi_temp_allocator
#include "kigu/profiling.h"
#include "kigu/array.h"
#include "kigu/array_utils.h"
#include "kigu/common.h"
#include "kigu/cstring.h"
#include "kigu/map.h"
#include "kigu/string.h"
#include "kigu/node.h"

#define DESHI_DISABLE_IMGUI
#include "core/logger.h"
#include "core/platform.h"
#include "core/file.h"
#include "core/threading.h"

#include <stdio.h>
#include <stdlib.h>

#include "kigu/common.h"
#include "kigu/unicode.h"
#include "kigu/hash.h"
#include "types.h"

#endif

#define perror(token, ...)\
sufile->logger.error(token, __VA_ARGS__)

#define perror_ret(token, ...){\
sufile->logger.error(token, __VA_ARGS__);\
return 0;\
}

#define pwarn(token, ...)\
sufile->logger.warn(token, __VA_ARGS__);


#define expect(...) if(curr_match(__VA_ARGS__))
#define expect_next(...) if(next_match(__VA_ARGS__))
#define expect_group(...) if(curr_match_group(__VA_ARGS__))

str8 type_token_to_str(Type type){
    switch(type){
        case Token_Void:       return STR8("void");
        case Token_Signed8:    return STR8("s8");
        case Token_Signed16:   return STR8("s16");
        case Token_Signed32:   return STR8("s32");
        case Token_Signed64:   return STR8("s64");
        case Token_Unsigned8:  return STR8("u8");
        case Token_Unsigned16: return STR8("u16");
        case Token_Unsigned32: return STR8("u32");
        case Token_Unsigned64: return STR8("u64");
        case Token_Float32:    return STR8("f32");
        case Token_Float64:    return STR8("f64");
        case Token_String:     return STR8("str");
        case Token_Any:        return STR8("any");
        case Token_Struct:     return STR8("struct-type");
    }
    return STR8("UNKNOWN DATA TYPE");
}

template<typename... T>
suNode* ParserThread::binop_parse(suNode* node, suNode* ret, Type next_stage, T... tokchecks){
    //TODO(sushi) need to detect floats being used in bitwise operations (or just allow it :) 
    suNode* out = ret;
    suNode* lhs_node = ret;
    while(next_match(tokchecks...)){
        curt++;
        //make binary op expression
        Expression* op = arena.make_expression(curt->raw);
        op->type = binop_token_to_expression(curt->type);
        op->token_start = curt-1;

        //readjust parents to make the binary op the new child of the parent node
        //and the ret node a child of the new binary op
        change_parent(node, &op->node);
        change_parent(&op->node, out);

        //evaluate next expression
        curt++;
        Expression* rhs = ExpressionFromNode(define(&op->node, next_stage));

        out = &op->node;
    }
    return out;
}



suNode* ParserThread::define(suNode* node, Type stage){DPZoneScoped;
    //ThreadSetName(suStr8("parsing ", curt->raw, " in ", curt->file));
    switch(stage){
        case psFile:{ //-------------------------------------------------------------------------------------------------File
            // i dont think this stage will ever be touched
        }break;

        case psDirective:{ //---------------------------------------------------------------------------------------Directive
            // there is no reason to check for invalid directives here because that is handled by lexer
            expect(Token_Directive_Import){
                Statement* s = arena.make_statement(curt->raw);
                insert_last(node, &s->node);
                curt++;
                expect(Token_OpenBrace){
                    curt++;
                    while(!curr_match(Token_CloseBrace)){
                        suNode* n = define(&s->node, psImport);
                        if(!n) return 0;
                        expect(Token_Comma) {
                            curt++;
                            expect(Token_CloseBrace){curt++;break;}
                        }else expect(Token_CloseBrace) {curt++;break;}
                        else perror_ret(curt, "expected a , or a } after import block.");

                    }     
                }else{
                    suNode* n = define(&s->node, psImport);
                    if(!n) return 0;
                }
                
            }

        }break;

        case psSubimport:{

        }break;

        case psImport:{ //---------------------------------------------------------------------------------------------Import
            //#import 
            //TODO(sushi) cleanup the massive code duplication here and consider moving import evaluation (not parsing) to the next stage
            array<Declaration*> imported_decls;

            expect(Token_LiteralString){
                Expression* e = arena.make_expression(curt->raw);
                e->type = Expression_Literal;
                insert_last(node, &e->node);
                curt++;
                expect(Token_OpenBrace){
                    curt++;
                    while(!curr_match(Token_CloseBrace)){
                        expect(Token_Identifier){
                            Expression* id = arena.make_expression(curt->raw);
                            id->type = Expression_IdentifierLHS;
                            id->token_start = curt;
                            id->token_end = curt;
                            curt++;
                            expect(Token_As){
                                Expression* op = arena.make_expression(curt->raw);
                                op->type = Expression_BinaryOpAs;
                                op->token_start = id->token_start;
                                curt++;
                                expect(Token_Identifier){
                                    op->token_end = curt;
                                    Expression* idrhs = arena.make_expression(curt->raw);
                                    idrhs->type = Expression_IdentifierLHS;
                                    idrhs->token_start = curt;
                                    idrhs->token_end = curt;
                                    insert_last(&op->node, &id->node);
                                    insert_last(&op->node, &idrhs->node);
                                    insert_last(&e->node, &op->node);
                                    curt++;
                                }else perror_ret(curt, "expected an identifier for subimport alias.");
                            }else{
                                insert_last(&e->node, &id->node);
                            }
                            //this seems redundant
                            //i dont think we need to even check for commas in this situation
                            //but i prefer to enforce them for multiple items for consistency
                            expect(Token_Comma){
                                curt++;
                                expect(Token_CloseBrace){curt++;break;}
                            }else expect(Token_CloseBrace) {curt++;break;}
                            else perror_ret(curt, "expected a , after subimport.");
                        }else perror_ret(curt, "expected an identifier for subimport.");
                    }

                }
                return &e->node;
            }

           
        }break;

        case psRun:{ //---------------------------------------------------------------------------------------------------Run
            NotImplemented;
        }break;

        case psScope:{ //-----------------------------------------------------------------------------------------------Scope
            Scope* s = arena.make_scope(curt->raw);
            suNode* me = &s->node;
            insert_last(node, me);
            while(!next_match(Token_CloseBrace)){
                curt++;
                expect(Token_Identifier){
                    if(curt->is_declaration){
                        suNode* ret = define(me, psDeclaration);
                        if(!ret) return 0;
                        Declaration* d = DeclarationFromNode(ret);
                        if(d->type == Declaration_Variable){
                            expect(Token_Semicolon){}
                            else perror_ret(curt, "expected ; after variable declaration.");
                        }
                    }else{
                        define(me, psStatement);
                    }
                }else expect(Token_EOF){
                    perror_ret(curt, "unexpected end of file.");
                } else {
                    define(me, psStatement);
                }
            }
            curt++;
        }break;

        case psDeclaration:{ //-----------------------------------------------------------------------------------Declaration
            expect(Token_Identifier) {} else perror_ret(curt, "INTERNAL: parser stage psDeclaration started, but current token is not an identifier.");
            DPTracyDynMessage(toStr("declaring identifier ", curt->raw));
            switch(curt->decl_type){
                case Declaration_Structure:{
                    Declaration* decl = 0;

                    //first make sure that the user isnt trying to redefine a structure that was already made in an imported file
                    // if(Declaration* d = sufile->parser.imported_decl.at(curt->raw)){
                    //     if(d->type == Declaration_Structure){
                    //         perror(curt, "attempt to define structure '", curt->raw, "' but it already exists as an import from ", d->token_start->file);
                    //         sufile->logger.log(d->token_start, 0, "original definition of '", curt->raw, "' is here.");
                    //         return 0;
                    //     }
                    // } 

                    // if(curt->is_global){
                    //     //if the token is global then it will have been added to pending_globals 
                    //     if(Declaration* d = parser->pending_globals.at(curt->raw)){
                    //         if(d->complete || d->in_progress){
                    //             //perror(curt, "attempt to define structure '", curt->raw, "' but it already has a definition.");
                    //             //sufile->logger.log(curt, 0, "original definition of '", curt->raw, "' is here.");
                    //             //return 0;
                    //         }
                    //         decl = d;
                    //     }
                    // }else{
                    //     //otherwise just make a new one
                    //     //TODO(sushi) we should probably check for already existant struct defs here too, since someone could try and redefine a 
                    //     //            struct in a local scope
                    //     Struct* s = arena.make_struct(curt->raw);
                    //     decl = &s->decl;
                    //     if(is_internal){
                    //         sufile->parser.internal_decl.add(curt->raw, decl);
                    //     }else{
                    //         sufile->parser.exported_decl.add(curt->raw, decl);                            
                    //     }
                    // }

                    Struct* s = arena.make_struct(curt->raw);//StructFromDeclaration(decl);
                    decl = &s->decl;

                    s->members.init();
                    s->decl.in_progress = 1;
                    s->decl.working_thread = this;
                    s->decl.token_start = curt;
                    s->decl.type = Declaration_Structure;
                    s->decl.identifier = curt->raw;
                    s->decl.declared_identifier = curt->raw;

                    if(curt->is_global){
                        if(is_internal) sufile->parser.internal_decl.add(s->decl.identifier, &s->decl);
                        else            sufile->parser.exported_decl.add(s->decl.identifier, &s->decl);
                    }

                    curt++;
                    expect(Token_Colon){
                        curt++;
                        expect(Token_StructDecl){
                            curt++;
                            expect(Token_OpenBrace){
                                while(!next_match(Token_CloseBrace)){
                                    curt++;
                                    expect(Token_Identifier){
                                        if(!curt->is_declaration){
                                            perror(curt, "only declarations are allowed inside struct definitions. NOTE(sushi) if this is a declaration, then this error indicates something wrong internally, please let me know.");
                                            return 0;
                                        }
                                        suNode* fin = define(&s->decl.node, psDeclaration);
                                        if(!fin){
                                            return 0;
                                        } 
                                        Declaration* d = DeclarationFromNode(fin);
                                        s->members.add(d->identifier, d);
                                    }
                                }
                            } else perror(curt, "expected a '{' after 'struct' in definition of struct '", s->decl.identifier, "'.");
                        } else perror(curt, "INTERNAL: expected 'struct' after ':' for struct definition. NOTE(sushi) tell me if this happens");
                    } else perror(curt, "INTERNAL: expected ':' for struct declaration. NOTE(sushi) tell me if this happens");

                    s->decl.complete = 1;
                    this->cv.notify_all();
                }break;

                case Declaration_Function:{
                    //str8 fname = suStr8(curt->raw, curt->l0, curt->c0);
                    //Declaration* decl = parser->pending_globals.at(fname);

                    //if(curt->is_global && !decl){
                    //    perror(curt, "INTERNAL: a global declaration token does not have a corresponding pending_globals entry.");
                    //    return 0;
                    //}else if(!decl){
                    //    Function* f = arena.make_function(curt->raw);
                    //    decl = &f->decl;
                    //}

                    Function* f = arena.make_function(curt->raw);//FunctionFromDeclaration(decl);
                    f->decl.token_start = curt;
                    f->decl.type = Declaration_Function;
                    f->decl.identifier = curt->raw;
                    f->decl.declared_identifier = curt->raw;
                    b32 is_global = curt->is_global;
                    str8 id = curt->raw;
                    f->internal_label = id;
                    f->internal_label = suStr8(f->internal_label, "@");
                    curt++;
                    expect(Token_OpenParen){ // name(
                        while(1){
                            curt++;
                            expect(Token_CloseParen) { break; }
                            else expect(Token_Comma){}
                            else expect(Token_Identifier){
                                Variable* v = VariableFromNode(define(&f->decl.node, psDeclaration));
                                f->internal_label = suStr8(f->internal_label, (v->decl.token_start + 2)->raw, ",");
                                forI(v->pointer_depth){
                                    f->internal_label = suStr8(f->internal_label, STR8("*"));
                                }

                            } else perror_ret(curt, "expected an identifier for function variable declaration.");
                        }
                        curt++;
                        expect(Token_Colon){ // name(...) :
                            curt++;
                            //TODO(sushi) multiple return types
                            expect_group(TokenGroup_Type){ // name(...) : <type>
                                f->data_type = curt->type;
                                f->internal_label = suStr8(f->internal_label, "@", curt->raw);
                                if(is_global){
                                    if(is_internal) sufile->parser.internal_decl.add(f->internal_label, &f->decl);
                                    else            sufile->parser.exported_decl.add(f->internal_label, &f->decl);
                                }
                            }else expect(Token_Identifier){ // name(...) : <type>
                                //we are most likely referencing a struct type in this case
                                f->data_type = Token_Struct;
                                f->internal_label = suStr8(f->internal_label, "@", curt->raw);
                                if(curt->is_global){
                                    if(is_internal) sufile->parser.internal_decl.add(f->internal_label, &f->decl);
                                    else            sufile->parser.exported_decl.add(f->internal_label, &f->decl);
                                }
                            } else perror(curt, "expected a type specifier after ':' in function declaration.");
                            curt++;
                            expect(Token_OpenBrace) {
                                define(&f->decl.node, psScope);
                            } else perror(curt, "expected '{' after function declaration.");
                        } else perror(curt, "expected : after function definition.");
                    } else perror(curt, "expected ( after identifier in function declaration.");
                }break;

                case Declaration_Variable:{ //name
                    Variable* v = arena.make_variable(suStr8(curt->raw, " -decl"));
                    if(curt->is_global){
                        if(is_internal) sufile->parser.internal_decl.add(curt->raw, &v->decl);
                        else            sufile->parser.exported_decl.add(curt->raw, &v->decl);
                    }
                    v->decl.declared_identifier = curt->raw;
                    v->decl.identifier = curt->raw;
                    v->decl.token_start = curt;
                    v->decl.type = Declaration_Variable;
                    str8 id = curt->raw;
                    curt++;
                    expect(Token_Colon){ // name :
                        curt++;
                        expect_group(TokenGroup_Type){ // name : <type>
                            v->data.type = curt->type;
                            v->data.type_name = curt->raw;
                        }else expect(Token_Identifier){ // name : <type>
                            //in this case we expect this identifier to actually be a struct so we must look for it
                            //first check known declarations 
                            v->data.type = Token_Struct;
                            v->data.type_name = curt->raw;
                        }else v->data.implicit = 1;
                        curt++;
                        expect(Token_Assignment){ // name : <type> = || name := 
                            Token* before = curt;
                            curt++;
                            Expression* e = ExpressionFromNode(define(&v->decl.node, psExpression));
                        } else if(v->data.implicit) perror(curt, "Expected a type specifier or assignment after ':' in declaration of variable '", id, "'");
                        insert_last(node, &v->decl.node);
                        return &v->decl.node;
                    } else perror(curt, "Expected a ':' after identifier for variable decalration.");
                }break;
            }
        }break;

        case psStatement:{ //---------------------------------------------------------------------------------------Statement
            switch(curt->type){
                case Token_OpenBrace:{
                    define(node, psScope);
                }break;

                case Token_Directive_Import:
                case Token_Directive_Include:
                case Token_Directive_Internal:
                case Token_Directive_Run:{
                    //not sure yet when this will happen and how it will be handled
                    //the only place this should be encountered is #run, probably
                    TestMe;
                }break;

                case Token_Using:{
                    Statement* s = arena.make_statement();
                    s->type = Statement_Using;
                    s->token_start = curt;
                    curt++;
                    expect(Token_Identifier){
                        Expression* e = arena.make_expression(curt->raw);
                        e->type = Expression_IdentifierRHS;
                        e->token_start = curt;
                        e->token_end = curt;
                        insert_last(&s->node, &e->node);
                        curt++;
                        expect(Token_Semicolon){}
                        else perror_ret(curt, "expected a semicolon after using statement");
                    } else perror_ret(curt, "expected a variable identifier after 'using'.");
                }break;

                case Token_Return:{
                    Statement* s = arena.make_statement(curt->raw);
                    s->type = Statement_Return;
                    s->token_start = curt;
                    insert_last(node, &s->node);
                    curt++;
                    expect(Token_Semicolon){
                        curt++;
                    }else{
                        suNode* n = define(&s->node, psExpression);
                        if(!n) return 0;
                        change_parent(&s->node, n);
                    }
                    return &s->node;
                }break;

                case Token_If:{
                    Statement* s = arena.make_statement(curt->raw);
                    s->token_start = curt;
                    s->type = Statement_Conditional;
                    insert_last(node, &s->node);
                    curt++;
                    expect(Token_OpenParen){
                        curt++;
                        expect(Token_CloseParen) perror_ret(curt, "expected an expression for if statement.");
                        suNode* n = define(&s->node, psExpression);
                        if(!n) return 0;
                        curt++;
                        expect(Token_CloseParen){
                            curt++;
                            expect(Token_OpenBrace){
                                n = define(&s->node, psScope);
                                if(!n) return 0;
                                s->token_end = curt;
                            }else{  
                                //check that the user isnt trying to declare anything in an unbraced if statement
                                expect(Token_Identifier)
                                    if(curt->is_declaration) perror_ret(curt, "can't declare something in an unscoped if statement.");
                                n = define(&s->node, psStatement);
                                if(!n) return 0;
                                curt++;
                                expect(Token_Semicolon){}
                                else perror_ret(curt, "expected a ;");
                                s->token_end = curt;
                            }

                        }else perror_ret(curt, "expected a ) after if statement's expression.");
                    }else perror_ret(curt, "expected a ( for if statement.");
                    return &s->node;
                }break;

                case Token_Else:{
                    Statement* s = arena.make_statement(curt->raw);
                    s->type = Statement_Else;
                    s->token_start = curt;
                    insert_last(node, &s->node);
                    curt++;
                    expect(Token_OpenBrace){
                        suNode* n = define(&s->node, psScope);
                        if(!n) return 0;
                        s->token_end = curt;
                    }else{
                        //check that the user isnt trying to declare anything in an unscoped else statement
                        expect(Token_Identifier)
                            if(curt->is_declaration) perror_ret(curt, "can't declare something in an unscoped else statement.");
                        suNode* n = define(&s->node, psStatement);
                        if(!n) return 0;
                        curt++;
                        expect(Token_Semicolon){}
                        else perror_ret(curt, "expected a ;");
                        s->token_end = curt;
                    }
                    return &s->node;
                }break; 

                default:{
                    //this is probably just some expression
                    Statement* s = arena.make_statement();
                    s->token_start = curt;
                    insert_last(node, &s->node);
                    suNode* ret = define(node, psExpression);
                    if(!ret) return 0;
                    curt++;
                    expect(Token_Semicolon){}
                    else perror_ret(curt, "expected a ; after statement.");
                    s->token_end = curt;
                }break;

            }
        }break;

        case psExpression:{ //-------------------------------------------------------------------------------------Expression
            expect(Token_Identifier){
                //go down expression chain first
                suNode* n = define(node, psConditional);
                if(!n) return 0;
                Expression* e = ExpressionFromNode(n);

                expect(Token_Assignment){
                    Expression* op = arena.make_expression(curt->raw);
                    change_parent(&op->node, &e->node);
                    curt++;
                    suNode* ret = define(&op->node, psExpression);
                    insert_last(node, &op->node);
                    return &op->node;                                                
                }
                return n;
            }else{
                Expression* e = ExpressionFromNode(define(node, psConditional));
                return &e->node;
            }

        }break;

        case psConditional:{ //-----------------------------------------------------------------------------------Conditional
            expect(Token_If){
                NotImplemented;
            }else{
                Expression* e = ExpressionFromNode(define(node, psLogicalOR));
                return &e->node;
            }
        }break;

        case psLogicalOR:{ //---------------------------------------------------------------------------------------LogicalOR
            Expression* e = ExpressionFromNode(define(node, psLogicalAND));
            return binop_parse(node, &e->node, psLogicalAND, Token_OR);
        }break;

        case psLogicalAND:{ //-------------------------------------------------------------------------------------LogicalAND
            Expression* e = ExpressionFromNode(define(node, psBitwiseOR));
            return binop_parse(node, &e->node, psBitwiseOR, Token_AND);
        }break;

        case psBitwiseOR:{ //---------------------------------------------------------------------------------------BitwiseOR
            Expression* e = ExpressionFromNode(define(node, psBitwiseXOR));
            return binop_parse(node, &e->node, psBitwiseXOR, Token_BitOR);
        }break;

        case psBitwiseXOR:{ //-------------------------------------------------------------------------------------BitwiseXOR
            Expression* e = ExpressionFromNode(define(node, psBitwiseAND));
            return binop_parse(node, &e->node, psBitwiseAND, Token_BitXOR);
        }break;

        case psBitwiseAND:{ //-------------------------------------------------------------------------------------BitwiseAND
            Expression* e = ExpressionFromNode(define(node, psEquality));
            return binop_parse(node, &e->node, psEquality, Token_BitAND);
        }break;

        case psEquality:{ //-----------------------------------------------------------------------------------------Equality
            Expression* e = ExpressionFromNode(define(node, psRelational));
            return binop_parse(node, &e->node, psRelational, Token_NotEqual, Token_Equal);
        }break;

        case psRelational:{ //-------------------------------------------------------------------------------------Relational
            Expression* e = ExpressionFromNode(define(node, psBitshift));
            return binop_parse(node, &e->node, psBitshift, Token_LessThan, Token_GreaterThan, Token_LessThanOrEqual, Token_GreaterThanOrEqual);
        }break;

        case psBitshift:{ //-----------------------------------------------------------------------------------------Bitshift
            Expression* e = ExpressionFromNode(define(node, psAdditive));
            return binop_parse(node, &e->node, psAdditive, Token_BitShiftLeft, Token_BitShiftRight);
        }break;

        case psAdditive:{ //-----------------------------------------------------------------------------------------Additive
            Expression* e = ExpressionFromNode(define(node, psTerm));
            return binop_parse(node, &e->node, psTerm, Token_Plus, Token_Negation);
        }break;

        case psTerm:{ //-------------------------------------------------------------------------------------------------Term
            Expression* e = ExpressionFromNode(define(node, psAccess));
            return binop_parse(node, &e->node, psFactor, Token_Multiplication, Token_Division, Token_Modulo);
        }break;

        case psAccess:{
            Expression* e = ExpressionFromNode(define(node, psFactor));
            //dont use binop_parse because this stage is special
            while(next_match(Token_Dot)){
                curt++;
                Expression* op = arena.make_expression(curt->raw);
                op->type = Expression_BinaryOpMemberAccess;
                op->token_start = curt-1;

                change_parent(node, &op->node);
                change_parent(&op->node, &e->node);

                curt++;
                expect(Token_Identifier){
                    Expression* rhs = arena.make_expression(curt->raw);
                    rhs->token_start = curt;
                    rhs->token_end = curt;
                    rhs->type = Expression_IdentifierRHS;
                    insert_last(&op->node, &rhs->node);
                    e = op;
                }else perror_ret(curt, "expected an identifier for member access.");
            }
            return &e->node;
        }break;

        case psFactor:{ //---------------------------------------------------------------------------------------------Factor
            switch(curt->type){
                case Token_LiteralFloat:{
                    Expression* e = arena.make_expression(curt->raw);
                    e->type = Expression_Literal;
                    e->data.type = Token_Float64;
                    e->data.float64 = curt->f64_val;
                    insert_last(node, &e->node);
                    return &e->node;
                }break;

                case Token_LiteralInteger:{
                    Expression* e = arena.make_expression(curt->raw);
                    e->type = Expression_Literal;
                    e->data.type = Token_Signed64;
                    e->data.int64 = curt->s64_val;
                    insert_last(node, &e->node);
                    return &e->node;
                }break;

                case Token_OpenParen:{
                    curt++;
                    Token* start = curt;
                    suNode* ret = define(node, psExpression);
                    curt++;
                    expect(Token_CloseParen){
                        return ret;
                    }else perror(curt, "expected a ) to close ( in line ", start->l0, " on column ", start->c0);
                }break;

                case Token_Identifier:{
                    Expression* e = arena.make_expression(curt->raw);
                    expect_next(Token_OpenParen){
                        //this must be a function call
                        e->type = Expression_FunctionCall;
                        curt+=2;
                        NotImplemented;
                    }else{
                        //this is just some identifier
                        e->type = Expression_IdentifierRHS;
                        insert_last(node, &e->node);
                        expect_next(Token_Increment, Token_Decrement){
                            curt++;
                            Expression* incdec = arena.make_expression((curt->type == Token_Increment ? STR8("++") : STR8("--")));
                            incdec->type = (curt->type == Token_Increment ? Expression_IncrementPostfix : Expression_DecrementPostfix);
                            insert_last(node, &incdec->node);
                            change_parent(&incdec->node, &e->node);
                            return &incdec->node;
                        }
                    }
                    return &e->node;
                }break;

                
            }
        }break;
    }


    return 0;
}

void Parser::parse(){DPZoneScoped;
    Stopwatch time = start_stopwatch();
    sufile->logger.log(Verbosity_Stages, "Parsing...");

    threads = array<ParserThread>(deshi_allocator);
    pending_globals.init();

   
    sufile->logger.log(Verbosity_StageParts, "Checking that imported files are parsed");

    {// make sure that all imported modules have been parsed before parsing this one
     // then gather the defintions from them that this file wants to use
        forI(sufile->lexer.imports.count){
            Token* itok = sufile->lexer.tokens.readptr(sufile->lexer.imports[i]);
            if(!itok->scope_depth){
                //dummy thread, this shouldnt actually be concurrent
                ParserThread pt;
                pt.curt = itok;
                pt.sufile = sufile;
                pt.parser = this;
                pt.define(&sufile->parser.base, psDirective);
            }
        }
        
        forI(sufile->preprocessor.imported_files.count){
            if(sufile->preprocessor.imported_files[i]->stage < FileStage_Parser){
                compiler.logger.warn("INTERNAL: somehow a file wasnt parsed after the initial check above ", __FILENAME__, ", line ", __LINE__);
            }
        }
    }

    threads.reserve(sufile->preprocessor.internal_decl.count+sufile->preprocessor.exported_decl.count);

    for(u32 idx : sufile->preprocessor.internal_decl){
        threads.add(ParserThread());
        ParserThread* pt = threads.last;
        pt->cv.init();
        pt->curt = sufile->lexer.tokens.readptr(idx);
        pt->parser = this;
        pt->node = &sufile->parser.base;
        pt->stage = psDeclaration;
        pt->is_internal = 1;
        // switch(sufile->lexer.tokens[idx].decl_type){
        //     case Declaration_Function:{
        //         Function* f = arena.make_function(pt->curt->raw);
        //         f->decl.working_thread = pt;
        //         str8 fname = suStr8(sufile->lexer.tokens[idx].raw, sufile->lexer.tokens[idx].l0, sufile->lexer.tokens[idx].c0);
        //         pending_globals.add(fname, &f->decl);
        //     }break;
        //     case Declaration_Variable:{
        //         Variable* v = arena.make_variable(pt->curt->raw);
        //         v->decl.working_thread = pt;
        //         sufile->parser.internal_decl.add(sufile->lexer.tokens[idx].raw, &v->decl);
        //     }break;
        //     case Declaration_Structure:{
        //         Struct* s = arena.make_struct(pt->curt->raw);
        //         s->decl.working_thread = pt;
        //         sufile->parser.internal_decl.add(sufile->lexer.tokens[idx].raw, &s->decl);
        //     }break;
        // }
        DeshThreadManager->add_job({&parse_threaded_stub, pt});
    }

    for(u32 idx : sufile->preprocessor.exported_decl){
        threads.add(ParserThread());
        ParserThread* pt = threads.last;
        pt->cv.init();
        pt->curt = sufile->lexer.tokens.readptr(idx);
        pt->parser = this;
        pt->node = &sufile->parser.base;
        pt->stage = psDeclaration;
        pt->is_internal = 0;
        //TODO(sushi) this is probably where we want to look out for variable/struct name conflicts 
        //            functions dont matter because of overloading
        // switch(sufile->lexer.tokens[idx].decl_type){
        //     case Declaration_Function:{
        //         Function* f = arena.make_function(pt->curt->raw);
        //         f->decl.working_thread = pt;
        //         f->decl.type = Declaration_Function;
        //         //because we support overloaded functions we cant just store a function in this 
        //         //map by name. we also dont want to parse the function for its signature here either
        //         //so we store its name followed by its line and column number
        //         //like so:  main10
        //         str8 fname = suStr8(sufile->lexer.tokens[idx].raw, sufile->lexer.tokens[idx].l0, sufile->lexer.tokens[idx].c0);
        //         pending_globals.add(fname, &f->decl);
        //     }break;
        //     case Declaration_Variable:{
        //         Variable* v = arena.make_variable(pt->curt->raw);
        //         v->decl.working_thread = pt;
        //         v->decl.type = Declaration_Variable;
        //         pending_globals.add(sufile->lexer.tokens[idx].raw, &v->decl);
        //     }break;
        //     case Declaration_Structure:{
        //         Struct* s = arena.make_struct(pt->curt->raw);
        //         s->decl.working_thread = pt;
        //         s->decl.type = Declaration_Structure;
        //         pending_globals.add(sufile->lexer.tokens[idx].raw, &s->decl);
        //     }break;
        // }
        DeshThreadManager->add_job({&parse_threaded_stub, pt});
    }

    DeshThreadManager->wake_threads();

    forI(threads.count){
        while(!threads[i].finished){
            threads[i].cv.wait();
        }
    }

    forI(sufile->parser.exported_decl.data.count){
        change_parent(&sufile->parser.base, &sufile->parser.exported_decl.atIdx(i)->node);
        move_to_parent_first(&sufile->parser.exported_decl.atIdx(i)->node);

    }
    forI(sufile->parser.imported_decl.data.count){
        change_parent(&sufile->parser.base, &sufile->parser.imported_decl.atIdx(i)->node);
        move_to_parent_first(&sufile->parser.imported_decl.atIdx(i)->node);
    }

    forI(sufile->parser.internal_decl.data.count){
        change_parent(&sufile->parser.base, &sufile->parser.internal_decl.atIdx(i)->node);
        move_to_parent_first(&sufile->parser.internal_decl.atIdx(i)->node);
    }

        
    sufile->logger.log(Verbosity_Stages, VTS_GreenFg, "Finished parsing in ", peek_stopwatch(time), " ms", VTS_Default);
}