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
    testhttprequestparser.cpp
    testorderedset.cpp
    testpath.cpp
    testutilsbytearray.cpp
    testutilscompare.cpp
    testutilsdatetime.cpp
    testutilsgzip.cpp
    testutilsio.cpp
    testutilsjson.cpp
    testutilsmisc.cpp
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

# testapicontroller exercises APIController (parseList / JSON-array params), which lives in the
# webui library, so unlike the other tests it must also link qbt_webui.
if (TARGET qbt_webui)
    add_executable(testapicontroller testapicontroller.cpp)
    target_link_libraries(testapicontroller PRIVATE Qt::Test qbt_webui qbt_base)
    add_test(NAME testapicontroller COMMAND testapicontroller)
    add_dependencies(check testapicontroller)
endif()
