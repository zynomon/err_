# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "MinSizeRel")
  file(REMOVE_RECURSE
  "CMakeFiles/err__autogen.dir/AutogenUsed.txt"
  "CMakeFiles/err__autogen.dir/ParseCache.txt"
  "err__autogen"
  )
endif()
