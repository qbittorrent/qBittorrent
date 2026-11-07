find_package(Qt6 REQUIRED COMPONENTS Test)

enable_testing(true)
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure)

include_directories("../src")

set(testFiles
    testalgorithm.cpp
    testbittorrentpeeraddress.cpp
    testbittorrenttorrentdescriptor.cpp
    testbittorrenttrackerentry.cpp
    testconceptsexplicitlyconvertibleto.cpp
    testconceptsstringable.cpp
    testglobal.cpp
    testhttprequestparser.cpp
    testorderedset.cpp
    testpath.cpp
    testutilsbytearray.cpp
    testutilscompare.cpp
    testutilsdatetime.cpp
    testutilsdict.cpp
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

if (UNIX)
    add_executable(testsignalhandler
        testsignalhandler.cpp
        ../src/app/signalhandler.cpp
    )
    target_link_libraries(testsignalhandler PRIVATE Qt::Test qbt_common_cfg)
    add_test(NAME testsignalhandler COMMAND testsignalhandler)
    add_dependencies(check testsignalhandler)
endif()
