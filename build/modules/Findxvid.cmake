if(WIN32)
  find_path(XVID_INCLUDE_DIR xvid.h $ENV{PROGRAMFILES}/XviD/include)
  find_library(XVID_LIBRARY NAMES xvidcore PATHS $ENV{PROGRAMFILES}/XviD/lib)
else()
  find_path(XVID_INCLUDE_DIR xvid.h PATHS /usr/include
        ../../../libs/include
        ../../libs/include
        ../../include)
  find_library(XVID_LIBRARY NAMES xvidcore PATHS /usr/lib /usr/local/lib  
      ../../../libs/lib
      ../../libs/lib
      ../../lib)
endif()

if(XVID_INCLUDE_DIR AND XVID_LIBRARY)
  set(XVID_LIBRARIES ${XVID_LIBRARY})
  set(XVID_INCLUDE_DIRS ${XVID_INCLUDE_DIR})
  set(XVID_FOUND 1)
else()
  set(XVID_INCLUDE_DIRS)
  set(XVID_LIBRARIES)
  set(XVID_FOUND 0)
endif()

if(NOT XVID_FOUND)
 message(FATAL_ERROR " libxvidcore was not found.")
else()
  message(STATUS "Found libxvid.")
endif()