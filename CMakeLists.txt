CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(libwat)

ADD_DEFINITIONS(-ggdb3 -O0 -Wall -Wshadow -Wcast-qual -Wcast-align -Wchar-subscripts -Wmissing-prototypes -Wmissing-declarations)

# Please modify the libtool (LT) current, revision and age version numbers properly
# Package and LT versions are not the same (they can match sometimes though)
# see http://sources.redhat.com/autobook/autobook/autobook_91.html for details
# more info on versioning: http://www.nondot.org/sabre/Mirrored/libtool-2.1a/libtool_6.html#SEC33
SET(wat_VERSION_LT_CURRENT 2)
SET(wat_VERSION_LT_REVISION 0)
SET(wat_VERSION_LT_AGE 0)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(test)

