set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/googletest ${CMAKE_BINARY_DIR}/googletest)

add_executable(cppcoder_tests
    TypesTests.cpp
    JsonUtilTests.cpp
    TaskQueueTests.cpp
    CodebaseScannerTests.cpp
    WorkerTests.cpp
    JudgeTests.cpp
    ResearchEngineTests.cpp
)
target_link_libraries(cppcoder_tests PRIVATE cppcoder_core GTest::gtest_main)
target_include_directories(cppcoder_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(cppcoder_tests)
