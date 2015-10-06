cmake_minimum_required(VERSION 3.2)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
if(NOT DEFINED CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
endif()

project(arcd)

set(INSTALL_INCLUDE_DIR include CACHE PATH
	"Installation directory for header files")
set(INSTALL_LIB_DIR lib CACHE PATH
	"Installation directory for libraries")
set(INSTALL_CMAKE_DIR lib/cmake/arcd CACHE PATH
	"Installation directory for CMake files")
option(ARCD_CONFIGURE_INSTALL "Generate install target" ON)
option(ARCD_EXAMPLES "Build examples" OFF)
option(ARCD_TESTS "Build tests" OFF)

add_subdirectory(arcd)

if(ARCD_EXAMPLES)
	add_subdirectory(examples)
endif()
if(ARCD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()

if(ARCD_CONFIGURE_INSTALL)
	export(EXPORT arcd)
	install(EXPORT arcd
		DESTINATION ${INSTALL_CMAKE_DIR})
	configure_file(arcd-config.cmake.in arcd-config.cmake @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/arcd-config.cmake
		DESTINATION ${INSTALL_CMAKE_DIR})
endif()
