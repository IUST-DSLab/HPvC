add_library(executer executer.c ${PROJECT_SOURCE_DIR}/utils/utils.c)

include_directories(executer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
