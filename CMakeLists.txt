cmake_minimum_required(VERSION 2.8)
project(apollo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_FLAGS "$ENV{CXXFLAGS} -rdynamic -O3 -fPIC -ggdb -std=c++11 -Wall -Wno-deprecated -Werror -Wno-unused-function")

include_directories(.)
link_directories(/apps/sylar/lib)

set(LIB_SRC
	src/log.cc
	src/util.cc
	src/config.cc
)

add_library(apollo SHARED ${LIB_SRC})
find_library(YAMLCPP yaml-cpp)

set(LIBS
	apollo
    yaml-cpp
    )

add_executable(test tests/test.cc)
add_dependencies(test apollo)
target_link_libraries(test ${LIBS})

add_executable(test_config tests/test_config.cc)
add_dependencies(test_config apollo)
target_link_libraries(test_config ${LIBS})

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
