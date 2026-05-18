cmake_minimum_required(VERSION 3.16)
project(chacha2020)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(chacha2020
    main.cpp
    glad.c
    depdencies/include/imgui/imgui.cpp
    depdencies/include/imgui/imgui_draw.cpp
    depdencies/include/imgui/imgui_tables.cpp
    depdencies/include/imgui/imgui_widgets.cpp
    depdencies/include/imgui/imgui_stdlib.cpp
    depdencies/include/imgui/backends/imgui_impl_glfw.cpp
    depdencies/include/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(chacha2020 PRIVATE
    depdencies/include
    glad/include
)

target_link_libraries(chacha2020
    OpenGL::GL
    glfw
    OpenSSL::Crypto
    dl
)
