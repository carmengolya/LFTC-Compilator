#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lexer.h"
#include "utils.h"

Token *tokens; // single linked list of tokens
Token *lastTk; // the last token in list

int line = 1; // the current line in the input file

static char decodeEscape(const char **ppch)
{
    switch (**ppch)
    {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case '0': return '\0';
        default:
            err("invalid escape sequence: \\%c", **ppch);
    }
}

static void appendChar(char **buf, size_t *len, size_t *cap, char ch)
{
    if (*len + 1 >= *cap)
    {
        *cap *= 2;
        char *newBuf = realloc(*buf, *cap);
        if (!newBuf)
            err("not enough memory");
        *buf = newBuf;
    }
    (*buf)[(*len)++] = ch;
}

static const char *tokenName(int code)
{
    switch (code)
    {
        case ID: return "ID";
        case TYPE_CHAR: return "TYPE_CHAR";
        case TYPE_DOUBLE: return "TYPE_DOUBLE";
        case ELSE: return "ELSE";
        case IF: return "IF";
        case TYPE_INT: return "TYPE_INT";
        case RETURN: return "RETURN";
        case STRUCT: return "STRUCT";
        case VOID: return "VOID";
        case WHILE: return "WHILE";
        case COMMA: return "COMMA";
        case END: return "END";
        case SEMICOLON: return "SEMICOLON";
        case LPAR: return "LPAR";
        case RPAR: return "RPAR";
        case LBRACKET: return "LBRACKET";
        case RBRACKET: return "RBRACKET";
        case LACC: return "LACC";
        case RACC: return "RACC";
        case ASSIGN: return "ASSIGN";
        case EQUAL: return "EQUAL";
        case ADD: return "ADD";
        case SUB: return "SUB";
        case MUL: return "MUL";
        case DIV: return "DIV";
        case DOT: return "DOT";
        case AND: return "AND";
        case OR: return "OR";
        case NOT: return "NOT";
        case NOTEQ: return "NOTEQ";
        case LESS: return "LESS";
        case LESSEQ: return "LESSEQ";
        case GREATER: return "GREATER";
        case GREATEREQ: return "GREATEREQ";
        case INT: return "INT";
        case CHAR: return "CHAR";
        case STRING: return "STRING";
        case DOUBLE: return "DOUBLE";
        default: return "UNKNOWN";
    }
}

// adds a token to the end of the tokens list and returns it
// sets its code and line
Token *addTk(int code)
{
    Token *tk = safeAlloc(sizeof(Token));
    tk->code = code;
    tk->line = line;
    tk->next = NULL;
    if (lastTk)
    {
        lastTk->next = tk;
    }
    else
    {
        tokens = tk;
    }
    lastTk = tk;
    return tk;
}

char *extract(const char *begin, const char *end)
{
    int len = (int)(end - begin);
    char *text = (char *)safeAlloc((size_t)len + 1);
    strncpy(text, begin, (size_t)len);
    text[len] = '\0';
    return text;
}

