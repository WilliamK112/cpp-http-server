cmake_minimum_required(VERSION 3.16)
project(cpp_http_server VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(http_server
  src/main.cpp
  src/server.cpp
  src/request.cpp
  src/response.cpp
  src/router.cpp
  src/thread_pool.cpp
  src/logger.cpp
  src/config.cpp
  src/static_file_handler.cpp
)

# Threads are required in Phase 3, but linking now keeps setup simple.
find_package(Threads REQUIRED)
target_link_libraries(http_server PRIVATE Threads::Threads)
