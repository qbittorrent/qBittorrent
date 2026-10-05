add_library(qbt_luabridge INTERFACE)

set(LUABRIDGE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Any.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Array.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Deque.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Dump.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/FlatMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/FlatSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/ForwardList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/List.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/LuaBridge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Map.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/MultiMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Set.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Span.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/StdExpected.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/UnorderedMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/UnorderedMultiMap.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/UnorderedSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Variant.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/Vector.h
)

source_group("LuaBridge" FILES ${LUABRIDGE_HEADERS})

set(LUABRIDGE_DETAIL_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/CFunctions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/ClassInfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Converter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Coroutine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Enum.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Errors.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Expected.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/FlagSet.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/FuncTraits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Globals.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Invoke.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Iterator.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/LuaException.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/LuaHelpers.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/LuaRef.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Namespace.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Options.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Overload.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Result.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/ScopeGuard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Stack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/TypeTraits.h
    ${CMAKE_CURRENT_SOURCE_DIR}/LuaBridge/detail/Userdata.h
)

source_group("LuaBridge\\detail" FILES ${LUABRIDGE_DETAIL_HEADERS})

target_sources(qbt_luabridge INTERFACE
    ${LUABRIDGE_HEADERS}
    ${LUABRIDGE_DETAIL_HEADERS}
)

target_include_directories(qbt_luabridge INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
