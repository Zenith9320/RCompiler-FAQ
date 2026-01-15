# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(RCompiler C CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加可执行文件
add_executable(code
    main.cpp
    src/lexer.cpp
    # 添加其他源文件...
)

# 包含目录
target_include_directories(code PRIVATE include)

# 如果需要链接库（如boost）
find_package(Boost COMPONENTS regex REQUIRED)
target_link_libraries(code PRIVATE Boost::regex)