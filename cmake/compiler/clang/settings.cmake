# Set build-directive (used in core to tell which buildtype we used)
add_definitions(-D_BUILD_DIRECTIVE='"${CMAKE_BUILD_TYPE}"')

add_definitions(-fno-delete-null-pointer-checks)
add_definitions(-stdlib=libc++)

set(CMAKE_EXE_LINKER_FLAGS "-stdlib=libc++ -lc++abi")

if(PLATFORM EQUAL 32)
  # Required on 32-bit systems to enable SSE2 (standard on x64)
  add_definitions(-msse2 -mfpmath=sse)
endif()
add_definitions(-DHAVE_SSE2 -D__SSE2__)
message(STATUS "CLANG: SSE2 flags forced")

if( WITH_WARNINGS )
  add_definitions(-Wall -Wfatal-errors -Wextra)
  message(STATUS "CLANG: All warnings enabled")
else()
  add_definitions(-w)
  message(STATUS "CLANG: All warnings disabled")
endif()

# if( WITH_COREDEBUG )
#  add_definitions(-ggdb3)
#  message(STATUS "CLANG: Debug-flags set (-ggdb3)")
# endif()
