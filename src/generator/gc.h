#pragma once

// code generation

#include "types/at.h"
#include "vm/vm.h"

// inserts after the specified instruction a conversion instruction
// only if necessary
void insertConvIfNeeded(Instr *before,Type *srcType,Type *dstType);

// if lval is true, generates an rval from the current value from stack
void addRVal(Instr **code,bool lval,Type *type);
