#add_library(monitor monitor.c ${PROJECT_SOURCE_DIR}/utils/utils.c)

include_directories(executer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(monitor monitor.c)
target_link_libraries(monitor LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} zmq czmq utils XPCOM VBoxCAPIGlue protobuf-c)
add_executable(server server.c)
target_link_libraries(server LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} zmq czmq utils XPCOM VBoxCAPIGlue protobuf-c)
add_executable(apiTest apiTest.c)
target_link_libraries(apiTest LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} zmq czmq utils protobuf-c)
add_executable(interface interface.c)
target_link_libraries(interface LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} zmq czmq utils protobuf-c)