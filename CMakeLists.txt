cmake_minimum_required(VERSION 3.0)
project(limboole
    VERSION 1.2)

set(VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# =============================================
# Options
# =============================================

option(USE_PICOSAT "download and use the picosat solver as SAT solving backend" ON)
option(USE_LINGELING "download and use the lingeling solver as SAT solving backend" ON)

set(PICOSAT_URL "http://fmv.jku.at/picosat/picosat-965.tar.gz"
    CACHE STRING "URL of the picosat tar.gz to use")
set(PICOSAT_SHA512
    "b5372962a0079d70641fd76c431b4ab5042c2f3015179762e447634f220cf0744eea786ee40b00035a342f3ead482061ebe1a9e3726bd06e7adf8e0ac0c5eabf"
    CACHE STRING "SHA512 hash of the picosat tar.gz file")
set(LINGELING_URL "http://fmv.jku.at/lingeling/lingeling-bcj-78ebb86-180517.tar.gz"
    CACHE STRING "URL of the lingeling tar.gz to use")
set(LINGELING_SHA512
    "ad20522f2dd86fe1a12f6e1fc09f1f88d97ac191eb2ee57d16aee3fcad4b045e400179470f872e0552a456f3a06f41b5c6d423acdcbe6bc273e051b18fe4d281"
    CACHE STRING "SHA512 hash of the lingeling tar.gz file")

# =============================================
# External Project Setup
# =============================================

include(ExternalProject)

ExternalProject_Add(picosat_project
    PREFIX picosat
    URL ${PICOSAT_URL}
    URL_HASH SHA512=${PICOSAT_SHA512}

    CONFIGURE_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project && ./configure.sh
    BUILD_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project && make
    INSTALL_COMMAND ""
)

ExternalProject_Add(lingeling_project
    PREFIX lingeling
    URL ${LINGELING_URL}
    URL_HASH SHA512=${LINGELING_SHA512}

    CONFIGURE_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/lingeling/src/lingeling_project && ./configure.sh
    BUILD_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/lingeling/src/lingeling_project && make
    INSTALL_COMMAND ""
)

# =============================================
# Setup limboole sources, executables and tests.
# =============================================

set(LIMBOOLE_EXECUTABLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/limboole.c
    )

set(TESTLIMBOOLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/limboole.c
    )

set(DIMACS2BOOLE_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/dimacs2boole.c
    )

add_executable(limboole ${LIMBOOLE_EXECUTABLE_SRCS})

add_executable(testlimboole ${TESTLIMBOOLE_SRCS})

add_executable(dimacs2boole ${DIMACS2BOOLE_SRCS})

# =============================================
# Setup VERSION
# =============================================

add_compile_definitions(VERSION="${VERSION}")

# =============================================
# Setup SAT backend linkage.
# =============================================

if(USE_PICOSAT)
    add_library(picosat STATIC IMPORTED)
    set_target_properties(picosat PROPERTIES IMPORTED_LOCATION
	${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project/libpicosat.a)
    target_compile_definitions(picosat INTERFACE "-DLIMBOOLE_USE_PICOSAT")
    target_include_directories(picosat INTERFACE
	${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project/)
    
    target_link_libraries(limboole picosat)
    target_link_libraries(testlimboole picosat)
    
    add_dependencies(limboole picosat_project)
    add_dependencies(testlimboole picosat_project)
    add_dependencies(dimacs2boole picosat_project)
endif()

if(USE_LINGELING)
    add_library(lingeling STATIC IMPORTED)
    set_target_properties(lingeling PROPERTIES IMPORTED_LOCATION
	${CMAKE_CURRENT_BINARY_DIR}/lingeling/src/lingeling_project/liblgl.a)
    target_compile_definitions(lingeling INTERFACE "-DLIMBOOLE_USE_LINGELING")
    target_include_directories(lingeling INTERFACE
	${CMAKE_CURRENT_BINARY_DIR}/lingeling/src/lingeling_project/)
    
    target_link_libraries(limboole lingeling m)
    target_link_libraries(testlimboole lingeling m)
    
    add_dependencies(limboole lingeling_project)
    add_dependencies(testlimboole lingeling_project)
    add_dependencies(dimacs2boole lingeling_project)
endif()
