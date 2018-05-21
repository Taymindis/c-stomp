cmake_minimum_required(VERSION 3.0)
project(c_stomp)

set(PROJNAME cstomp)

IF (NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
SET(CMAKE_INSTALL_INCLUDEDIR /usr/local/include)
ENDIF(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)

file(GLOB_RECURSE sources src/main/*.c src/main/*.h)
file(GLOB_RECURSE testfile example/main.c src/main/*.h)
file(GLOB_RECURSE testfile2 example/share_sess_sample.c src/main/*.h)


add_executable(run-test ${sources} ${testfile})
add_executable(run-test2 ${sources} ${testfile2})

target_include_directories(run-test PUBLIC src/main)
target_include_directories(run-test2 PUBLIC src/main)

target_link_libraries(run-test PUBLIC pthread)
target_link_libraries(run-test2 PUBLIC pthread)

include_directories(src/main /usr/local/include)

# SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wstrict-prototypes -Wmissing-prototypes")
# SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-declarations -Wshadow -Wpointer-arith -Wcast-qual")
# SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsign-compare -std=c11 -pedantic")

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

###############################################################################
## packaging ##################################################################
###############################################################################
add_library(${PROJNAME}.static STATIC ${sources})
set_target_properties(${PROJNAME}.static PROPERTIES OUTPUT_NAME ${PROJNAME})
add_library(${PROJNAME} SHARED ${sources})

# link_directories(/usr/local/lib /usr/lib)

## For .a library
install(TARGETS ${PROJNAME}.static EXPORT ${PROJNAME}
		ARCHIVE DESTINATION lib${LIB_SUFFIX}
		LIBRARY DESTINATION lib${LIB_SUFFIX}
		# RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		)

## For .so library
install(TARGETS ${PROJNAME} EXPORT ${PROJNAME}
		ARCHIVE DESTINATION lib${LIB_SUFFIX}
		LIBRARY DESTINATION lib${LIB_SUFFIX}
		# RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		)
install(FILES src/main/cstomp.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(CODE "execute_process(COMMAND ldconfig)")

set(CPACK_PACKAGE_NAME "c-stomp")
set(CPACK_PACKAGE_VERSION "1.0.0")

# we don't want to split our program up into several things
set(CPACK_MONOLITHIC_INSTALL 1)

# This must be last
include(CPack)
