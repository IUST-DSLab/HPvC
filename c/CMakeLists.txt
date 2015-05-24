cmake_minimum_required(VERSION 2.8)
project(VHPC)

find_package(Threads)

# OVB SDK
include_directories(${PROJECT_SOURCE_DIR}/sdk/bindings/c/include)
add_library(XPCOM ${PROJECT_SOURCE_DIR}/sdk/bindings/xpcom/lib/VirtualBox_i.c)
include_directories(${PROJECT_SOURCE_DIR}/sdk/bindings/xpcom/include)
add_library(VBoxCAPIGlue ${PROJECT_SOURCE_DIR}/sdk/bindings/c/glue/VBoxCAPIGlue.c)
include_directories(${PROJECT_SOURCE_DIR}/sdk/bindings/c/glue)

# Utils
include_directories(${PROJECT_SOURCE_DIR}/utils)
#include_directories(${PROJECT_SOURCE_DIR}/executer)
#include_directories(${PROJECT_SOURCE_DIR}/organizer)


add_subdirectory(utils)
add_subdirectory(executer)
add_subdirectory(organizer)
add_subdirectory(monitor)

link_directories(
  /usr/local/lib
)

#add_executable(vhpc vhpc.c)
#target_link_libraries(vhpc LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} czmq utils executer organizer XPCOM VBoxCAPIGlue)
