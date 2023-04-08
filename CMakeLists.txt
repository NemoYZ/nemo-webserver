cmake_minimum_required(VERSION 3.2)

project(nemo)

enable_language(C ASM)

include(cmake/utils.cmake)

string(CONCAT CXX_FLAGS
    "-rdynamic " 
    "-Og "
    "-ggdb "
    "-std=c++20 "
    "-Wno-deprecated "
    "-Werror "
    "-Wall "
    "-Wno-unused-variable "
    "-Wno-unused-function "
    "-Wno-unused-but-set-variable "
    "-Wno-builtin-macro-redefined"
)

file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/common/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/container/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/context/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/coroutine/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/log/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/net/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/system/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/util/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/db/*.cc")
list(APPEND LIB_SRC ${srcs})
file(GLOB_RECURSE srcs "${PROJECT_SOURCE_DIR}/src/orm/*.cc")
list(APPEND LIB_SRC ${srcs})

ragel_maker(${PROJECT_SOURCE_DIR}/src/net/uri.rl 
            LIB_SRC 
            ${PROJECT_SOURCE_DIR}/src/net)
ragel_maker(${PROJECT_SOURCE_DIR}/src/net/http/http11_parser.rl 
            LIB_SRC 
            ${PROJECT_SOURCE_DIR}/src/net/http)
ragel_maker(${PROJECT_SOURCE_DIR}/src/net/http/http_client_parser.rl 
            LIB_SRC 
            ${PROJECT_SOURCE_DIR}/src/net/http)

add_library(nemo SHARED ${LIB_SRC})
force_redefine_file_macro_for_sources(nemo) #__FILE__

set(CMAKE_CXX_FLAGS ${CXX_FLAGS})

include_directories(${PROJECT_SOURCE_DIR}/third_party/include)
include_directories(${PROJECT_SOURCE_DIR}/include)
link_directories(${PROJECT_SOURCE_DIR}/bin)
link_directories(${PROJECT_SOURCE_DIR}/third_party/lib)
link_directories(${PROJECT_SOURCE_DIR}/third_party/lib64)
link_directories(${PROJECT_SOURCE_DIR}/third_party/lib64/mysql)

find_library(YAMLCPP yaml-cpp)
find_library(PTHREAD pthread)
find_library(JSONCPP jsoncpp)
find_library(SSL ssl)
find_library(MYSQL mysqlclient)

set(LIBS
    nemo
    pthread
    dl
    ssl
    jsoncpp
    mysqlclient
    yaml-cpp
    fcontext
    protobuf
    soci_core
    soci_mysql
    tinyxml2
)

nemo_add_tests(${PROJECT_SOURCE_DIR}/tests nemo "${LIBS}")
nemo_add_executable(main ${PROJECT_SOURCE_DIR}/src/main.cc nemo "${LIBS}")

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)