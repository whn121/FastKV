cmake_minimum_required(VERSION 3.16)

project(FastKV)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(fastkv
    src/main.cpp
    src/KVStore.cpp
    src/MemTable.cpp
    src/SSTable.cpp
    src/WAL.cpp
)

target_include_directories(fastkv
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)