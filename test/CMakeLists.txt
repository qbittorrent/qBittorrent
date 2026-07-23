find_package(Qt6 REQUIRED COMPONENTS Test)

enable_testing(true)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

include_directories("../src")

set(testFiles
    testalgorithm.cpp
    testbittorrentpeeraddress.cpp
    testbittorrenttrackerentry.cpp
    testconceptsexplicitlyconvertibleto.cpp
    testconceptsstringable.cpp
    testglobal.cpp
    testorderedset.cpp
    testpath.cpp
    testutilsbytearray.cpp
    testutilscompare.cpp
    testutilsdatetime.cpp
    testutilsfs.cpp
    testutilsgzip.cpp
    testutilshashvalue.cpp
    testutilsio.cpp
    testutilsmisc.cpp
    testutilsnet.cpp
    testutilsnumber.cpp
    testutilsstring.cpp
    testutilsversion.cpp
)

foreach(testFile ${testFiles})
    get_filename_component(testFilename "${testFile}" NAME_WLE)

    add_executable("${testFilename}" "${testFile}")
    target_link_libraries("${testFilename}" PRIVATE Qt::Test qbt_base)
    add_test(NAME "${testFilename}" COMMAND "${testFilename}")

    add_dependencies(check "${testFilename}")
endforeach()

if (GUI AND (CMAKE_SYSTEM_NAME STREQUAL "Darwin"))
    if (WEBUI)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)

        add_test(NAME testsessionpaths
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/testsessionpaths.py"
                "$<TARGET_FILE:qbt_app>")
        set_tests_properties(testsessionpaths PROPERTIES
            RUN_SERIAL TRUE
            TIMEOUT 120
        )
        add_dependencies(check qbt_app)
    endif()
endif()
