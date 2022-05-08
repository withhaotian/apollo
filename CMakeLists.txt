cmake_minimum_required(VERSION 2.8)
project(apollo)

include(${CMAKE_SOURCE_DIR}/cmake/utils.cmake)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O0 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function -Wno-builtin-macro-redefined")

include_directories(.)
link_directories(/apps/sylar/lib)

set(LIB_SRC
	src/log.cc
	src/util.cc
	src/config.cc
	src/thread.cc
)

add_library(apollo SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(apollo)  # __FILE__
find_library(YAMLCPP yaml-cpp)

set(LIBS
	apollo
    yaml-cpp
	pthread
    )

add_executable(test tests/test.cc)
force_redefine_file_macro_for_sources(test)  # __FILE__
add_dependencies(test apollo)
target_link_libraries(test ${LIBS})

add_executable(test_config tests/test_config.cc)
force_redefine_file_macro_for_sources(test_config)  # __FILE__
add_dependencies(test_config apollo)
target_link_libraries(test_config ${LIBS})

add_executable(test_thread tests/test_thread.cc)
force_redefine_file_macro_for_sources(test_thread)  # __FILE__
add_dependencies(test_thread apollo)
target_link_libraries(test_thread ${LIBS})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
