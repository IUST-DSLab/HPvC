add_library (utils utils.c oe.pb-c.c ee.pb-c.c monitor.pb-c.c)

include_directories (utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
