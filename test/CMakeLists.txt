
INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/src/include")

#DAHDI test applications
SET(DAHDI_TESTS
	test_dahdi_voice
	test_dahdi_sms)

IF (IS_DIRECTORY /usr/include/dahdi)
message("DAHDI installation detected")
FOREACH(TEST ${DAHDI_TESTS})
	ADD_EXECUTABLE(${TEST}
	${PROJECT_SOURCE_DIR}/test/${TEST}.c
	${PROJECT_SOURCE_DIR}/test/test_utils.c)
	TARGET_LINK_LIBRARIES(${TEST} wat)
	ADD_DEPENDENCIES(${TEST} ${PROJECT_NAME})
ENDFOREACH(TEST)
ENDIF(IS_DIRECTORY /usr/include/dahdi)

