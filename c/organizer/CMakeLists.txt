#add_library (organizer organizer.c ${PROJECT_SOURCE_DIR}/utils/utils.c)

include_directories (organizer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} )

add_executable(organizer organizer.c)
target_link_libraries(organizer LINK_PUBLIC ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS} czmq utils protobuf-c)
