#CMake files thwat generates build files for libwat
#
# David Yat Sin <dyatsin@sangoma.com>

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(libwat)

ADD_DEFINITIONS(-ggdb3 -O0 -Wall -Wshadow -Wcast-qual -Wcast-align -Wchar-subscripts -Wmissing-prototypes -Wmissing-declarations)

SET(wat_VERSION_LT_CURRENT 2)
SET(wat_VERSION_LT_REVISION 2)
SET(wat_VERSION_LT_AGE 0)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(test)

