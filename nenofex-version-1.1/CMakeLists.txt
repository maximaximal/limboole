set(NENOFEX_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/nenofex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/stack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/atpg.c
    )

add_library(nenofex STATIC ${NENOFEX_SRCS})
target_include_directories(nenofex PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nenofex picosat)
