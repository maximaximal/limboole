cmake_minimum_required(VERSION 3.0)
project(limboole
    VERSION 1.2)

set(VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# =============================================
# Setup limboole sources, executables and tests.
# =============================================

set(LIMBOOLE_EXECUTABLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/limboole.c
    )

set(DIMACS2BOOLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/dimacs2boole.c
    )

add_executable(limboole ${LIMBOOLE_EXECUTABLE_SRCS})

add_executable(dimacs2boole ${DIMACS2BOOLE_SRCS})

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -Os)

set_target_properties(limboole PROPERTIES COMPILE_DEFINITIONS "LIMBOOLE_USE_LINGELING")
set_target_properties(dimacs2boole PROPERTIES COMPILE_DEFINITIONS "LIMBOOLE_USE_LINGELING")

# =============================================
# Setup VERSION
# =============================================

add_definitions(-DVERSION="${VERSION}")

# =============================================
# Setup SAT backend linkage.
# =============================================

add_subdirectory(lingeling-bcj-78ebb86-180517)

target_link_libraries(limboole lgl)

set_target_properties(limboole PROPERTIES LINK_FLAGS "-s INVOKE_RUN=0 -s MODULARIZE=1 -s EXIT_RUNTIME=0 -s EXPORT_NAME='createLimbooleModule' -s EXPORTED_FUNCTIONS=['_limboole','_main'] -s EXTRA_EXPORTED_RUNTIME_METHODS=['FS','callMain','cwrap']")
set_target_properties(dimacs2boole PROPERTIES LINK_FLAGS "-s INVOKE_RUN=0 -s EXIT_RUNTIME=0 -s MODULARIZE=1 -s EXPORT_NAME='createDimacsToBooleModule' -s EXTRA_EXPORTED_RUNTIME_METHODS=['FS','callMain']")

# =============================================
# Install all targets
# =============================================

set(INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS limboole
    RUNTIME DESTINATION ${INSTALL_DIR}
    LIBRARY DESTINATION ${INSTALL_DIR}
    ARCHIVE DESTINATION ${INSTALL_DIR})

install(TARGETS dimacs2boole
    RUNTIME DESTINATION ${INSTALL_DIR}
    LIBRARY DESTINATION ${INSTALL_DIR}
    ARCHIVE DESTINATION ${INSTALL_DIR})

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/README
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    ${CMAKE_CURRENT_SOURCE_DIR}/index.html
    DESTINATION ${INSTALL_DIR})
