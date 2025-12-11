cmake_minimum_required(VERSION 3.26)
project(limboole
    VERSION 1.3.2)

set(VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH})

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

target_compile_definitions(limboole PRIVATE LIMBOOLE_USE_PICOSAT LIMBOOLE_USE_DEPQBF)
target_compile_definitions(dimacs2boole PRIVATE LIMBOOLE_USE_PICOSAT LIMBOOLE_USE_DEPQBF)

# =============================================
# Setup VERSION
# =============================================

add_definitions(-DVERSION="${VERSION}")

# =============================================
# Setup SAT backend linkage.
# =============================================

add_subdirectory(picosat-960)
add_subdirectory(nenofex-version-1.1)
add_subdirectory(depqbf-version-6.03)

target_link_libraries(limboole picosat qdpll)

if(CMAKE_CXX_COMPILER MATCHES "/em\\+\\+(-[a-zA-Z0-9.])?$")
    set_target_properties(limboole PROPERTIES LINK_FLAGS "-s INVOKE_RUN=0 -s MODULARIZE=1 -s EXIT_RUNTIME=0 -s EXPORT_NAME='createLimbooleModule' -s EXPORTED_FUNCTIONS=['_limboole_extended','_main'] -s EXTRA_EXPORTED_RUNTIME_METHODS=['FS','callMain','cwrap']")
    set_target_properties(dimacs2boole PROPERTIES LINK_FLAGS "-s INVOKE_RUN=0 -s EXIT_RUNTIME=0 -s MODULARIZE=1 -s EXPORT_NAME='createDimacsToBooleModule' -s EXTRA_EXPORTED_RUNTIME_METHODS=['FS','callMain']")
endif()

# =============================================
# Install all targets
# =============================================

if(SKBUILD)
  target_compile_definitions(picosat PUBLIC NGETRUSAGE)
  install(TARGETS limboole DESTINATION "${SKBUILD_SCRIPTS_DIR}")
  install(TARGETS dimacs2boole DESTINATION "${SKBUILD_SCRIPTS_DIR}")
else()
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
endif()


# =============================================
# Setup test target
# =============================================
  
include(CTest)
