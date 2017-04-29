 ###############################################################
 #
 # Copyright 2011 Red Hat, Inc.
 #
 # Licensed under the Apache License, Version 2.0 (the "License"); you
 # may not use this file except in compliance with the License.  You may
 # obtain a copy of the License at
 #
 #    http://www.apache.org/licenses/LICENSE-2.0
 #
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 #
 ###############################################################

MACRO (GLIBC_DETECT _VERSION)

# there are multiple ways to detect glibc, but given nmi's
# cons'd up paths I will trust only gcc.  I guess I could also use
# ldd --version to detect.

    set(_GLIB_SOURCE_DETECT "
#include <limits.h>
#include <stdio.h>
int main()
{
  printf(\"%d%d\",__GLIBC__, __GLIBC_MINOR__);
  return 0;
}
")

file (WRITE ${CMAKE_CURRENT_BINARY_DIR}/build/cmake/glibc.cpp "${_GLIB_SOURCE_DETECT}\n")

try_run(POST26_GLIBC_DETECTED
        POST26_GLIBC_COMPILE
        ${CMAKE_CURRENT_BINARY_DIR}/build/cmake
        ${CMAKE_CURRENT_BINARY_DIR}/build/cmake/glibc.cpp
        RUN_OUTPUT_VARIABLE GLIBC_VERSION )

if (GLIBC_VERSION AND POST26_GLIBC_COMPILE )
    set(${_VERSION} ${GLIBC_VERSION})
else()
    message(STATUS "NOTE: Could not detect GLIBC_VERSION from compiler")
endif()

ENDMACRO (GLIBC_DETECT)
