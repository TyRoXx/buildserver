FIND_PATH(URIPARSER_INCLUDE_DIR NAMES uriparser/Uri.h)
MARK_AS_ADVANCED(URIPARSER_INCLUDE_DIR)
FIND_LIBRARY(URIPARSER_LIBRARY NAMES uriparser)
MARK_AS_ADVANCED(URIPARSER_LIBRARY)
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(URIPARSER DEFAULT_MSG URIPARSER_LIBRARY URIPARSER_INCLUDE_DIR)
IF (URIPARSER_FOUND)
	SET(URIPARSER_LIBRARIES ${URIPARSER_LIBRARY})
	SET(URIPARSER_INCLUDE_DIRS ${URIPARSER_INCLUDE_DIR})
ELSE()
	SET(URIPARSER_LIBRARIES)
	SET(URIPARSER_INCLUDE_DIRS)
ENDIF()
