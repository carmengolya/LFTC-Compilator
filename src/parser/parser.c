#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "parser.h"

// The parser implements a recursive descent parser for the following grammar:
bool unit();
bool structDef();
bool varDef();
bool typeBase();
bool arrayDecl();
bool fnDef();
bool fnParam();
bool stm();
bool stmCompound();
bool expr();
bool exprAssign();
bool exprOr();
bool exprOrPrim();
bool exprAnd();
bool exprAndPrim();
bool exprEq();
bool exprEqPrim();
bool exprRel();
bool exprRelPrim();
bool exprAdd();
bool exprAddPrim();
bool exprMul();
bool exprMulPrim();
bool exprCast();
bool exprUnary();
bool exprPostfix();
bool exprPostfixPrim();
bool exprPrimary();

Token *iTk;		        // the iterator in the tokens list
Token *consumedTk;		// the last consumed token

void tkerr(const char *fmt,...)
{
	fprintf(stderr,"error in line %d: ",iTk->line);
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
}

bool consume(int code)
{
	printf("[DEBUG] trying to consume token code %d at line %d\n", code, iTk->line);
	if (iTk->code == code)
	{
		consumedTk=iTk;
		iTk = iTk->next;
		return true;
	}
	return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase()
{
	if (consume(TYPE_INT))
	{
		return true;
	}
	if (consume(TYPE_DOUBLE))
	{
		return true;
	}
	if (consume(TYPE_CHAR))
	{
		return true;
	}
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			return true;
		}
	}
	return false;
}

bool structDef()
{
	Token *start = iTk;

	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			if (consume(LACC))
			{
				for (;;)
				{
					if (varDef()) {}
					else break;
				}

				if (consume(RACC))
				{
					if (consume(SEMICOLON))
					{
						return true;
					}
					else
					{
						tkerr("expected ';' after struct definition");
					}
				}
				else
				{
					tkerr("expected '}' after struct body");
				}
			}
			else
			{
				tkerr("expected '{' after struct name");
			}
		}
		else
		{
			tkerr("expected struct name after 'struct'");
		}
	}

	iTk = start;
	return false;
}

bool arrayDecl()
{
	Token *start = iTk;

	if (consume(LBRACKET))
	{	
		if(consume(INT)) {}

		if (consume(RBRACKET))
		{
			return true;
		}
		else
		{
			tkerr("expected ']' after array size");
		}
	}

	iTk = start;
	return false;
}

bool exprPrimary()
{
	if (consume(ID))
	{
		if (consume(LPAR))
		{
			if (expr())
			{
				while (consume(COMMA))
				{
					if (!expr())
					{
						tkerr("syntax error in function call arguments");
					}
				}
			}
			
			if (consume(RPAR))
			{
				return true;
			}
			else
			{
				tkerr("expected ')' after function call arguments");
			}
		}
		else
		{
			return true; // an identifier that is not a function call is a valid primary expression
		}
	}

	if (consume(LPAR))
	{
		if (expr())
		{
			if (consume(RPAR))
			{
				return true;
			}
			else
			{
				tkerr("expected ')' after expression");
			}
		}
		else
		{
			tkerr("syntax error in parenthesized expression");
		}
	}

	if (consume(INT) || consume(CHAR) || consume(STRING) || consume(DOUBLE))
	{
		return true;
	}

	return false;
}

bool exprPostfixPrim()
{
	if (consume(LBRACKET))
		{
			if (expr())
			{
				if (consume(RBRACKET))
				{
					if (exprPostfixPrim())
					{
						return true;
					}
				}
				else
				{
					tkerr("expected ']' after array index");
				}
			}
			else
			{
				tkerr("syntax error in array index expression");
			}
		}
		else if (consume(DOT))
		{
			if (consume(ID))
			{
				if (exprPostfixPrim())
				{
					return true;
				}
			}
			else
			{
				tkerr("expected field name after '.'");
			}
		}
	return true; // epsilon
}

bool exprPostfix()
{
	Token *start = iTk; 

	if (exprPrimary())
	{
		if(exprPostfixPrim())
		{
			return true;
		}
		else
		{
			tkerr("syntax error in postfix expression");
		}
	}

	iTk = start;
	return false;
}

