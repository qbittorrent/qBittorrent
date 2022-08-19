if (QT6)
    find_package(Qt6 REQUIRED COMPONENTS Test)
else()
    find_package(Qt5 REQUIRED COMPONENTS Test)
endif()

enable_testing(true)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

include_directories("../src")

set(testFiles
    testalgorithm.cpp
    testorderedset.cpp
    testpath.cpp
    testutilscompare.cpp
    testutilsgzip.cpp
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
