find_path(SCOTCH_INCLUDE_DIRS NAMES scotch.h HINTS ${SCOTCH_DIR}/include)
find_library(SCOTCH_LIBRARIES NAMES scotch HINTS ${SCOTCH_DIR}/lib)

# Handle the QUIETLY and REQUIRED arguments and set SCOTCH_FOUND to TRUE if
# all listed variables are TRUE.
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(Scotch DEFAULT_MSG SCOTCH_LIBRARIES SCOTCH_INCLUDE_DIRS)

MARK_AS_ADVANCED(SCOTCH_INCLUDE_DIRS SCOTCH_LIBRARIES)