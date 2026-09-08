file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

add_executable(gravsim_tests main.cpp ${TEST_SOURCES})
target_link_libraries(gravsim_tests PRIVATE gravsim)
target_include_directories(gravsim_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME gravsim_tests COMMAND gravsim_tests)
