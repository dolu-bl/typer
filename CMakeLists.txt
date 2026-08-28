cmake_minimum_required(VERSION 3.15.7)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_PROJECT_TOP_LEVEL_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/conan_provider.cmake
)

project(typer)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED COMPONENTS program_options)
find_package(nlohmann_json REQUIRED)

set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/typer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/terminal.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${Boost_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    Boost::program_options
    nlohmann_json::nlohmann_json
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/
)
