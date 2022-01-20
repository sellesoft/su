#pragma once
#ifndef SU_LEXER_H
#define SU_LEXER_H

#include "utils/string.h"
#include "utils/array.h"
#include "utils/defines.h"


enum Token_Type {
    Token_ERROR,                    // when something doesnt make sense during lexing
    Token_EOF,                      // end of file
	
    Token_Identifier,               // function/variable names
    Token_Literal,                  // 1, 2, 3.221, "string", 'c'
	
	//// control characters ////
    Token_Semicolon,                // ;
    Token_OpenBrace,                // {
    Token_CloseBrace,               // }
    Token_OpenParen,                // (
    Token_CloseParen,               // )
    Token_Comma,                    // ,
	Token_QuestionMark,             // ?
    Token_Colon,                    // :
    Token_Dot,                      // .
	
	//// operators ////
    Token_Plus,                     // +
    Token_PlusAssignment,           // +=
    Token_Negation,                 // -
    Token_NegationAssignment,       // -=
    Token_Multiplication,           // *
    Token_MultiplicationAssignment, // *=
    Token_Division,                 // /
    Token_DivisionAssignment,       // /=
    Token_BitNOT,                   // ~
    Token_BitNOTAssignment,         // ~=
    Token_BitAND,                   // &
    Token_BitANDAssignment,         // &=
    Token_AND,                      // &&
    Token_BitOR,                    // |
    Token_BitORAssignment,          // |=
    Token_OR,                       // ||
    Token_BitXOR,                   // ^
    Token_BitXORAssignment,         // ^=
    Token_BitShiftLeft,             // <<
    Token_BitShiftRight,            // >>
    Token_Modulo,                   // %
    Token_ModuloAssignment,         // %=
    Token_Assignment,               // =
    Token_Equal,                    // ==
    Token_LogicalNOT,               // !
    Token_NotEqual,                 // !=
    Token_LessThan,                 // <
    Token_LessThanOrEqual,          // <=
    Token_GreaterThan,              // >
    Token_GreaterThanOrEqual,       // >=
	
	//// control keywords ////
	Token_Return,                   // return
    Token_If,                       // if
    Token_Else,                     // else
	Token_Struct,                   // struct
	
	//// type keywords ////
	Token_Signed32,                 // s32 
    Token_Signed64,                 // s64 
    Token_Unsigned32,               // u32 
    Token_Unsigned64,               // u64 
    Token_Float32,                  // f32 
    Token_Float64,                  // f64 
};

static const char* tokenStrings[] = {
    "ERROR",
	"EOF",
	
    "Identifier",
	"Literal",
	
    "Semicolon",
    "OpenBrace",
    "CloseBrace",
    "OpenParen",
    "CloseParen",
    "Comma",
	"QuestionMark",
    "Colon",
    "Dot",
	
    "Plus",
    "PlusAssignment",
    "Negation",
    "NegationAssignment",
    "Multiplication",
    "MultiplicationAssignment",
    "Division",
    "DivisionAssignment",
    "BitNOT",
    "BitNOTAssignment",
    "BitAND",
    "BitANDAssignment",
    "AND",
    "BitOR",
    "BitORAssignment",
    "OR",
    "BitXOR",
    "BitXORAssignment",
    "BitShiftLeft",
    "BitShiftRight",
    "Modulo",
    "ModuloAssignment",
    "Assignment",
    "Equal",
    "LogicalNOT",
    "NotEqual",
    "LessThan",
    "LessThanOrEqual",
    "GreaterThan",
    "GreaterThanOrEqual",
	
	"Return",
	"If",
	"Else",
	"Struct",
	
	"Signed32",
	"Signed64",
	"Unsigned32",
	"Unsigned64",
	"Float32",
	"Float64",
};

struct token {
	string str;
	Token_Type type; 
	u32 line = 0;
    //string filename = "";
};

namespace suLexer {
	b32 lex(FILE* file, array<token>& tokens);
}

#endif //SU_LEXER_H