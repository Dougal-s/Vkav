find_package(GTest REQUIRED)

macro(add_test TESTNAME)
	add_executable(${TESTNAME} ${ARGN})
	target_include_directories(${TESTNAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
	target_link_libraries(${TESTNAME} GTest::GTest GTest::Main)
	gtest_discover_tests(${TESTNAME} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
endmacro()

add_test(Calculate CalculateTests.cpp ${PROJECT_SOURCE_DIR}/src/Calculate.cpp)
add_test(Settings SettingsTests.cpp ${PROJECT_SOURCE_DIR}/src/Settings.cpp)
