add_library(lgl STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/lglib.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lglbnr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lgldimacs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/lglopts.c
    )

string(TIMESTAMP TODAY "%Y%m%d")

target_compile_definitions(lgl PRIVATE "-DNLGLYALSAT -DNLGLDRUPLIG")

target_include_directories(lgl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(lgl m)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lglcfg.h.in ${CMAKE_CURRENT_BINARY_DIR}/lglcfg.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/lglcflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/lglcflags.h)
target_include_directories(lgl PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
