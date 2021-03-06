cmake_minimum_required(VERSION 2.8)
project(buildserver)

if(UNIX)
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		add_definitions("-std=c++1y")
	else()
		execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
		if(GCC_VERSION VERSION_GREATER 4.7)
			add_definitions(-std=c++1y)
		else()
			add_definitions(-std=c++0x)
		endif()
	endif()
	add_definitions("-Wall -Wextra -Wconversion -Wvla")
	add_definitions("-Wno-missing-field-initializers")

	option(BUILDSERVER_PEDANTIC "pass -pedantic to the compiler (may generate useless warnings)" OFF)
	if(BUILDSERVER_PEDANTIC)
		add_definitions("-Wpedantic")
	endif()
endif()

if(MSVC)
	#make Visual C++ 2013 build in parallel
	add_definitions("/MP")

	#we want more warnings
	add_definitions("/W4")

	#disable "'std::_Copy_n': Function call with parameters that may be unsafe"
	add_definitions("/wd4996")

	#workaround for Boost 1.55 Context error LNK2026
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
endif()

set(conanbuildinfo ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
if(EXISTS ${conanbuildinfo})
	include(${conanbuildinfo})
	conan_basic_setup()
	set(Boost_LIBRARIES ${CONAN_LIBS})
else()
	if(MSVC)
		set(Boost_USE_STATIC_LIBS ON) 
	endif()
	if(NOT MSVC)
		set(SILICIUM_BOOST_REQUIRED REQUIRED)
	endif()
	find_package(Boost ${SILICIUM_BOOST_REQUIRED} COMPONENTS unit_test_framework thread filesystem system coroutine context program_options date_time regex chrono)
	if(UNIX)
		add_definitions("-DBOOST_TEST_DYN_LINK")
	endif()
	include_directories(SYSTEM ${Boost_INCLUDE_DIR})
	link_directories(${Boost_LIBRARY_DIR})
endif()

if(WIN32)
	add_definitions("-D_WIN32_WINNT=0x0600")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

set(SILICIUM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/silicium" CACHE PATH "")
find_package(Silicium REQUIRED)
include_directories(${SILICIUM_INCLUDE_DIR})

set(VENTURA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/ventura" CACHE PATH "")
include_directories(${VENTURA_INCLUDE_DIR})

find_package(Lua51)
if(Lua51_FOUND)
	include_directories(${LUA_INCLUDE_DIR})
	set(LUACPP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/lua-cpp" CACHE PATH "")
	find_package(LuaCpp REQUIRED)
	include_directories(${LUACPP_INCLUDE_DIRS})
endif()

find_package(UriParser)
if(URIPARSER_FOUND)
	include_directories(SYSTEM ${URIPARSER_INCLUDE_DIR})
endif()

include_directories(".")
add_subdirectory("server")
add_subdirectory("server-cli")
add_subdirectory("examples")
add_subdirectory("tyroxx-ci")
add_subdirectory("test")

if(WIN32)
	set(BUILDSERVER_CLANG_FORMAT "C:/Program Files/LLVM/bin/clang-format.exe" CACHE TYPE PATH)
else()
	set(BUILDSERVER_CLANG_FORMAT "clang-format-3.7" CACHE TYPE PATH)
endif()
file(GLOB_RECURSE formatted examples/*.cpp server/*.hpp server/*.cpp server-cli/*.cpp test/*.cpp nanoweb/*.hpp)
add_custom_target(clang-format COMMAND ${BUILDSERVER_CLANG_FORMAT} -i ${formatted} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
