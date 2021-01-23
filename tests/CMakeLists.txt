find_package(GTest REQUIRED)


macro(create_test TESTNAME)
	add_executable(${TESTNAME} ${ARGN})
	target_include_directories(${TESTNAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
	target_link_libraries(${TESTNAME} GTest::GTest GTest::Main)
	gtest_discover_tests(${TESTNAME} WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)

	add_test(${TESTNAME}_test ${TESTNAME})
endmacro()

endmacro()

create_test(Calculate CalculateTests.cpp ${PROJECT_SOURCE_DIR}/src/Calculate.cpp)
create_test(Settings SettingsTests.cpp ${PROJECT_SOURCE_DIR}/src/Settings.cpp)
create_test(Parse ParseTests.cpp ${PROJECT_SOURCE_DIR}/src/ModuleConfig.cpp ${PROJECT_SOURCE_DIR}/src/Calculate.cpp)
