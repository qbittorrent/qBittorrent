// https://github.com/kunitoki/LuaBridge3
// Copyright 2020, kunitoki
// Copyright 2020, Dmitry Tarakanov
// Copyright 2012, Vinnie Falco <vinnie.falco@gmail.com>
// Copyright 2007, Nathan Reed
// SPDX-License-Identifier: MIT

#pragma once

// All #include dependencies are listed here
// instead of in the individual header files.

#define LUABRIDGE_MAJOR_VERSION 3
#define LUABRIDGE_MINOR_VERSION 1
#define LUABRIDGE_VERSION 301

#include "detail/Config.h"

#include "detail/CFunctions.h"
#include "detail/ClassInfo.h"
#include "detail/Coroutine.h"
#include "detail/Enum.h"
#include "detail/Errors.h"
#include "detail/Expected.h"
#include "detail/FlagSet.h"
#include "detail/FuncTraits.h"
#include "detail/Globals.h"
#include "detail/Invoke.h"
#include "detail/Iterator.h"
#include "detail/LuaException.h"
#include "detail/LuaHelpers.h"
#include "detail/LuaRef.h"
#include "detail/Namespace.h"
#include "detail/Options.h"
#include "detail/Overload.h"
#include "detail/Result.h"
#include "detail/ScopeGuard.h"
#include "detail/Stack.h"
#include "detail/TypeTraits.h"
#include "detail/Userdata.h"