Token *tokenize(const char *pch)
{
    const char *start;
    Token *tk;

    tokens = NULL;
    lastTk = NULL;
    line = 1;

    for (;;)
    {
        switch (*pch)
        {
            case ' ':
            case '\t':
            {
                pch++;
                break;
            }

            case '\r':
            {   // handles different kinds of newlines (Windows: \r\n, Linux:
                // \n, MacOS, OS X: \r or \n)
                if (pch[1] == '\n')
                    pch++;
            }
                
            case '\n':
            {   // fallthrough to \n
                line++;
                pch++;
                break;
            }

            case '\0':
            {
                addTk(END);
                return tokens;
            }

            case ',':
            {
                addTk(COMMA);
                pch++;
                break;
            }

            case ';':
            {
                addTk(SEMICOLON);
                pch++;
                break;
            }

            case '.':
            {
                addTk(DOT);
                pch++;
                break;
            }

            case '=':
            {
                if (pch[1] == '=')
                {
                    addTk(EQUAL);
                    pch += 2;
                }
                else
                {
                    addTk(ASSIGN);
                    pch++;
                }
                break;
            }

            case '!':
            {
                if(pch[1] == '=')
                {
                    addTk(NOTEQ);
                    pch += 2;
                }
                else
                {
                    addTk(NOT);
                    pch++;
                }
                break;
            }

            case '<': // LESS
            {
                if(pch[1] == '=')
                {
                    addTk(LESSEQ);
                    pch += 2;
                }
                else
                {
                    addTk(LESS);
                    pch++;
                }
                break;
            }

            case '>': // GREATER
            {
                if(pch[1] == '=')
                {
                    addTk(GREATEREQ);
                    pch += 2;
                }
                else
                {
                    addTk(GREATER);
                    pch++;
                }
                break;
            }

            case '+':
            {
                addTk(ADD);
                pch++;
                break;
            }

            case '-':
            {
                addTk(SUB);
                pch++;
                break;
            }

            case '*':
            {
                addTk(MUL);
                pch++;
                break;
            }

            case '/':
            {
                if (pch[1] == '/')
                {
                    pch += 2;
                    while (*pch != '\0' && *pch != '\n' && *pch != '\r')
                        pch++;
                }
                else if (pch[1] == '*')
                {
                    pch += 2;
                    while (*pch)
                    {
                        if (*pch == '\r')
                        {
                            if (pch[1] == '\n')
                                pch++;
                            line++;
                        }
                        else if (*pch == '\n')
                        {
                            line++;
                        }
                        else if (*pch == '*' && pch[1] == '/')
                        {
                            pch += 2;
                            break;
                        }
                        pch++;
                    }
                    if (*pch == '\0')
                        err("unterminated comment");
                }
                else
                {
                    addTk(DIV);
                    pch++;
                }
                break;
            }

            case '(':
            {
                addTk(LPAR);
                pch++;
                break;
            }

            case '&':
            {
                if (pch[1] == '&')
                {
                    addTk(AND);
                    pch += 2;
                }
                else
                {
                    err("invalid char: %c (%d)", *pch, *pch);
                }
                break;
            }

            case '|':
            {
                if (pch[1] == '|')
                {
                    addTk(OR);
                    pch += 2;
                }
                else
                {
                    err("invalid char: %c (%d)", *pch, *pch);
                }
                break;
            }

            case ')':
            {
                addTk(RPAR);
                pch++;
                break;
            }

            case '[':
            {
                addTk(LBRACKET);
                pch++;
                break;
            }

            case ']':
            {
                addTk(RBRACKET);
                pch++;
                break;
            }

            case '{':
            {
                addTk(LACC);
                pch++;
                break;
            }

            case '}':
            {
                addTk(RACC);
                pch++;
                break;
            }

            default:
            {
                if (isalpha(*pch) || *pch == '_')
                {
                    for (start = pch++; isalnum(*pch) || *pch == '_'; pch++){}

                    char *text = extract(start, pch);
                    if (strcmp(text, "char") == 0)
                    {
                        addTk(TYPE_CHAR);
                        free(text);
                    }
                    else if (strcmp(text, "int") == 0)
                    {
                        addTk(TYPE_INT);
                        free(text);
                    }
                    else if (strcmp(text, "double") == 0)
                    {
                        addTk(TYPE_DOUBLE);
                        free(text);
                    }
                    else if (strcmp(text, "if") == 0)
                    {
                        addTk(IF);
                        free(text);
                    }
                    else if (strcmp(text, "else") == 0)
                    {
                        addTk(ELSE);
                        free(text);
                    }
                    else if (strcmp(text, "return") == 0)
                    {
                        addTk(RETURN);
                        free(text);
                    }
                    else if (strcmp(text, "struct") == 0)
                    {
                        addTk(STRUCT);
                        free(text);
                    }
                    else if (strcmp(text, "void") == 0)
                    {
                        addTk(VOID);
                        free(text);
                    }
                    else if (strcmp(text, "while") == 0)
                    {
                        addTk(WHILE);
                        free(text);
                    }
                    else
                    {
                        tk = addTk(ID);
                        tk->text = text;
                    }
                }
                else if (isdigit((unsigned char) *pch))
                {
                    const char *startNum = pch;
                    int isDouble = 0;

                    while (isdigit((unsigned char)*pch))
                        pch++;

                    if (*pch == '.')
                    {
                        isDouble = 1;
                        pch++;
                        while (isdigit((unsigned char)*pch))
                            pch++;
                    }

                    if (*pch == 'e' || *pch == 'E')
                    {
                        isDouble = 1;
                        pch++;
                        if (*pch == '+' || *pch == '-')
                            pch++;
                        if (!isdigit((unsigned char)*pch))
                            err("invalid exponent in number");
                        while (isdigit((unsigned char)*pch))
                            pch++;
                    }

                    char *text = extract(startNum, pch);
                    if (isDouble)
                    {
                        tk = addTk(DOUBLE);
                        tk->d = strtod(text, NULL);
                    }
                    else
                    {
                        tk = addTk(INT);
                        tk->i = (int)strtol(text, NULL, 10);
                    }
                    free(text);
                }
                else if (*pch == '"')
                {
                    pch++;
                    size_t cap = 16;
                    size_t len = 0;
                    char *text = safeAlloc(cap);

                    while (*pch != '\0' && *pch != '"')
                    {
                        if (*pch == '\n' || *pch == '\r')
                            err("unterminated string literal - line %d", line);

                        if (*pch == '\\')
                        {
                            pch++;
                            if (*pch == '\0' || *pch == '\n' || *pch == '\r')
                                err("unterminated string literal - line %d", line);
                            appendChar(&text, &len, &cap, decodeEscape(&pch));
                            pch++;
                        }
                        else
                        {
                            appendChar(&text, &len, &cap, *pch);
                            pch++;
                        }
                    }

                    if (*pch != '"')
                        err("unterminated string literal - line %d", line);
                    pch++;

                    text[len] = '\0';
                    tk = addTk(STRING);
                    tk->text = text;
                }
                else if (*pch == '\'')
                {
                    pch++;
                    if (*pch == '\0' || *pch == '\n' || *pch == '\r' || *pch == '\'')
                        err("invalid char literal - line %d", line);

                    char ch;
                    if (*pch == '\\')
                    {
                        pch++;
                        if (*pch == '\0' || *pch == '\n' || *pch == '\r')
                            err("invalid char literal - line %d", line);
                        ch = decodeEscape(&pch);
                        pch++;
                    }
                    else
                    {
                        ch = *pch;
                        pch++;
                    }

                    if (*pch != '\'')
                        err("invalid char literal - line %d", line);
                    pch++;

                    tk = addTk(CHAR);
                    tk->c = ch;
                }
                else
                {
                    err("invalid char: %c (%d) - line %d", *pch, *pch, line);
                }
            }
        }
    }
}

void showTokens(const Token *tokens)
{
    for (const Token *tk = tokens; tk; tk = tk->next)
    {
        printf("%d\t%s", tk->line, tokenName(tk->code));
        switch (tk->code)
        {
            case ID:
                printf(":%s", tk->text);
                break;
            case INT:
                printf(":%d", tk->i);
                break;
            case DOUBLE:
                printf(":%g", tk->d);
                break;
            case CHAR:
                printf(":%c", tk->c);
                break;
            case STRING:
                printf(":%s", tk->text);
                break;
        }
        putchar('\n');
    }
}
