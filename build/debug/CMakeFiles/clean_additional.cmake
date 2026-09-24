# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/tiffview_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/tiffview_autogen.dir/ParseCache.txt"
  "tests/CMakeFiles/tests_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/tests_autogen.dir/ParseCache.txt"
  "tests/tests_autogen"
  "tiffview_autogen"
  )
endif()
