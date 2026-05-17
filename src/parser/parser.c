#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>

#include "domain/ad.h"
#include "lexer/lexer.h"
#include "parser.h"
#include "utils/utils.h"

// The parser implements a recursive descent parser for the following grammar:
bool unit();
bool structDef();
bool varDef();
bool typeBase(Type *t);         // AD: primeste Type *t - atribut sintetizat pentru tipul de baza
bool arrayDecl(Type *t);        // AD: primeste Type *t - atribut inout pentru dimensiunea vectorului
bool fnDef();
bool fnParam();
bool stm();
bool stmCompound(bool newDomain); // AD: primeste bool newDomain - daca true, creeaza un domeniu nou
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
	int errLine = consumedTk ? consumedTk->line : 0;
	fprintf(stderr,"error in line %d: ", errLine);
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
}

bool consume(int code)
{
	if (iTk->code == code)
	{
		consumedTk = iTk;
		iTk = iTk->next;
		return true;
	}
	return false;
}

// typeBase[out Type *t]: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
// AD: sets the base type in t->tb
// AD: for STRUCT, verifies that the structure was defined earlier and sets t->s
bool typeBase(Type *t)
{
	t->n = -1; // AD: implicit, the symbol is not an array (n<0 means scalar)

	if (consume(TYPE_INT))
	{
		t->tb = TB_INT; // AD: base type int
		return true;
	}
	if (consume(TYPE_DOUBLE))
	{
		t->tb = TB_DOUBLE; // AD: base type double
		return true;
	}
	if (consume(TYPE_CHAR))
	{
		t->tb = TB_CHAR; // AD: base type char
		return true;
	}
	if (consume(STRUCT))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk; // AD: retains the token with the structure name
            t->tb = TB_STRUCT;          // AD: base type struct
            t->s = findSymbol(tkName->text); // AD: searches for the structure in all domains (must be defined earlier in the same or parent domain)
            if (!t->s) tkerr("structure not defined: %s", tkName->text); // AD: error if the structure does not exist
			return true;
		}
		else
		{
			tkerr("expected struct name after 'struct'");
		}
	}
	return false;
}

// structDef: STRUCT ID[tkName] LACC ( varDef )* RACC SEMICOLON
// AD: adds the structure to the current domain, creates a new domain for its members
bool structDef()
{
    Token *start = iTk;

    if (consume(STRUCT))
    {
        if (consume(ID))
        {
			Token *tkName = consumedTk; // AD: retains the token with the structure name for adding it to the symbol table

            if (consume(LACC))
            {
				// AD: verifies that the structure name is not already defined in the current domain
				Symbol *s = findSymbolInDomain(symTable, tkName->text);
				if (s) tkerr("symbol redefinition: %s", tkName->text);
				
				// AD: creates a symbol of type struct and adds it to the current domain
				s = addSymbolToDomain(symTable, newSymbol(tkName->text, SK_STRUCT));
				s->type.tb = TB_STRUCT;
				s->type.s = s;   // AD: structure refers to itself (necessary for typeSize and recursive structures)
				s->type.n = -1;  // AD: structure is not an array
				
				pushDomain(); // AD: creates a new domain for the structure's members
				owner = s;    // AD: sets the owner to the current structure, used in varDef for SK_STRUCT

                while(varDef()) {} // AD: parses the members of the structure; each varDef adds them to structMembers

                if (consume(RACC))
                {
                    if (consume(SEMICOLON))
                    {
						owner = NULL; // AD: leave the structure, owner becomes NULL again (we are back in the global/parent domain)
						dropDomain(); // AD: erase the structure domain (members have been copied to structMembers)

                        return true;
                    }
                    else 
					{
						tkerr("expected ';' after struct definition");
					}
                }
                else 
				{
					tkerr("expected '}' after struct body, but got '%s'", iTk->text);
				}
            }
        }
    }

    iTk = start;
    return false;
}

