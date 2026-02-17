cmake_minimum_required(VERSION 3.16)
project(nfsv3_toy_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(nfsv3_toy_server
  src/main.cpp
  src/config.cpp
  src/net.cpp
  src/rpc.cpp
  src/server.cpp
  src/worker_pool.cpp
  src/xdr.cpp
)

target_include_directories(nfsv3_toy_server PRIVATE include)

target_compile_options(nfsv3_toy_server PRIVATE
  -Wall -Wextra -Wpedantic
)
