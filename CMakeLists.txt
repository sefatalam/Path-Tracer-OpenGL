cmake_minimum_required(VERSION 3.15)
project(GLRT CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_COMPILER   "C:/msys64/ucrt64/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "C:/msys64/ucrt64/bin/g++.exe")

add_executable(GLRT
    src/main.cpp
    src/glad.c
)

target_include_directories(GLRT PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_directories(GLRT PRIVATE
    ${CMAKE_SOURCE_DIR}/lib
)

target_link_libraries(GLRT PRIVATE
    glfw3
    opengl32
    gdi32
    assimp
)

set(GLRT_RUNTIME_DLLS
    ${CMAKE_SOURCE_DIR}/glfw3.dll
    ${CMAKE_SOURCE_DIR}/libassimp-6.dll
    ${CMAKE_SOURCE_DIR}/zlib1.dll
    ${CMAKE_SOURCE_DIR}/libminizip-1.dll
    ${CMAKE_SOURCE_DIR}/libbz2-1.dll
)

add_custom_command(TARGET GLRT POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GLRT_RUNTIME_DLLS}
        $<TARGET_FILE_DIR:GLRT>
)