// arrayDecl[inout Type *t]: LBRACKET ( INT )? RBRACKET
// AD: if it has a size, set t->n = its value (e.g., int v[10] -> t->n=10)
// AD: if it doesn't have a size, set t->n = 0 (e.g., int v[] -> t->n=0)
bool arrayDecl(Type *t)
{
	Token *start = iTk;

	if (consume(LBRACKET))
	{	
		if(consume(INT)) 
		{
			Token *tkSize = consumedTk; // AD: retains the token with the array size
            t->n = tkSize->i;           // AD: sets the size of the array in the type
		}
		else
		{
			t->n = 0; // AD: array without specified size (e.g., function parameter int v[])
		}

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

// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )?
//            | LPAR expr RPAR
//            | INT | CHAR | STRING | DOUBLE
bool exprPrimary()
{
	Token *start = iTk;
	
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

		return true;
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
	}

	if (consume(INT) || consume(CHAR) || consume(STRING) || consume(DOUBLE))
	{
		return true;
	}

	iTk = start;
	return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim
//               | DOT ID exprPostfixPrim
//			     | epsilon
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
	}

	iTk = start;
	return false;
}

// exprUnary: ( NOT | SUB ) exprUnary | exprPostfix
bool exprUnary()
{
	Token *start = iTk;
	if (consume(NOT) || consume(SUB))
	{
		if (exprUnary())
		{
			return true;
		}
		else
		{
			tkerr("syntax error in unary expression: expected expression after operator");
		}
	}

	iTk = start;

	if (exprPostfix())
	{
		return true;
	}

	return false;
}

