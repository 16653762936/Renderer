# 这是 CMake 的说明书，不是 C++ 源码。CMake 读它，再去叫 MSVC 真正编译。

cmake_minimum_required(VERSION 3.20) # 太老的 CMake 看不懂后面的语法就退出
project(Renderer LANGUAGES C CXX)    # 工程名；C 是因为 GLAD 的 gl.c 是 C 文件

set(CMAKE_CXX_STANDARD 17)           # 用 C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 编译器不支持 C++17 就报错，别悄悄降级

# FetchContent：编译时自动从 GitHub 拉 GLFW
include(FetchContent)
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# GLM：header-only 的向量/矩阵库。阶段 4 因 GitHub 拉不下来才手写 LookAt/透视，现在改回来。
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glm)

# GLAD：把「glDrawArrays 这些函数在驱动里的地址」加载进来。Windows 上必须有它才能用现代 OpenGL。
add_library(glad STATIC third_party/glad/src/gl.c)
target_include_directories(glad PUBLIC third_party/glad/include)

add_executable(renderer src/main.cpp)
target_link_libraries(renderer PRIVATE glad glfw glm::glm opengl32)

if(MSVC)
    target_compile_options(renderer PRIVATE /utf-8)
endif()
