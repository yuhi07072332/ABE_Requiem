cmake_minimum_required(VERSION 3.30)

project(genk VERSION 0.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")

file(GLOB SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_executable(genk ${SOURCES})

target_compile_features(genk PRIVATE cxx_std_23)

target_compile_options(genk PRIVATE
    -Wall
    -Wextra
    -Wno-reorder
    -Wno-unused-but-set-variable 
)

if(WIN32)
    target_compile_options(genk PRIVATE
        -lstdc++exp 
    )
endif()
