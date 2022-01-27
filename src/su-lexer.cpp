enum LexerState_ {
	DiscernChar,
	ReadingString,
	ReadingStringLiteral,
}; typedef u32 LexerState;

LexerState state = DiscernChar;

array<u32> idindexes;
array<u32> globalids;
array<cstring> structnames;

b32 lex_file(const string& file, const cstring& filename) {
	cstring buff{file.str, file.count};
	lexer.file_index.add(filename);
	LexedFile& lfile = lexer.file_index[filename];
	u32 lines = 0;
	b32 readingstring = 0;
	b32 grab_structname = 0;
	u32 scope = 0;
	char* chunk_start = 0;
#define chunksiz buff.str - chunk_start
#define chunkcmp chunk_start, chunksiz
#define chunkstr cstring{chunk_start, u32(buff.str - chunk_start)}
#define cstrc(count) cstring{chunk_start, count}
#define cstrb(count) cstring{buff.str, count}
	
	
	while (buff.count) {
		switch (state) {
			case DiscernChar: {
				switch (buff[0]) {
					case '#': { lfile.tokens.add(token{ cstrb(1), Token_Directive,    Token_Directive,        scope, lines }); lfile.preprocessor_tokens.add(lfile.tokens.count-1); advance(&buff); }break;
					case ';': { lfile.tokens.add(token{ cstrb(1), Token_Semicolon,    Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					case '{': { scope++; lfile.tokens.add(token{ cstrb(1), Token_OpenBrace,    Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					case '}': { lfile.tokens.add(token{ cstrb(1), Token_CloseBrace,   Token_ControlCharacter, scope, lines }); advance(&buff); scope--; }break;
					case '(': { lfile.tokens.add(token{ cstrb(1), Token_OpenParen,    Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					case ')': { lfile.tokens.add(token{ cstrb(1), Token_CloseParen,   Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					case ',': { lfile.tokens.add(token{ cstrb(1), Token_Comma,        Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					case '?': { lfile.tokens.add(token{ cstrb(1), Token_QuestionMark, Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					case ':': { lfile.tokens.add(token{ cstrb(1), Token_Colon,        Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					case '.': { lfile.tokens.add(token{ cstrb(1), Token_Dot,          Token_ControlCharacter, scope, lines }); advance(&buff); }break;
					

					case '+': {
						if      (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2),Token_PlusAssignment, Token_Operator, scope, lines }); advance(&buff, 2); }
						else if (buff[1] == '+') { lfile.tokens.add(token{ cstrb(2),Token_Increment,      Token_Operator, scope, lines }); advance(&buff, 2); }
						else                     { lfile.tokens.add(token{ cstrb(1),Token_Plus,           Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '-': {
						if      (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_NegationAssignment, Token_Operator, scope, lines }); advance(&buff, 2); }
						else if (buff[1] == '-') { lfile.tokens.add(token{ cstrb(2), Token_Decrememnt,         Token_Operator, scope, lines }); advance(&buff, 2); }
						else                     { lfile.tokens.add(token{ cstrb(1), Token_Negation,           Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '*': {
						if (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_MultiplicationAssignment, Token_Operator, scope, lines }); advance(&buff, 2); }
						else                { lfile.tokens.add(token{ cstrb(1), Token_Multiplication,           Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '/': {
						if (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_DivisionAssignment, Token_Operator, scope, lines }); advance(&buff, 2); }
						else                { lfile.tokens.add(token{ cstrb(1), Token_Division,           Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '~': {
						lfile.tokens.add(token{ cstrb(1), Token_BitNOT,           Token_Operator, scope, lines }); advance(&buff); 
					}break;
					case '&': {
						if      (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_BitANDAssignment, Token_Operator, scope, lines }); advance(&buff, 2); }
						else if (buff[1] == '&') { lfile.tokens.add(token{ cstrb(2), Token_AND,              Token_Operator, scope, lines }); advance(&buff, 2); }
						else                     { lfile.tokens.add(token{ cstrb(1), Token_BitAND,           Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '|': {
						if      (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_BitORAssignment, Token_Operator, scope, lines }); advance(&buff, 2); }
						else if (buff[1] == '|') { lfile.tokens.add(token{ cstrb(2), Token_OR,              Token_Operator, scope, lines }); advance(&buff, 2); }
						else                     { lfile.tokens.add(token{ cstrb(1), Token_BitOR,           Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '^': {
						if (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_BitXORAssignment, Token_Operator, scope, lines }); advance(&buff, 2); }
						else                { lfile.tokens.add(token{ cstrb(1), Token_BitXOR,           Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '%': {
						if (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_Equal,  Token_Operator, scope, lines }); advance(&buff, 2); }
						else                { lfile.tokens.add(token{ cstrb(1), Token_Modulo, Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '=': {
						if (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_Equal,      Token_Operator, scope, lines }); advance(&buff, 2); }
						else                { lfile.tokens.add(token{ cstrb(1), Token_Assignment, Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '!': {
						if (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_NotEqual,   Token_Operator, scope, lines }); advance(&buff, 2); }
						else                { lfile.tokens.add(token{ cstrb(1), Token_LogicalNOT, Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '<': {
						if      (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_LessThanOrEqual, Token_Operator, scope, lines }); advance(&buff, 2); }
						else if (buff[1] == '<') { 
							if(buff[2] == '='){ lfile.tokens.add(token{ cstrb(3), Token_BitShiftLeftAssignment,    Token_Operator, scope, lines }); advance(&buff, 3); }
							else { lfile.tokens.add(token{ cstrb(2), Token_BitShiftLeft,    Token_Operator, scope, lines }); advance(&buff, 2); }
						}
						else                     { lfile.tokens.add(token{ cstrb(1), Token_LessThan,        Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '>': {
						if      (buff[1] == '=') { lfile.tokens.add(token{ cstrb(2), Token_GreaterThanOrEqual, Token_Operator, scope, lines }); advance(&buff, 2); }
						else if (buff[1] == '>') { 
							if(buff[2] == '=') { lfile.tokens.add(token{ cstrb(3), Token_BitShiftRightAssignment,      Token_Operator, scope, lines }); advance(&buff, 3); }
							else {lfile.tokens.add(token{ cstrb(2), Token_BitShiftRight,      Token_Operator, scope, lines }); advance(&buff, 2); }}
						else                     { lfile.tokens.add(token{ cstrb(1), Token_GreaterThan,        Token_Operator, scope, lines }); advance(&buff); }
					}break;
					case '\n': { lines++; advance(&buff); }break;
					case '\t':
					case ' ': { advance(&buff); }break;
					case '"': {
						state = ReadingStringLiteral;
						advance(&buff);
						chunk_start = buff.str;
					}break;
					default: { //if this char is none of the above cases we start looking for a worded token
						state = ReadingString;
						chunk_start = buff.str;
						advance(&buff);
					}break;
				}
			}break;
			
			case ReadingString: {
				switch (buff[0]) {
					case ';': case ' ': case '{': case '}': case '(': case ')':
					case ',': case '+': case '*': case '/': case '-': case '<': 
					case '=': case '!': case '~': case '&': case '|': case '\t':
					case '%': case ':': case '?': case '>': case '^': case '\n': {
						state = DiscernChar;
						if (isalpha(*chunk_start)) {
							if      (!strncmp("return",   chunk_start, 6)) { lfile.tokens.add(token{ cstrc(6), Token_Return,     Token_ControlKeyword, scope, lines });}
							else if (!strncmp("if",       chunk_start, 2)) { lfile.tokens.add(token{ cstrc(2), Token_If,         Token_ControlKeyword, scope, lines });}
							else if (!strncmp("else",     chunk_start, 4)) { lfile.tokens.add(token{ cstrc(4), Token_Else,       Token_ControlKeyword, scope, lines });}
							else if (!strncmp("for",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_For,        Token_ControlKeyword, scope, lines });}
							else if (!strncmp("while",    chunk_start, 5)) { lfile.tokens.add(token{ cstrc(5), Token_While,      Token_ControlKeyword, scope, lines });}
							else if (!strncmp("break",    chunk_start, 5)) { lfile.tokens.add(token{ cstrc(5), Token_Break,      Token_ControlKeyword, scope, lines });}
							else if (!strncmp("continue", chunk_start, 8)) { lfile.tokens.add(token{ cstrc(8), Token_Continue,   Token_ControlKeyword, scope, lines });}
							else if (!strncmp("this",     chunk_start, 4)) { lfile.tokens.add(token{ cstrc(4), Token_This,       Token_This,           scope, lines });}
							else if (!strncmp("struct",   chunk_start, 6)) { lfile.tokens.add(token{ cstrc(6), Token_StructDecl, Token_ControlKeyword, scope, lines }); grab_structname = 1;}
							else if (!strncmp("void",     chunk_start, 4)) { lfile.tokens.add(token{ cstrc(4), Token_Void,       Token_Typename,       scope, lines });}
							else if (!strncmp("s8",       chunk_start, 2)) { lfile.tokens.add(token{ cstrc(2), Token_Signed8,    Token_Typename,       scope, lines });}
							else if (!strncmp("s16",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Signed16,   Token_Typename,       scope, lines });}
							else if (!strncmp("s32",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Signed32,   Token_Typename,       scope, lines });}
							else if (!strncmp("s64",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Signed64,   Token_Typename,       scope, lines });}
							else if (!strncmp("u8",       chunk_start, 2)) { lfile.tokens.add(token{ cstrc(2), Token_Unsigned8,  Token_Typename,       scope, lines });}
							else if (!strncmp("u16",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Unsigned16, Token_Typename,       scope, lines });}
							else if (!strncmp("u32",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Unsigned32, Token_Typename,       scope, lines });}
							else if (!strncmp("u64",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Unsigned64, Token_Typename,       scope, lines });}
							else if (!strncmp("f32",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Float32,    Token_Typename,       scope, lines });}
							else if (!strncmp("f64",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Float64,    Token_Typename,       scope, lines });}
							else if (!strncmp("any",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_Any,        Token_Typename,       scope, lines });}
							else if (!strncmp("str",      chunk_start, 3)) { lfile.tokens.add(token{ cstrc(3), Token_String,     Token_Typename,       scope, lines });}
							else { 
								lfile.tokens.add(token{ chunkstr, Token_Identifier, Token_Identifier, scope, lines });
								if (grab_structname) {
									structnames.add(chunkstr);
									grab_structname = 0;
								}
								
								idindexes.add(lfile.tokens.count - 1);
								if (!lfile.tokens.last->scope) {
									globalids.add(lfile.tokens.count - 1);
								}
								
							}
						}
						else {
							b32 valid = 1;
							b32 isfloat = 0;
							forI(buff.str - chunk_start) { 
								if (chunk_start[i] == '.')
									isfloat = 1;
								else if (!isnumber(chunk_start[i])) 
									valid = 0; 
							}
							if (valid) { 
								lfile.tokens.add(token{ chunkstr, (isfloat ? Token_LiteralFloat : Token_LiteralInteger), Token_Literal, scope, lines });
								if (isfloat) {
									lfile.tokens.last->float64 = stod(chunkstr);
								}
								else {
									lfile.tokens.last->integer = stolli(chunkstr);
								}
							}
							else {
								//lfile.tokens.add(token{ chunkstr, Token_ERROR, lines });
								//PRINTLN("TOKEN ERROR ON TOKEN " << chunkstr);
							}
							// TODO error out of the program here
						}
						
						if (buff[0] == '\n') {
							advance(&buff);
							lines++;
						}
						
					}break;
					
					case '.': {
						if (isalpha(*chunk_start)) {
							//must be member access period (i hope)
							//TODO maybe add checks here that its not a keyword
							if(!strncmp("this", chunk_start, 4)) lfile.tokens.add(token{ cstrc(4), Token_This, Token_This, scope, lines});
							else lfile.tokens.add(token{ chunkstr, Token_Identifier, Token_Identifier, scope, lines });
							lfile.tokens.add(token{ cstr_lit("."), Token_Dot, Token_ControlCharacter, scope, lines });
							state = DiscernChar;
							advance(&buff);
						}
						else { advance(&buff); }//float case handled above
					}break;
					
					default: { advance(&buff); }break;
				}
			}break;
			
			//TODO setup escape character stuff
			case ReadingStringLiteral: {
				switch (buff[0]) {
					case '"': {
						lfile.tokens.add(token{ chunkstr, Token_LiteralString, Token_Literal, scope, lines });
						state = DiscernChar;
						advance(&buff);
					}break;
					case '\n': {
						PRINTLN("unexpected \\n before \" on line " << lines);
						return false;
					}break;
					default: { advance(&buff); }break;
				}
			}break;
		}
	}
	lfile.tokens.add(token{ cstr_lit(""), Token_EOF, Token_EOF, scope, lines});
	
	//not very helpful
	if (scope) logE("lexer", "unbalanced {} somewhere in code");
	
	//iterate over all found identifiers and figure out if they match any known struct names
	//so parser knows when a struct name is being used as a type
	//TODO find a good way to only add identifiers that are in the global scope 
	forI(Max(idindexes.count, globalids.count)) {
		b32 isstruct = 0;
		if (i < idindexes.count && lfile.tokens[idindexes[i] - 1].type != Token_StructDecl) {
			token& t = lfile.tokens[idindexes[i]];
			b32 isstruct = 0;
			for (cstring& str : structnames) {
				if (equals(str, t.str)) {
					t.type = Token_Struct;
					t.group = Token_Typename;
					isstruct = 1;
					break;
				}
			}
			
		}
		if(i < globalids.count) {
			if (lfile.tokens[globalids[i] - 1].type == Token_StructDecl) {
				lfile.struct_decl.add(globalids[i] - 1);
			}
			else if (match_any(lfile.tokens[globalids[i] - 1].group, Token_Typename)) {
				if (lfile.tokens[globalids[i] + 1].type == Token_OpenParen) {
					lfile.func_decl.add(globalids[i] - 1);
				}
				else {
					lfile.var_decl.add(globalids[i] - 1);
				}
			} 
		}
		
	}
	
	return true;
}