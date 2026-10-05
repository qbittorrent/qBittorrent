/*
** $Id: ldo.h $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#ifndef ldo_h
#define ldo_h


#include "llimits.h"
#include "lobject.h"
#include "lstate.h"
#include "lzio.h"


/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/

#if !defined(HARDSTACKTESTS)
#define condmovestack(L,pre,pos)	((void)0)
#else
/* realloc stack keeping its size */
#define condmovestack(L,pre,pos)  \
  { int sz_ = stacksize(L); pre; luaD_reallocstack((L), sz_, 0); pos; }
#endif

#define luaD_checkstackaux(L,n,pre,pos)  \
	if (l_unlikely(L->stack_last.p - L->top.p <= (n))) \
	  { pre; luaD_growstack(L, n, 1); pos; } \
	else { condmovestack(L,pre,pos); }

/* In general, 'pre'/'pos' are empty (nothing to save) */
#define luaD_checkstack(L,n)	luaD_checkstackaux(L,n,(void)0,(void)0)



#define savestack(L,pt)		(cast_charp(pt) - cast_charp(L->stack.p))
#define restorestack(L,n)	cast(StkId, cast_charp(L->stack.p) + (n))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  luaD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p),  /* save 'p' */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/
#if !defined(LUAI_MAXCCALLS)
#define LUAI_MAXCCALLS		200
#endif


/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

LUAI_FUNC l_noret luaD_errerr (lua_State *L);
LUAI_FUNC void luaD_seterrorobj (lua_State *L, TStatus errcode, StkId oldtop);
LUAI_FUNC TStatus luaD_protectedparser (lua_State *L, ZIO *z,
                                                  const char *name,
                                                  const char *mode);
LUAI_FUNC void luaD_hook (lua_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
LUAI_FUNC void luaD_hookcall (lua_State *L, CallInfo *ci);
LUAI_FUNC int luaD_pretailcall (lua_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
LUAI_FUNC CallInfo *luaD_precall (lua_State *L, StkId func, int nResults);
LUAI_FUNC void luaD_call (lua_State *L, StkId func, int nResults);
LUAI_FUNC void luaD_callnoyield (lua_State *L, StkId func, int nResults);
LUAI_FUNC TStatus luaD_closeprotected (lua_State *L, ptrdiff_t level,
                                                     TStatus status);
LUAI_FUNC TStatus luaD_pcall (lua_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
LUAI_FUNC void luaD_poscall (lua_State *L, CallInfo *ci, int nres);
LUAI_FUNC int luaD_reallocstack (lua_State *L, int newsize, int raiseerror);
LUAI_FUNC int luaD_growstack (lua_State *L, int n, int raiseerror);
LUAI_FUNC void luaD_shrinkstack (lua_State *L);
LUAI_FUNC void luaD_inctop (lua_State *L);
LUAI_FUNC int luaD_checkminstack (lua_State *L);

LUAI_FUNC l_noret luaD_throw (lua_State *L, TStatus errcode);
LUAI_FUNC l_noret luaD_throwbaselevel (lua_State *L, TStatus errcode);
LUAI_FUNC TStatus luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);

#endif

