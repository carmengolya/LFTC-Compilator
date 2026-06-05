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

bool expr(Ret *r);
bool exprAssign(Ret *r);
bool exprOr(Ret *r);
bool exprOrPrim(Ret *r);
bool exprAnd(Ret *r);
bool exprAndPrim(Ret *r);
bool exprEq(Ret *r);
bool exprEqPrim(Ret *r);
bool exprRel(Ret *r);
bool exprRelPrim(Ret *r);
bool exprAdd(Ret *r);
bool exprAddPrim(Ret *r);
bool exprMul(Ret *r);
bool exprMulPrim(Ret *r);
bool exprCast(Ret *r);
bool exprUnary(Ret *r);
bool exprPostfix(Ret *r);
bool exprPostfixPrim(Ret *r);
bool exprPrimary(Ret *r);

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
bool exprPrimary(Ret *r)
{
	Token *start = iTk;
	
	if (consume(ID))
	{
		Symbol *s = findSymbol(consumedTk->text);
		if (!s) tkerr("undefined id: %s", consumedTk->text);

		if (consume(LPAR))
		{
			if (s->kind != SK_FN) tkerr("only a function can be called");
			
			Ret rArg;
			Symbol *param = s->fn.params;

			if (expr(&rArg))
			{
				if (!param) tkerr("too many arguments in function call");
				if (!convTo(&rArg.type, &param->type)) tkerr("in call, cannot convert the argument type to the parameter type");

				addRVal(&owner->fn.instr,rArg.lval,&rArg.type);
				insertConvIfNeeded(lastInstr(owner->fn.instr),&rArg.type,&param->type);

				param = param->next;

				while (consume(COMMA))
				{
					if (!expr(&rArg))
					{
						tkerr("syntax error in function call arguments");
					}
					
					if (!param) tkerr("too many arguments in function call");
					if (!convTo(&rArg.type, &param->type)) tkerr("in call, cannot convert the argument type to the parameter type");

					addRVal(&owner->fn.instr, rArg.lval, &rArg.type);
					insertConvIfNeeded(lastInstr(owner->fn.instr), &rArg.type, &param->type);

					param = param->next;
				}
			}
			
			if (consume(RPAR))
			{
				if (param) tkerr("too few arguments in function call");

				if (s->fn.extFnPtr)
				{
					addInstr(&owner->fn.instr, OP_CALL_EXT)->arg.extFnPtr = s->fn.extFnPtr;
				}
				else
				{
					addInstr(&owner->fn.instr, OP_CALL)->arg.instr = s->fn.instr;
				}
				
				*r = (Ret){s->type, false, true};
				return true;
			}
			else
			{
				tkerr("expected ')' after function call arguments");
			}
		}

		if (s->kind == SK_FN) tkerr("a function can only be called");

		if (s->kind == SK_VAR)
		{
			if (s->owner == NULL)
			{
				addInstr(&owner->fn.instr, OP_ADDR)->arg.p = s->varMem;
			}
			else
			{
				switch (s->type.tb)
				{
					case TB_INT:    addInstrWithInt(&owner->fn.instr, OP_FPADDR_I, s->varIdx + 1); break;
					case TB_DOUBLE: addInstrWithInt(&owner->fn.instr, OP_FPADDR_F, s->varIdx + 1); break;
				}
			}
		}

		if (s->kind == SK_PARAM)
		{
			switch (s->type.tb)
			{
				case TB_INT:    addInstrWithInt(&owner->fn.instr, OP_FPADDR_I, s->paramIdx - symbolsLen(s->owner->fn.params) - 1); break;
				case TB_DOUBLE: addInstrWithInt(&owner->fn.instr, OP_FPADDR_F, s->paramIdx - symbolsLen(s->owner->fn.params) - 1); break;
			}
		}

		*r = (Ret){s->type, true, s->type.n >= 0};

		return true;
	}

	if (consume(LPAR))
	{
		if (expr(r))
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

	if (consume(INT))
	{
		addInstrWithInt(&owner->fn.instr, OP_PUSH_I, consumedTk->i);
		*r = (Ret){{TB_INT, NULL, -1}, false, true};
		return true;
	}

	if (consume(DOUBLE))
	{
		addInstrWithDouble(&owner->fn.instr, OP_PUSH_F, consumedTk->d);
		*r = (Ret){{TB_DOUBLE, NULL, -1}, false, true};
		return true;
	}

	if (consume(CHAR))
	{
		*r = (Ret){{TB_CHAR, NULL, -1}, false, true};
		return true;
	}

	if (consume(STRING))
	{
		*r = (Ret){{TB_CHAR, NULL, 0}, false, true};
		return true;
	}

	iTk = start;
	return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim
//               | DOT ID exprPostfixPrim
//			     | epsilon
bool exprPostfixPrim(Ret *r)
{
	if (consume(LBRACKET))
	{
		Ret idx;
		if (expr(&idx))
		{
			if (consume(RBRACKET))
			{
				if (r->type.n < 0) tkerr("only an array can be indexed");
				Type tInt = {TB_INT, NULL, -1};
				if (!convTo(&idx.type, &tInt)) tkerr("the index is not convertible to int");
				r->type.n = -1;
				r->lval = true;
				r->ct = false;

				if (exprPostfixPrim(r))
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
			if(r->type.tb != TB_STRUCT) tkerr("a field can only be selected from a struct");
			
			Symbol *s = findSymbolInList(r->type.s->structMembers, consumedTk->text);
			if(!s) tkerr("the structure %s does not have a field %s", r->type.s->name, consumedTk->text);
			*r = (Ret){s->type, true, s->type.n >= 0};
			
			if (exprPostfixPrim(r))
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

bool exprPostfix(Ret *r)
{
	Token *start = iTk; 

	if (exprPrimary(r))
	{
		if(exprPostfixPrim(r))
		{
			return true;
		}
	}

	iTk = start;
	return false;
}

// exprUnary: ( NOT | SUB ) exprUnary | exprPostfix
bool exprUnary(Ret *r)
{
	Token *start = iTk;
	if (consume(NOT) || consume(SUB))
	{
		if (exprUnary(r))
		{
			if (!canBeScalar(r)) tkerr("unary - or ! must have a scalar operand");
			r->lval=false;
			r->ct=true;

			return true;
		}
		else
		{
			tkerr("syntax error in unary expression: expected expression after operator");
		}
	}

	iTk = start;

	if (exprPostfix(r))
	{
		return true;
	}

	return false;
}

// exprCast: LPAR typeBase[&t] arrayDecl[&t]? RPAR exprCast | exprUnary
// AD: t is declared locally - necessary to be able to call typeBase and arrayDecl with the new signature
bool exprCast(Ret *r)
{
	Token *start = iTk;

	if (consume(LPAR))
	{
		Type t; // AD: the type used in the cast; must be declared to be able to call typeBase(&t) and arrayDecl(&t)
		Ret op;
		if (typeBase(&t)) // AD: consumes the base type and sets it in t
		{
			if (arrayDecl(&t)) {} // AD: optional - if [] exists, set t.n
			
			if (consume(RPAR))
			{
				if (exprCast(&op))
				{
					if(t.tb == TB_STRUCT)		  tkerr("cannot convert to a struct type");
					if(op.type.tb == TB_STRUCT)   tkerr("cannot convert a struct");
					if(op.type.n >= 0 && t.n < 0) tkerr("an array can be converted only to another array");
					if(op.type.n < 0 && t.n >= 0) tkerr("a scalar can be converted only to another scalar");
					*r = (Ret){t, false, true};
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

	if(exprUnary(r))
	{
		return true;
	}

	return false;
}

// exprMulPrim: ( MUL | DIV ) exprCast exprMulPrim | epsilon
bool exprMulPrim(Ret *r)
{
	Token *op;

	if (consume(MUL))
	{
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprCast(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for *");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (tDst.tb)
			{
				case TB_INT:	addInstr(&owner->fn.instr, OP_MUL_I); break;
				case TB_DOUBLE: addInstr(&owner->fn.instr, OP_MUL_F); break;
			}
			// *r = (Ret){tDst, false, true};

			if (exprMulPrim(r))
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
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprCast(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for /");

			addRVal(&owner->fn.instr,right.lval,&right.type);
			insertConvIfNeeded(lastLeft,&r->type,&tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
			
			switch (tDst.tb)
			{
				case TB_INT:	addInstr(&owner->fn.instr, OP_DIV_I); break;
				case TB_DOUBLE: addInstr(&owner->fn.instr, OP_DIV_F); break;
			}

			// *r = (Ret){tDst, false, true};

			if (exprMulPrim(r))
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

bool exprMul(Ret *r)
{
	Token *start = iTk;

	if (exprCast(r))
	{
		if(exprMulPrim(r))
		{
			return true;
		}
	}	

	iTk = start;
	return false;
}

// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim | epsilon
bool exprAddPrim(Ret *r)
{
	Token *op;

	if (consume(ADD))
	{
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprMul(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for +");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (tDst.tb)
			{
				case TB_INT:    addInstr(&owner->fn.instr, OP_ADD_I); break;
				case TB_DOUBLE: addInstr(&owner->fn.instr, OP_ADD_F); break;
			}

			// *r = (Ret){tDst, false, true};

			if (exprAddPrim(r))
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
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprMul(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for -");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
			switch (tDst.tb)
			{
				case TB_INT:	addInstr(&owner->fn.instr, OP_SUB_I); break;
				case TB_DOUBLE: addInstr(&owner->fn.instr, OP_SUB_F); break;
			}

			// *r = (Ret){tDst, false, true};

			if (exprAddPrim(r))
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

bool exprAdd(Ret *r)
{
	Token *start = iTk;

	if (exprMul(r))
	{
		if (exprAddPrim(r))
		{
			return true;
		}
	}	

	iTk = start;
	return false;
}

// exprRelPrim: ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim | epsilon
bool exprRelPrim(Ret *r)
{
	Token *op;

	if (consume(LESS))
	{
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for <");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);

			switch (tDst.tb)
			{
				case TB_INT:    addInstr(&owner->fn.instr, OP_LESS_I); break;
				case TB_DOUBLE: addInstr(&owner->fn.instr, OP_LESS_F); break;
			}
			
			// *r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprRelPrim(r))
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
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for <=");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);

			// *r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprRelPrim(r))
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
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for >");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);

			// *r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprRelPrim(r))
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
		Instr *lastLeft = lastInstr(owner->fn.instr);
		addRVal(&owner->fn.instr, r->lval, &r->type);

		Ret right;
		if (exprAdd(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for >=");

			addRVal(&owner->fn.instr, right.lval, &right.type);
			insertConvIfNeeded(lastLeft, &r->type, &tDst);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);

			// *r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprRelPrim(r))
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

bool exprRel(Ret *r)
{
	Token *start = iTk;

	if (exprAdd(r))
	{
		if (exprRelPrim(r))
		{
			return true;
		}
	}

	iTk = start;
	return false;
}

// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim | epsilon
bool exprEqPrim(Ret *r)
{
	if (consume(EQUAL))
	{
		Ret right;
		if (exprRel(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for ==");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprEqPrim(r))
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
		Ret right;
		if (exprRel(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst))tkerr("invalid operand type for !=");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};

			if (exprEqPrim(r))
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

bool exprEq(Ret *r)
{
	Token *start = iTk;

	if (exprRel(r))
	{
		if (exprEqPrim(r))
		{
			return true;
		}
	}
	
	iTk = start;
	return false;
}

// exprAndPrim: ( AND ) exprEq exprAndPrim | epsilon
bool exprAndPrim(Ret *r)
{
	if(consume(AND))
	{
		Ret right;
		if (exprEq(&right))
		{
			Type tDst;
			if (!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for &&");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprAndPrim(r))
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

bool exprAnd(Ret *r)
{
	Token *start = iTk;

	if (exprEq(r))
	{
		if (exprAndPrim(r))
		{
			return true;
		}
	}
	
	iTk = start;
	return false;
}

// exprOrPrim: ( OR ) exprAnd exprOrPrim | epsilon
bool exprOrPrim(Ret *r)
{
	if(consume(OR))
	{
		Ret right;
		if (exprAnd(&right))
		{
			Type tDst;
			if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for ||");
			*r = (Ret){{TB_INT, NULL, -1}, false, true};
			if (exprOrPrim(r))
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

bool exprOr(Ret *r)
{
	Token *start = iTk;

	if (exprAnd(r))
	{
		if (exprOrPrim(r))
		{
			return true;
		}
	}
	
	iTk = start;
	return false;
}

// exprAssign: exprUnary ASSIGN exprAssign | exprOr
bool exprAssign(Ret *r)
{
	Token *start = iTk;
	Ret rDst;

	if (exprUnary(&rDst))
	{
		if (consume(ASSIGN))
		{
			if (exprAssign(r))
			{
				addRVal(&owner->fn.instr, r->lval, &r->type);
				insertConvIfNeeded(lastInstr(owner->fn.instr), &r->type, &rDst.type);
				switch (rDst.type.tb)
				{
					case TB_INT:    addInstr(&owner->fn.instr, OP_STORE_I); break;
					case TB_DOUBLE: addInstr(&owner->fn.instr, OP_STORE_F); break;
				}

				if (!rDst.lval)					   tkerr("the assign destination must be a left-value");
				if (rDst.ct)					   tkerr("the assign destination cannot be constant");
				if (!canBeScalar(&rDst))		   tkerr("the assign destination must be scalar");
				if (!canBeScalar(r)) 			   tkerr("the assign source must be scalar");
				if (!convTo(&r->type, &rDst.type)) tkerr("the assign source cannot be converted to destination");
				r->lval = false;
				r->ct = true;
				return true;
			}
			else
			{
				tkerr("syntax error in assignment expression: expected expression after '='");
			}
		}
	}

	iTk = start;

	if (exprOr(r))
	{
		return true;
	}

	return false;
}

// expr: exprAssign
bool expr(Ret *r)
{
	if (exprAssign(r))
	{
		return true;
	}

	return false;
}

// stmCompound[in bool newDomain]: LACC ( varDef | stm )* RACC
bool stmCompound(bool newDomain)
{
    Token *start = iTk;
    if (consume(LACC))
    {
        if (newDomain) pushDomain();

        for (;;)
        {
            if (varDef()) {}
            else if (stm()) {}
            else break;
        }

        if (consume(RACC))
        {
            if (newDomain) dropDomain();
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
	Ret rCond, rExpr;

	if (stmCompound(true))
	{
		return true;
	}

	if (consume(IF))
	{
		if (consume(LPAR))
		{
			if (expr(&rCond))
			{
				if (!canBeScalar(&rCond)) tkerr("the if condition must be a scalar value");

				if (consume(RPAR))
				{
					addRVal(&owner->fn.instr, rCond.lval, &rCond.type);
					Type intType = {TB_INT, NULL, -1};
					insertConvIfNeeded(lastInstr(owner->fn.instr), &rCond.type, &intType);
					Instr *ifJF = addInstr(&owner->fn.instr, OP_JF);

					if (stm())
					{
						if (consume(ELSE))
						{
							Instr *ifJMP = addInstr(&owner->fn.instr, OP_JMP);
                            ifJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);

							if (!stm())
							{
								tkerr("syntax error in 'else' statement");
							}

							ifJMP->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
						}
						else
						{
							ifJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);
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
		Instr *beforeWhileCond = lastInstr(owner->fn.instr);
		if (consume(LPAR))
		{
			if (expr(&rCond))
			{
				if (!canBeScalar(&rCond)) tkerr("the while condition must be a scalar value");

				if (consume(RPAR))
				{
					addRVal(&owner->fn.instr, rCond.lval, &rCond.type);
					Type intType = {TB_INT, NULL, -1};
					insertConvIfNeeded(lastInstr(owner->fn.instr), &rCond.type, &intType);
					Instr *whileJF = addInstr(&owner->fn.instr, OP_JF);

					if (stm())
					{
						addInstr(&owner->fn.instr, OP_JMP)->arg.instr = beforeWhileCond->next;
						whileJF->arg.instr = addInstr(&owner->fn.instr, OP_NOP);

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
		if (expr(&rExpr)) 
		{
			addRVal(&owner->fn.instr, rExpr.lval, &rExpr.type);
			insertConvIfNeeded(lastInstr(owner->fn.instr), &rExpr.type, &owner->type);
			addInstrWithInt(&owner->fn.instr, OP_RET, symbolsLen(owner->fn.params));

			if (owner->type.tb == TB_VOID) 		    tkerr("a void function cannot return a value");
			if (!canBeScalar(&rExpr)) 			    tkerr("the return value must be a scalar value");
			if (!convTo(&rExpr.type, &owner->type)) tkerr("cannot convert the return expression type to the function return type");
		}
		else
		{
			addInstr(&owner->fn.instr, OP_RET_VOID);

			if(owner->type.tb != TB_VOID) tkerr("a non-void function must return a value");
		}
		
		if (consume(SEMICOLON))
		{
			return true;
		}
		else
		{
			tkerr("expected ';' after return statement");
		}
	}

	if (expr(&rExpr))
	{
		if (rExpr.type.tb != TB_VOID) addInstr(&owner->fn.instr, OP_DROP);

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
bool fnParam()
{
    Token *start = iTk;
    Type t;

    if (typeBase(&t))
    {
        if (consume(ID))
        {
            Token *tkName = consumedTk;

            if (arrayDecl(&t))
			{
				t.n = 0;
			}
            
            Symbol *param = findSymbolInDomain(symTable, tkName->text);
            if (param) tkerr("symbol redefinition: %s", tkName->text);

            param = newSymbol(tkName->text, SK_PARAM);
            param->type = t;
            param->owner = owner;
            param->paramIdx = symbolsLen(owner->fn.params);

            addSymbolToDomain(symTable, param);
            addSymbolToList(&owner->fn.params, dupSymbol(param));

            return true;
        }
        tkerr("expected parameter name after type");
    }

    iTk = start;
    return false;
}

// fnDef: ( typeBase[&t] | VOID ) ID[tkName] LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound[false]
bool fnDef()
{
    Token *start = iTk;
    Type t;

	bool hasType = false;
	if (typeBase(&t))
	{
		hasType = true;
	}
	else if (consume(VOID))
	{
		t.tb = TB_VOID;
		t.n = -1;
		t.s = NULL;
		hasType = true;
	}

	if (!hasType)
	{
		iTk = start;
		return false;
	}

    if (hasType)
    {
        if (consume(ID))
        {
            Token *tkName = consumedTk;

            if (consume(LPAR))
            {
				Symbol *fn = findSymbolInDomain(symTable, tkName->text);
				if (fn) tkerr("symbol redefinition: %s", tkName->text);

				fn = newSymbol(tkName->text, SK_FN);
				fn->type = t;
				addSymbolToDomain(symTable, fn);

				owner = fn;
				pushDomain();

                if (fnParam())
                {
                    while (consume(COMMA))
                    {
                        if (!fnParam()) tkerr("expected parameter after ','");
                    }
                }
                if (consume(RPAR))
                {
					addInstr(&fn->fn.instr, OP_ENTER);
                    if (stmCompound(false))
                    {
						fn->fn.instr->arg.i = symbolsLen(fn->fn.locals);
						if(fn->type.tb == TB_VOID)
							addInstrWithInt(&fn->fn.instr, OP_RET_VOID, symbolsLen(fn->fn.params));
                        // dropDomain();
                        owner = NULL;
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
bool varDef()
{
	Token *start = iTk;
	Type t;

	if (typeBase(&t))
	{
		if (consume(ID))
		{
			Token *tkName = consumedTk;

			if(arrayDecl(&t))
			{
				if (t.n == 0) tkerr("a vector variable must have a specified dimension");
			}
			
			if(consume(SEMICOLON))
			{
				Symbol *var = findSymbolInDomain(symTable, tkName->text);
                if (var) tkerr("symbol redefinition: %s", tkName->text);

                var = newSymbol(tkName->text, SK_VAR);
                var->type = t;
                var->owner = owner;
                addSymbolToDomain(symTable, var);

                if (owner)
                {
                    switch (owner->kind)
                    {
                        case SK_FN:
                            var->varIdx = symbolsLen(owner->fn.locals);
                            addSymbolToList(&owner->fn.locals, dupSymbol(var));
                            break;
                        case SK_STRUCT:
                            var->varIdx = typeSize(&owner->type);
                            addSymbolToList(&owner->structMembers, dupSymbol(var));
                            break;
						default: break;
                    }
                }
                else
                {
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