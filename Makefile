CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Iinclude
# 添加 Boost 头文件路径（根据你的安装位置调整）
BOOST_INCLUDE = -I/usr/include/boost
# 添加 Boost 库链接
BOOST_LIBS = -lboost_regex

TARGET = compiler

# 源文件
SRCS = main.cpp $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)

# 默认目标
all: build

# 构建目标
build: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(BOOST_INCLUDE) -o $@ $^ $(BOOST_LIBS)

# 编译源文件
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(BOOST_INCLUDE) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all build run clean