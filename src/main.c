#include <stdio.h>
#include <stdlib.h>

#include "lexer/lexer.h"
#include "parser/parser.h"
#include "utils/utils.h"

int main(int argc, char **argv) 
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        return 1;
    }

    char *input = loadFile(argv[1]);
    Token *tokens = tokenize(input);
    showTokens(tokens);

    parse(tokens);

    free(input);
    return 0;
}