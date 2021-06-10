#include "su-lexer.h"
#include "utils/misc.h"

#define MAXBUFF 255

array<char> stopping_chars{
	';', ' ', '{',  '}', '\(', '\)', ',', '+', '-', '<', '>', '=', '\n'
};

array<string> keywords{
	"int", "return"
};

array<token> suLexer::lex(FILE* file) {
	array<token> tokens;
	char currChar = 0;
	string buff = "";
	u32 lines = 1;
	
	//TODO get rid of maxbuff here
	while (buff.size + 1 < MAXBUFF) {
		currChar = fgetc(file);
		//check that our current character isn't any 'stopping' characters
		if (is_in(currChar, stopping_chars)) {

			//store both the stopping character and previous buffer as tokens
			//also decide what type of token it is
			if (buff[0]) {
				token t;
				t.str = buff;
				//check if token is a keyword
				if (is_in(buff, keywords)) {
					if (buff == "return")   t.type = tok_Return;
					else if (buff == "int") t.type = tok_Keyword;
				}
				//if its not then it could be a number of other things
				else {
					//TODO make this cleaner
					if (isalpha(buff[0])) {
						//if it begins with a letter it must be an identifier
						//for now
						t.type = tok_Identifier;
					}
					else if (isdigit(buff[0])) {
						//check if its a digit, then verify that the rest are digits
						bool error = false;
						for (int i = 0; i < buff.size; i++) {
							if (!isdigit(buff[i])) error = true;
						}
						if (error) t.type = tok_ERROR;
						else t.type = tok_IntegerLiteral;
					}
				}
				t.line = lines;
				tokens.add(t);
			}
			if (currChar != ' ' && currChar != '\n') {
				token t;
				t.str = currChar;
				switch (currChar) {
					case ';':  t.type = tok_Semicolon; break;
					case '{':  t.type = tok_OpenBrace; break;
					case '}':  t.type = tok_CloseBrace; break;
					case '\(': t.type = tok_OpenParen; break;
					case '\)': t.type = tok_CloseParen; break;
					case ',':  t.type = tok_Comma; break;
					case '+':  t.type = tok_Plus; break;
					case '-':  t.type = tok_Minus; break;
					case '<':  t.type = tok_OpenAngle; break;
					case '>':  t.type = tok_CloseAngle; break;
					case '=':  t.type = tok_Assignment; break;
				}
				t.line = lines;
				tokens.add(t);
			}
			buff.clear();
		}
		else if (currChar == EOF) { 
			token t;
			t.str = "End of File";
			t.type = tok_EOF;
			t.line = lines;
			tokens.add(t);
			break;
		}
		else if (currChar != '\t' && currChar != '\n') {
			//if not then keep adding to buffer if character is not a newline or tab
			buff += currChar;
		}
		if (currChar == '\n') lines++;
	}

	return tokens;
}