/*
** $Id: lopcodes.c $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/

#define lopcodes_c
#define LUA_CORE

#include "lprefix.h"


#include "lopcodes.h"


#define opmode(mm,ot,it,t,a,m)  \
    (((mm) << 7) | ((ot) << 6) | ((it) << 5) | ((t) << 4) | ((a) << 3) | (m))


/* ORDER OP */

LUAI_DDEF const lu_byte luaP_opmodes[NUM_OPCODES] = {
/*       MM OT IT T  A  mode		   opcode  */
  opmode(0, 0, 0, 0, 1, iABC)		/* OP_MOVE */
 ,opmode(0, 0, 0, 0, 1, iAsBx)		/* OP_LOADI */
 ,opmode(0, 0, 0, 0, 1, iAsBx)		/* OP_LOADF */
 ,opmode(0, 0, 0, 0, 1, iABx)		/* OP_LOADK */
 ,opmode(0, 0, 0, 0, 1, iABx)		/* OP_LOADKX */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_LOADFALSE */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_LFALSESKIP */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_LOADTRUE */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_LOADNIL */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_GETUPVAL */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_SETUPVAL */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_GETTABUP */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_GETTABLE */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_GETI */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_GETFIELD */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_SETTABUP */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_SETTABLE */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_SETI */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_SETFIELD */
 ,opmode(0, 0, 0, 0, 1, ivABC)		/* OP_NEWTABLE */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_SELF */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_ADDI */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_ADDK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_SUBK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_MULK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_MODK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_POWK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_DIVK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_IDIVK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_BANDK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_BORK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_BXORK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_SHLI */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_SHRI */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_ADD */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_SUB */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_MUL */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_MOD */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_POW */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_DIV */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_IDIV */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_BAND */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_BOR */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_BXOR */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_SHL */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_SHR */
 ,opmode(1, 0, 0, 0, 0, iABC)		/* OP_MMBIN */
 ,opmode(1, 0, 0, 0, 0, iABC)		/* OP_MMBINI */
 ,opmode(1, 0, 0, 0, 0, iABC)		/* OP_MMBINK */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_UNM */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_BNOT */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_NOT */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_LEN */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_CONCAT */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_CLOSE */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_TBC */
 ,opmode(0, 0, 0, 0, 0, isJ)		/* OP_JMP */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_EQ */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_LT */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_LE */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_EQK */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_EQI */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_LTI */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_LEI */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_GTI */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_GEI */
 ,opmode(0, 0, 0, 1, 0, iABC)		/* OP_TEST */
 ,opmode(0, 0, 0, 1, 1, iABC)		/* OP_TESTSET */
 ,opmode(0, 1, 1, 0, 1, iABC)		/* OP_CALL */
 ,opmode(0, 1, 1, 0, 1, iABC)		/* OP_TAILCALL */
 ,opmode(0, 0, 1, 0, 0, iABC)		/* OP_RETURN */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_RETURN0 */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_RETURN1 */
 ,opmode(0, 0, 0, 0, 1, iABx)		/* OP_FORLOOP */
 ,opmode(0, 0, 0, 0, 1, iABx)		/* OP_FORPREP */
 ,opmode(0, 0, 0, 0, 0, iABx)		/* OP_TFORPREP */
 ,opmode(0, 0, 0, 0, 0, iABC)		/* OP_TFORCALL */
 ,opmode(0, 0, 0, 0, 1, iABx)		/* OP_TFORLOOP */
 ,opmode(0, 0, 1, 0, 0, ivABC)		/* OP_SETLIST */
 ,opmode(0, 0, 0, 0, 1, iABx)		/* OP_CLOSURE */
 ,opmode(0, 1, 0, 0, 1, iABC)		/* OP_VARARG */
 ,opmode(0, 0, 0, 0, 1, iABC)		/* OP_GETVARG */
 ,opmode(0, 0, 0, 0, 0, iABx)		/* OP_ERRNNIL */
 ,opmode(0, 0, 1, 0, 1, iABC)		/* OP_VARARGPREP */
 ,opmode(0, 0, 0, 0, 0, iAx)		/* OP_EXTRAARG */
};



/*
** Check whether instruction sets top for next instruction, that is,
** it results in multiple values.
*/
int luaP_isOT (Instruction i) {
  OpCode op = GET_OPCODE(i);
  switch (op) {
    case OP_TAILCALL: return 1;
    default:
      return testOTMode(op) && GETARG_C(i) == 0;
  }
}


/*
** Check whether instruction uses top from previous instruction, that is,
** it accepts multiple results.
*/
int luaP_isIT (Instruction i) {
  OpCode op = GET_OPCODE(i);
  switch (op) {
    case OP_SETLIST:
      return testITMode(GET_OPCODE(i)) && GETARG_vB(i) == 0;
    default:
      return testITMode(GET_OPCODE(i)) && GETARG_B(i) == 0;
  }
}

