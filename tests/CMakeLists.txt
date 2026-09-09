file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

add_executable(simcore-tests main.cpp ${TEST_SOURCES})
target_link_libraries(simcore-tests PRIVATE simcore)
target_include_directories(simcore-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_test(NAME simcore-tests COMMAND simcore-tests)
