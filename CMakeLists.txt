# libsol cmake build file
cmake_minimum_required(VERSION 2.6)
project(solc C)

# build for C99
add_definitions(-std=c99)

# load extra modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

# define sources and headers
set (SOLC_SOURCES
    main.c
    solc.c
    solcobj.c
    solgen.c)
set (SOLC_PUBLIC_HEADERS
    )
set (SOLC_PRIVATE_HEADERS
    solc.h
    solcobj.h
    solgen.h)

# create executable
add_executable(solc ${SOLC_SOURCES} ${SOLC_PUBLIC_HEADERS} ${SOLC_PRIVATE_HEADERS})

# install executable
install(TARGETS solc RUNTIME DESTINATION bin)