bool exprUnary()
{
	if (consume(NOT) || consume(SUB))
	{
		if (exprUnary())
		{
			return true;
		}
		else
		{
			tkerr("syntax error in unary expression");
		}
	}

	if (exprPostfix())
	{
		return true;
	}

	return false;
}

bool exprCast()
{
	Token *start = iTk;

	if (consume(LPAR))
	{
		if (typeBase())
		{
			if (arrayDecl()) {}
			
			if (consume(RPAR))
			{
				if (exprCast())
				{
					return true;
				}
				else
				{
					tkerr("syntax error in cast expression");
				}
			}
			else
			{
				tkerr("expected ')' after type in cast expression");
			}
		}
	}
	
	iTk = start;

	if(exprUnary())
	{
		return true;
	}

	return false;
}

bool exprMulPrim()
{
	if (consume(MUL))
	{
		if (exprCast())
		{
			if (exprMulPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in multiplication expression");
		}
	}
	else if (consume(DIV))
	{
		if (exprCast())
		{
			if (exprMulPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in division expression");
		}
	}

	return true; // epsilon
}

bool exprMul()
{
	Token *start = iTk;

	if (exprCast())
	{
		if(exprMulPrim())
		{
			return true;
		}
	}	

	iTk = start;
	return false;
}

bool exprAddPrim()
{
	if (consume(ADD))
	{
		if (exprMul())
		{
			if (exprAddPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in addition expression");
		}
	}
	else if (consume(SUB))
	{
		if (exprMul())
		{
			if (exprAddPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in subtraction expression");
		}
	}

	return true; // epsilon
}

bool exprAdd()
{
	Token *start = iTk;

	if (exprMul())
	{
		if (exprAdd())
		{
			return true;
		}
	}	

	iTk = start;
	return false;
}

bool exprRelPrim()
{
	if (consume(LESS))
	{
		if (exprAdd())
		{
			if (exprRelPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in less-than expression");
		}
	}
	else if (consume(LESSEQ))
	{
		if (exprAdd())
		{
			if (exprRelPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in less-than-or-equal expression");
		}
	}
	else if (consume(GREATER))
	{
		if (exprAdd())
		{
			if (exprRelPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in greater-than expression");
		}
	}
	else if (consume(GREATEREQ))
	{
		if (exprAdd())
		{
			if (exprRelPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in greater-than-or-equal expression");
		}
	}

	return true; // epsilon
}

bool exprRel()
{
	Token *start = iTk;

	if (exprAdd())
	{
		if (exprRelPrim())
		{
			return true;
		}
	}

	iTk = start;
	return false;
}

bool exprEqPrim()
{
	if (consume(EQUAL))
	{
		if (exprRel())
		{
			if (exprEqPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in equality expression");
		}
	}
	else if (consume(NOTEQ))
	{
		if (exprRel())
		{
			if (exprEqPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in inequality expression");
		}
	}
	
	return true; // epsilon
}

bool exprEq()
{
	Token *start = iTk;

	if (exprRel())
	{
		if (exprEqPrim())
		{
			return true;
		}
	}
	
	iTk = start;
	return false;
}

bool exprAndPrim()
{
	if(consume(AND))
	{
		if (exprEq())
		{
			if (exprAndPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in logical AND expression");
		}
	}

	return true; // epsilon
}

bool exprAnd()
{
	Token *start = iTk;

	if (exprEq())
	{
		if (exprAndPrim())
		{
			return true;
		}
	}
	
	iTk = start;
	return false;
}

bool exprOrPrim()
{
	if(consume(OR))
	{
		if (exprAnd())
		{
			if (exprOrPrim())
			{
				return true;
			}
		}
		else
		{
			tkerr("syntax error in logical OR expression");
		}
	}

	return true; // epsilon
}

bool exprOr()
{
	Token *start = iTk;

	if (exprAnd())
	{
		if (exprOrPrim())
		{
			return true;
		}
	}
	
	iTk = start;
	return false;
}

bool exprAssign()
{
	Token *start = iTk;

	if (exprUnary())
	{
		if (consume(ASSIGN))
		{
			if (exprAssign())
			{
				return true;
			}
			else
			{
				tkerr("syntax error in assignment expression");
			}
		}
	}

	iTk = start;

	if (exprOr())
	{
		return true;
	}

	return false;
}

bool expr()
{
	if (exprAssign())
	{
		return true;
	}

	return false;
}

bool stmCompound()
{
	Token *start = iTk;

	if (consume(LACC))
	{
		for (;;)
		{
			if (varDef()) {}
			else if (stm()) {}
			else break;
		}

		if (consume(RACC))
		{
			return true;
		}
		else
		{
			tkerr("expected '}' to end compound statement");
		}
	}
	else
	{
		tkerr("expected '{' to start compound statement");
	}

	iTk = start;
	return false;
}

bool stm()
{
	Token *start = iTk;

	if (stmCompound())
	{
		return true;
	}

	if (consume(IF))
	{
		if (consume(LPAR))
		{
			if (expr())
			{
				if (consume(RPAR))
				{
					if (stm())
					{
						if (consume(ELSE))
						{
							if (!stm())
							{
								tkerr("syntax error in 'else' statement");
							}
						}
						return true;
					}
					else
					{
						tkerr("syntax error in 'if' statement");
					}
				}
				else
				{
					tkerr("expected ')' after condition in 'if' statement");
				}
			}
			else
			{
				tkerr("syntax error in condition of 'if' statement");
			}
		}
		else
		{
			tkerr("expected '(' after 'if'");
		}
	}

	if (consume(WHILE))
	{
		if (consume(LPAR))
		{
			if (expr())
			{
				if (consume(RPAR))
				{
					if (stm())
					{
						return true;
					}
					else
					{
						tkerr("syntax error in 'while' statement");
					}
				}
				else
				{
					tkerr("expected ')' after condition in 'while' statement");
				}
			}
			else
			{
				tkerr("syntax error in condition of 'while' statement");
			}
		}
		else
		{
			tkerr("expected '(' after 'while'");
		}
	}

	if (consume(RETURN))
	{
		if (expr()) {}
		
		if (consume(SEMICOLON))
		{
			return true;
		}
		else
		{
			tkerr("expected ';' after return statement");
		}
	}

	if (expr())
	{
		if (consume(SEMICOLON))
		{
			return true;
		}
		else
		{
			tkerr("expected ';' after expression statement");
		}
	}

	if (consume(SEMICOLON))
	{
		return true;
	}
	else
	{
		tkerr("syntax error in statement");
	}

	iTk = start;
	return false;
}

bool fnParam()
{
	if (typeBase())
	{
		if (consume(ID))
		{
			if (arrayDecl())
			{
				return true;
			}
		}
		else
		{
			tkerr("expected parameter name after type");
		}
	}
	return false;
}

bool fnDef()
{
	Token *start = iTk;

	if (typeBase() || consume(VOID))
	{
		if (consume(ID))
		{
			if (consume(LPAR))
			{
				if (fnParam())
				{
					while (consume(COMMA))
					{
						if (!fnParam())
						{
							tkerr("syntax error in function parameters");
						}
					}
				}
				
				if (consume(RPAR))
				{
					if (stmCompound())
					{
						return true;
					}
					else
					{
						tkerr("syntax error in function body");
					}
				}
				else
				{
					tkerr("expected ')' after function parameters");
				}
			}
		}
	}

	iTk = start;
	return false;
}

bool varDef()
{
	Token *start = iTk;

	if (typeBase())
	{
		if (consume(ID))
		{
			if(arrayDecl()) {}
			
			if(consume(SEMICOLON))
			{
				return true;
			}
			else
			{
				tkerr("expected ';' after variable declaration");
			}
		}
		else
		{
			tkerr("expected variable name after type");
		}
	}

	iTk = start;
	return false;
}

// unit: ( structDef | fnDef | varDef )* END
bool unit()
{
	for (;;)
	{
		if (structDef()) {}
		else if (fnDef()) {}
		else if (varDef()) {}
		else break;
	}
	if (consume(END))
	{
		return true;
	}
	return false;
}

void parse(Token *tokens)
{
	iTk = tokens;
	if (!unit()) tkerr("syntax error");
}
