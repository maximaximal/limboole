set(LIBQDPLL_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/qdpll.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qdpll_pqueue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qdpll_mem.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qdpll_dep_man_qdag.c
    ${CMAKE_CURRENT_SOURCE_DIR}/qdpll_dynamic_nenofex.c
    )

add_library(qdpll STATIC ${LIBQDPLL_SRCS})
target_include_directories(qdpll PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(qdpll PRIVATE picosat nenofex)
