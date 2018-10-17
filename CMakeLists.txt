cmake_minimum_required(VERSION 3.0)
project(limboole
    VERSION 1.2)

set(VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR})

# =============================================
# Options
# =============================================

option(USE_PICOSAT "download and use the picosat solver as SAT solving backend" ON)
option(USE_LINGELING "download and use the lingeling solver as SAT solving backend" ON)

option(PACKAGE_DEB "package software as .deb if built on linux" ON)
option(PACKAGE_TGZ "package software as .tar.gz if built on linux" ON)
option(PACKAGE_RPM "package software as .rpm if built on linux" OFF)
option(PACKAGE_INSTALLER "package software as .exe installer if built for windows" OFF)
option(PACKAGE_ZIP "package software as .zip if built for windows" ON)

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

if(USE_PICOSAT)
    ExternalProject_Add(picosat_project
	PREFIX picosat
	URL ${PICOSAT_URL}
	URL_HASH SHA512=${PICOSAT_SHA512}

	# The following patch enables picosat to
	# be built using cross compilers on windows.
	PATCH_COMMAND cat ${CMAKE_CURRENT_SOURCE_DIR}/picosat_sigalrm_patch.c | cat - ${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project/app.c > /tmp/cat_tmp && mv /tmp/cat_tmp ${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project/app.c

	CONFIGURE_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project && ./configure.sh
	BUILD_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project && make all
	INSTALL_COMMAND ""
	)
endif()

if(USE_LINGELING)
    ExternalProject_Add(lingeling_project
	PREFIX lingeling
	URL ${LINGELING_URL}
	URL_HASH SHA512=${LINGELING_SHA512}

	CONFIGURE_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/lingeling/src/lingeling_project && ./configure.sh
	BUILD_COMMAND cd ${CMAKE_CURRENT_BINARY_DIR}/lingeling/src/lingeling_project && make
	INSTALL_COMMAND ""
	)
endif()

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

add_definitions(-DVERSION="${VERSION}")

# =============================================
# Setup SAT backend linkage.
# =============================================

if(USE_PICOSAT)
    add_library(picosat STATIC IMPORTED)
    set_target_properties(picosat PROPERTIES IMPORTED_LOCATION
	${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project/libpicosat.a)
    set_target_properties(picosat PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${CMAKE_CURRENT_BINARY_DIR}/picosat/src/picosat_project/)
    set_target_properties(picosat PROPERTIES INTERFACE_COMPILE_DEFINITIONS
	"LIMBOOLE_USE_PICOSAT")

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
    set_target_properties(lingeling PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
	${CMAKE_CURRENT_BINARY_DIR}/lingeling/src/lingeling_project/)
    set_target_properties(lingeling PROPERTIES INTERFACE_COMPILE_DEFINITIONS
	"LIMBOOLE_USE_LINGELING")
    
    target_link_libraries(limboole lingeling m)
    target_link_libraries(testlimboole lingeling m)
    
    add_dependencies(limboole lingeling_project)
    add_dependencies(testlimboole lingeling_project)
    add_dependencies(dimacs2boole lingeling_project)
endif()

# =============================================
# Install all targets
# =============================================

if(WIN32)
    set(CMAKE_INSTALL_BINDIR "./")
    set(CMAKE_INSTALL_LIBDIR "./")
    set(CMAKE_INSTALL_INFODIR "./")
    set(INFO_SUBDIR "")
else()
    include(GNUInstallDirs)
    set(INFO_SUBDIR "limboole/")
endif()

install(TARGETS limboole
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static)

install(TARGETS testlimboole
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static)

install(TARGETS dimacs2boole
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/README
    ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION ${CMAKE_INSTALL_INFODIR}/${INFO_SUBDIR})

# =============================================
# Setup CPack
# =============================================

set(CPACK_GENERATOR "")

if(WIN32)
    if(PACKAGE_ZIP)
	set(CPACK_GENERATOR ${CPACK_GENERATOR} ZIP)
    endif()
    if(PACKAGE_INSTALLER)
	set(CPACK_GENERATOR ${CPACK_GENERATOR} NSIS)
    endif()
else()
    if(PACKAGE_DEB)
	set(CPACK_GENERATOR ${CPACK_GENERATOR} DEB)
    endif()
    if(PACKAGE_TGZ)
	set(CPACK_GENERATOR ${CPACK_GENERATOR} TGZ)
    endif()
    if(PACKAGE_RPM)
	set(CPACK_GENERATOR ${CPACK_GENERATOR} RPM)
    endif()
endif()

set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "a simple tool for checking satisfiability respectively tautology on arbitrary structural formulas")
set(CPACK_PACKAGE_NAME "limboole")
set(CPACK_PACKAGE_VENDOR "FMV, JKU")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_RESSOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_RESSOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README)
set(CPACK_OUTPUT_FILE_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/packages)
set(CPACK_PACKAGE_CONTACT "Max Heisinger <maximilian.heisinger@jku.at>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "")

include(CPack)
