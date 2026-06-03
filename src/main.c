#include <stdio.h>
#include <stdlib.h>

#include "domain/ad.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "utils/utils.h"
#include "vm/vm.h"

int main(int argc, char **argv) 
{
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        return 1;
    }

    char *input = loadFile(argv[1]);
    Token *tokens = tokenize(input);
    // showTokens(tokens);

    pushDomain();
    vmInit();

    parse(tokens);
    
    // showDomain(symTable, "global");

    Instr *testCode1 = genTestProgram();
    run(testCode1);
    Instr *testCode2 = genTestProgram_double();
    run(testCode2);

    dropDomain();

    free(input);
    return 0;
}