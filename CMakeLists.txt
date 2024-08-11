cmake_minimum_required(VERSION 3.19)


project(message_bus)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SRC)



add_executable(${PROJECT_NAME} ${SRC})