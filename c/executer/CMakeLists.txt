#add_library(executer executer.c ${PROJECT_SOURCE_DIR}/utils/utils.c)

include_directories(executer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(executer executer.c)
target_link_libraries(executer LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} czmq utils XPCOM VBoxCAPIGlue protobuf-c)
