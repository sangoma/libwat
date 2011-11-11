#CMake files thwat generates build files for libwat
#
# David Yat Sin <dyatsin@sangoma.com>
	
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(libwat)

#Sample test applications
#FOREACH(TEST testwat_dahdi testwat_blocking)

#ADD_DEFINITIONS(-ggdb3 -O0 -Wall -Wshadow -Wcast-qual -Wcast-align -Wchar-subscripts -Wmissing-prototypes -Wmissing-declarations)

ADD_DEFINITIONS(-ggdb3 -O0 )
FOREACH(TEST testwat_dahdi testwat_dahdi_sms)
	ADD_EXECUTABLE(${TEST}
	${PROJECT_SOURCE_DIR}/test/${TEST}.c)
	TARGET_LINK_LIBRARIES(${TEST} wat)
	ADD_DEPENDENCIES(${TEST} ${PROJECT_NAME})
ENDFOREACH(TEST)

INCLUDE_DIRECTORIES("${PROJECT_SOURCE_DIR}/src/include")
ADD_SUBDIRECTORY(src)

