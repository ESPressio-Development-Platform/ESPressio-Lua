// Lua 5.5.1 compiled as C++ so its supported exception path preserves C++ RAII.
// Keep upstream sources unmodified. This follows upstream onelua.c's amalgamation
// arrangement but links only the standard libraries exposed by Instance.
#if !defined(__cpp_exceptions) && !defined(__EXCEPTIONS)
#error "ESPressio-Lua requires C++ exceptions (-fexceptions)."
#endif
#ifdef LUA_USE_LONGJMP
#error "ESPressio-Lua requires Lua's C++ exception unwinding."
#endif
#include "../vendor/lua/lprefix.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// Use the C++ limits header: some embedded C headers hide LLONG_MAX in C++ mode.
#include <climits>
#define LUA_CORE
#define LUA_LIB
#include "../vendor/lua/luaconf.h"
#undef LUAI_FUNC
#undef LUAI_DDEC
#undef LUAI_DDEF
#define LUAI_FUNC static
#define LUAI_DDEC(def)
#define LUAI_DDEF static
#include "../vendor/lua/lzio.c"
#include "../vendor/lua/lctype.c"
#include "../vendor/lua/lopcodes.c"
#include "../vendor/lua/lmem.c"
#include "../vendor/lua/lundump.c"
#include "../vendor/lua/ldump.c"
#include "../vendor/lua/lstate.c"
#include "../vendor/lua/lgc.c"
#include "../vendor/lua/llex.c"
#include "../vendor/lua/lcode.c"
#include "../vendor/lua/lparser.c"
#include "../vendor/lua/ldebug.c"
#include "../vendor/lua/lfunc.c"
#include "../vendor/lua/lobject.c"
#include "../vendor/lua/ltm.c"
#include "../vendor/lua/lstring.c"
#include "../vendor/lua/ltable.c"
#include "../vendor/lua/ldo.c"
#include "../vendor/lua/lvm.c"
#include "../vendor/lua/lapi.c"
#include "../vendor/lua/lauxlib.c"
#include "../vendor/lua/lbaselib.c"
#include "../vendor/lua/lmathlib.c"
#include "../vendor/lua/lstrlib.c"
#include "../vendor/lua/ltablib.c"
#include "../vendor/lua/lutf8lib.c"
