cmake_minimum_required(VERSION 2.8)
project(apollo)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")

include_directories(.)
link_directories(/apps/sylar/lib)

set(LIB_SRC
	src/address.cc
	src/config.cc
	src/fdmanager.cc
	src/fiber.cc
	src/hook.cc
	src/iomanager.cc
	src/log.cc
	src/mutex.cc
	src/scheduler.cc
	src/thread.cc
	src/timer.cc
	src/util.cc
)

add_library(apollo SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(apollo)  # __FILE__
find_library(YAMLCPP yaml-cpp)
find_library(PTHREAD pthread)
#set(LIBS ${LIBS} sylar)
#set(LIBS ${LIBS} dl)
#set(LIBS ${LIBS} ${YAMLCPP})
#set(LIBS ${LIBS} ${PTHREAD})

set(LIBS
		apollo
		dl
		yaml-cpp
		pthread
    )

add_executable(test_log tests/test_log.cc)
force_redefine_file_macro_for_sources(test)  # __FILE__
add_dependencies(test_log apollo)
target_link_libraries(test_log ${LIBS})

add_executable(test_config tests/test_config.cc)
force_redefine_file_macro_for_sources(test_config)  # __FILE__
add_dependencies(test_config apollo)
target_link_libraries(test_config ${LIBS})

add_executable(test_thread tests/test_thread.cc)
force_redefine_file_macro_for_sources(test_thread)  # __FILE__
add_dependencies(test_thread apollo)
target_link_libraries(test_thread ${LIBS})

add_executable(test_util tests/test_util.cc)
force_redefine_file_macro_for_sources(test_util)  # __FILE__
add_dependencies(test_util apollo)
target_link_libraries(test_util ${LIBS})

add_executable(test_fiber tests/test_fiber.cc)
force_redefine_file_macro_for_sources(test_fiber)  # __FILE__
add_dependencies(test_fiber apollo)
target_link_libraries(test_fiber ${LIBS})

add_executable(test_scheduler tests/test_scheduler.cc)
force_redefine_file_macro_for_sources(test_scheduler)  # __FILE__
add_dependencies(test_scheduler apollo)
target_link_libraries(test_scheduler ${LIBS})

add_executable(test_iomanager tests/test_iomanager.cc)
force_redefine_file_macro_for_sources(test_iomanager)  # __FILE__
add_dependencies(test_iomanager apollo)
target_link_libraries(test_iomanager ${LIBS})

add_executable(test_hook tests/test_hook.cc)
force_redefine_file_macro_for_sources(test_hook)  # __FILE__
add_dependencies(test_hook apollo)
target_link_libraries(test_hook ${LIBS})

add_executable(test_address tests/test_address.cc)
force_redefine_file_macro_for_sources(test_address)  # __FILE__
add_dependencies(test_address apollo)
target_link_libraries(test_address ${LIBS})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