// exprCast: LPAR typeBase[&t] arrayDecl[&t]? RPAR exprCast | exprUnary
// AD: t is declared locally - necessary to be able to call typeBase and arrayDecl with the new signature
bool exprCast()
{
	Token *start = iTk;
	Type t; // AD: the type used in the cast; must be declared to be able to call typeBase(&t) and arrayDecl(&t)

	if (consume(LPAR))
	{
		if (typeBase(&t)) // AD: consumes the base type and sets it in t
		{
			if (arrayDecl(&t)) {} // AD: optional - if [] exists, set t.n
			
			if (consume(RPAR))
			{
				if (exprCast())
				{
					return true;
				}
				else
				{
					tkerr("syntax error in cast expression: expected expression after ')'");
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

// exprMulPrim: ( MUL | DIV ) exprCast exprMulPrim | epsilon
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
			tkerr("syntax error in multiplication expression: expected expression after '*'");
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
			tkerr("syntax error in division expression: expected expression after '/'");
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

// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim | epsilon
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
			tkerr("syntax error in addition expression: expected expression after '+'");
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
			tkerr("syntax error in subtraction expression: expected expression after '-'");
		}
	}

	return true; // epsilon
}

bool exprAdd()
{
	Token *start = iTk;

	if (exprMul())
	{
		if (exprAddPrim())
		{
			return true;
		}
	}	

	iTk = start;
	return false;
}

// exprRelPrim: ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim | epsilon
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
			tkerr("syntax error in less-than expression: expected expression after '<'");
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
			tkerr("syntax error in less-than-or-equal expression: expected expression after '<='");
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
			tkerr("syntax error in greater-than expression: expected expression after '>'");
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
			tkerr("syntax error in greater-than-or-equal expression: expected expression after '>='");
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

// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim | epsilon
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
			tkerr("syntax error in equality expression: expected expression after '=='");
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
			tkerr("syntax error in inequality expression: expected expression after '!='");
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

// exprAndPrim: ( AND ) exprEq exprAndPrim | epsilon
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
			tkerr("syntax error in logical AND expression: expected expression after '&&'");
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

// exprOrPrim: ( OR ) exprAnd exprOrPrim | epsilon
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
			tkerr("syntax error in logical OR expression: expected expression after '||'");
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

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
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
				tkerr("syntax error in assignment expression: expected expression after '='");
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

// expr: exprAssign
bool expr()
{
	if (exprAssign())
	{
		return true;
	}

	return false;
}

// stmCompound[in bool newDomain]: LACC ( varDef | stm )* RACC
// AD: if newDomain == true, then a new domain is created at entry and deleted at exit
// AD: called with true from stm() -> the if/while/else blocks have their own domain
// AD: called with false from fnDef() -> the function body does not create an additional subdomain
//     (the function's domain was already created by fnDef before LPAR)
bool stmCompound(bool newDomain)
{
    Token *start = iTk;
    if (consume(LACC))
    {
        if (newDomain) pushDomain(); // AD: creates new domain for this compound block

        for (;;)
        {
            if (varDef()) {}      // AD: variables declared here belong to the current domain
            else if (stm()) {}
            else break;
        }

        if (consume(RACC))
        {
            if (newDomain) dropDomain(); // AD: erases the domain of the compound block and all symbols in it
            return true;
        }
        else tkerr("expected '}'");
    }
    iTk = start;
    return false;
}

// stm: stmCompound[true]
//    | IF LPAR expr RPAR stm ( ELSE stm )?
//    | WHILE LPAR expr RPAR stm
//    | RETURN expr? SEMICOLON
//    | expr? SEMICOLON
bool stm()
{
	Token *start = iTk;

	if (stmCompound(true)) // AD: compound block from stm -> creates new domain for the block
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

	iTk = start;
	return false;
}

// fnParam: typeBase[&t] ID[tkName] ( arrayDecl[&t] )?
// AD: adds the parameter to the current domain (local domain of the function)
// AD: adds a copy of the parameter to the function's parameter list (owner)
// AD: parameters declared as arrays with size (e.g., int v[10]) lose their size -> treated as pointers (t.n=0)
bool fnParam()
{
    Token *start = iTk;
    Type t; // AD: parameter type, completed by typeBase and arrayDecl

    if (typeBase(&t)) // AD: consumes the base type and sets it in t
    {
        if (consume(ID))
        {
            Token *tkName = consumedTk; // AD: remembers the token with the parameter name

            if (arrayDecl(&t)) // AD: optional - if [] exists, sets t.n
			{
				t.n = 0; // AD: the vector parameters lose their size (int v[10] becomes int v[])
			}
            
            // AD: verifies redefinition in the current domain (local domain of the function)
            Symbol *param = findSymbolInDomain(symTable, tkName->text);
            if (param) tkerr("symbol redefinition: %s", tkName->text);

            // AD: creates symbol of type parameter and completes the fields
            param = newSymbol(tkName->text, SK_PARAM);
            param->type = t;
            param->owner = owner;                          // AD: owner is the current function
            param->paramIdx = symbolsLen(owner->fn.params); // AD: the index of the parameter in the function's parameter list

            // AD: the parameter is added to both the current domain and the function's parameter list
            addSymbolToDomain(symTable, param);
            addSymbolToList(&owner->fn.params, dupSymbol(param)); // AD: copy in fn.params to survive dropDomain

            return true;
        }
        tkerr("expected parameter name after type");
    }

    iTk = start;
    return false;
}

// fnDef: ( typeBase[&t] | VOID ) ID[tkName] LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound[false]
// AD: adds the function to the current domain
// AD: creates a new domain for parameters and local variables (immediately after LPAR)
// AD: the function body {...} does not create an additional subdomain (stmCompound called with false)
bool fnDef()
{
    Token *start = iTk;
    Type t; // AD: the returned type of the function, completed by typeBase or set manually for VOID

	bool hasType = false;
	if (typeBase(&t)) // AD: tries to consume a base type (int, double, char, struct X)
	{
		hasType = true;
	}
	else if (consume(VOID))
	{
		// AD: void function - sets the type manually because VOID is not covered by typeBase
		t.tb = TB_VOID;
		t.n = -1;
		t.s = NULL;
		hasType = true;
	}

	if (!hasType)
	{
		iTk = start; // AD: it is not a function definition, reset the position
		return false;
	}

    if (hasType)
    {
        if (consume(ID))
        {
            Token *tkName = consumedTk; // AD: remembers the token with the function name

            if (consume(LPAR))
            {
				// AD: verifies that the function name is not already defined in the current domain
				Symbol *fn = findSymbolInDomain(symTable, tkName->text);
				if (fn) tkerr("symbol redefinition: %s", tkName->text);

				// AD: creates the function symbol, sets the return type and adds it to the current domain
				fn = newSymbol(tkName->text, SK_FN);
				fn->type = t;
				addSymbolToDomain(symTable, fn);

				owner = fn;   // AD: sets the owner to the current function (used in fnParam and varDef)
				pushDomain(); // AD: creates the local domain of the function (for parameters and local variables)


                if (fnParam()) // AD: parses the first parameter (if it exists)
                {
                    while (consume(COMMA))
                    {
                        if (!fnParam()) tkerr("expected parameter after ','");
                    }
                }
                if (consume(RPAR))
                {
                    // AD: stmCompound(false) - body of the function does not create a new subdomain
                    // AD: function domain (created above with pushDomain) covers both parameters and body
                    if (stmCompound(false))
                    {
                        dropDomain(); // AD: erases the local domain of the function
                        owner = NULL; // AD: leaving the function, owner becomes NULL again (global scope)
                        return true;
                    }
                    else tkerr("expected '{' for function body");
                }
                else tkerr("expected ')' after function parameters");
            }
        }
    }

    iTk = start;
    return false;
}

// varDef: typeBase[&t] ID[tkName] ( arrayDecl[&t] )? SEMICOLON
// AD: adds the variable to the current domain and to the owner's structure (if it exists)
// AD: for global variables (owner==NULL), allocates memory for their values
// AD: for local variables of a function, sets varIdx and adds to fn.locals
// AD: for members of a structure, sets varIdx (offset) and adds to structMembers
bool varDef()
{
	Token *start = iTk;
	Type t; // AD: the type of the variable, completed by typeBase and arrayDecl

	if (typeBase(&t)) // AD: consumes the base type and sets it in t
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk; // AD: remembers the token with the variable name

			if(arrayDecl(&t)) // AD: optional - if [] exists, sets t.n
			{
				// AD: vectors in varDef have to have a specified dimension (int v[] is invalid as a variable)
				if (t.n == 0) tkerr("a vector variable must have a specified dimension");
			}
			
			if(consume(SEMICOLON))
			{
				// AD: verifies that the variable name is not already defined in the current domain
				Symbol *var = findSymbolInDomain(symTable, tkName->text);
                if (var) tkerr("symbol redefinition: %s", tkName->text);

                // AD: creates the variable symbol and sets its type and owner
                var = newSymbol(tkName->text, SK_VAR);
                var->type = t;
                var->owner = owner; // AD: NULL if global, otherwise the function/struct in which it is defined
                addSymbolToDomain(symTable, var); // AD: adds the variable to the current domain

                if (owner)
                {
                    switch (owner->kind)
                    {
                        case SK_FN:
                            // AD: local variable of the function - varIdx = position in the locals list
                            var->varIdx = symbolsLen(owner->fn.locals);
                            // AD: adds a copy in fn.locals for persistence across dropDomain
                            addSymbolToList(&owner->fn.locals, dupSymbol(var));
                            break;
                        case SK_STRUCT:
                            // AD: member of the struct - varIdx = the offset in bytes compared to the beginning of the struct
                            var->varIdx = typeSize(&owner->type);
                            // AD: adds a copy in structMembers for persistence across dropDomain
                            addSymbolToList(&owner->structMembers, dupSymbol(var));
                            break;
						default: break; // AD: SK_VAR and/or SK_PARAM cannot be owners, so this default case should not be hit
                    }
                }
                else
                {
                    // AD: global variable - allocates memory for the variable value
                    var->varMem = safeAlloc(typeSize(&t));
                }

				return true;
			}
			else
			{
				tkerr("expected ';' after variable declaration");
			}
		}
		else
		{
			tkerr("expected variable name after type or fnDef or '{' from struct definition");
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

	tkerr("unexpected token after end of program");
	return false;
}

void parse(Token *tokens)
{
	iTk = tokens;
	if (!unit()) tkerr("syntax error");
}
