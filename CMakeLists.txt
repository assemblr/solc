# libsol cmake build file
cmake_minimum_required(VERSION 2.8)
project(solc C)
include_directories("${CMAKE_INSTALL_PREFIX}/include")

# build for C99
add_definitions(-std=c99)

if(${UNIX})
    add_definitions(-D_POSIX_C_SOURCE=200112)
endif()

# load extra modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# define sources and headers
set (LIBSOLC_SOURCES
    solc.c
    solcparse.c
    solcemit.c)
set (LIBSOLC_PUBLIC_HEADERS
    solc.h)
set (LIBSOL_PRIVATE_HEADERS
    )
set (SOLC_SOURCES
    main.c
    solgen.c
    linenoise.c)
set (SOLC_PUBLIC_HEADERS
    )
set (SOLC_PRIVATE_HEADERS
    solgen.h
    linenoise.h)

# create targets
add_library(libsolc SHARED ${LIBSOLC_SOURCES} ${LIBSOLC_PUBLIC_HEADERS} ${LIBSOLC_PRIVATE_HEADERS})
set_target_properties(libsolc PROPERTIES OUTPUT_NAME solc)
find_library(libsol sol)
target_link_libraries(libsolc ${libsol})
find_library(libm m)
target_link_libraries(libsolc ${libm})

add_executable(solc ${SOLC_SOURCES} ${SOLC_PUBLIC_HEADERS} ${SOLC_PRIVATE_HEADERS})
target_link_libraries(solc libsolc)

# load and link PCRE
find_package(PCRE REQUIRED)
include_directories(${PCRE_INCLUDE_DIRS})
target_link_libraries(libsolc ${PCRE_LIBRARIES})

# install targets
install(TARGETS libsolc LIBRARY DESTINATION lib)
install(FILES ${LIBSOLC_PUBLIC_HEADERS} DESTINATION include/solc)
install(TARGETS solc RUNTIME DESTINATION bin)
