cmake_minimum_required(VERSION 2.6)

set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}")

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_C_FLAGS_DEBUG "-g3 -Wall")
#set(CMAKE_BUILD_TYPE "Release")
#set(CMAKE_C_FLAGS "-Wall")

find_path(SIMGRID_INCLUDE
	NAMES simgrid/msg.h
	PATHS /usr/include
		  /usr/local/include
		  /opt/simgrid/include
)

include_directories(
	${SIMGRID_INCLUDE}
)

add_executable(hpvc hpvc.c)

#finding simgrid library
find_library(SIMGRID
	NAMES simgrid
	PATHS /opt/simgrid/lib
		  /usr/lib/
		  /usr/lib64/
		  /usr/local/lib/
		  /usr/local/lib64/
)

#Finding math library
find_library(LIBMATH
	NAMES m
)

### Add definitions for compile
target_link_libraries(hpvc ${LIBMATH} ${SIMGRID})
