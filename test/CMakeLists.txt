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
    testrssautodownloadrule.cpp
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

set(guiTestFiles)
if (GUI)
    list(APPEND guiTestFiles testrssreaderfunctional.cpp)
endif()

foreach(testFile IN LISTS testFiles guiTestFiles)
    get_filename_component(testFilename "${testFile}" NAME_WLE)

    add_executable("${testFilename}" "${testFile}")
    target_link_libraries("${testFilename}" PRIVATE Qt::Test qbt_base)
    if (testFile IN_LIST guiTestFiles)
        target_link_libraries("${testFilename}" PRIVATE qbt_gui)
        if (WEBUI)
            target_link_libraries("${testFilename}" PRIVATE qbt_webui)
        endif()
    endif()
    add_test(NAME "${testFilename}" COMMAND "${testFilename}")
    if (testFile IN_LIST guiTestFiles)
        set_tests_properties("${testFilename}" PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()

    add_dependencies(check "${testFilename}")
endforeach()
