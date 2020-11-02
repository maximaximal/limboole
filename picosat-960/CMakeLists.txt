add_library(picosat STATIC ${CMAKE_CURRENT_SOURCE_DIR}/picosat.c)
target_include_directories(picosat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
