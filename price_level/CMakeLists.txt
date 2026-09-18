cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(CMAKE_CXX_STANDARD 26)

project(price_level)
set(PROJECT_VERSION 1.0.0.0)
project(${PROJECT_NAME} VERSION ${PROJECT_VERSION} LANGUAGES CXX)

add_compile_options(-c -Wall -O3 -std=c++26)

add_executable(${PROJECT_NAME}
        main.cpp
        OrderPool_Indexed.hpp
        Order.hpp
        OrderPool.hpp
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_SOURCE_DIR}/utilities")
target_link_directories(${PROJECT_NAME} PUBLIC "${CMAKE_BINARY_DIR}/utilities")

TARGET_LINK_LIBRARIES(${PROJECT_NAME}
        utils
        pthread
        ${EXTRA_LIBS}
)