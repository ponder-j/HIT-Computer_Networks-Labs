# Makefile for HTTP Proxy Server v3.0
# Windows MinGW compilation

# 编译器设置
CXX = g++
CXXFLAGS = -Wall -std=c++11 -I./include -fexec-charset=GBK -finput-charset=UTF-8
LDFLAGS = -lws2_32

# 目录设置
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# 目标可执行文件
TARGET = $(BIN_DIR)/ProxyServer.exe

# 源文件
SOURCES = $(SRC_DIR)/main.cpp \
          $(SRC_DIR)/http_parser.cpp \
          $(SRC_DIR)/cache_manager.cpp \
          $(SRC_DIR)/filter_manager.cpp \
          $(SRC_DIR)/socket_manager.cpp

# 对象文件
OBJECTS = $(BUILD_DIR)/main.o \
          $(BUILD_DIR)/http_parser.o \
          $(BUILD_DIR)/cache_manager.o \
          $(BUILD_DIR)/filter_manager.o \
          $(BUILD_DIR)/socket_manager.o

# 默认目标
all: directories $(TARGET)

# 创建必要的目录
directories:
	@if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
	@if not exist $(BIN_DIR) mkdir $(BIN_DIR)

# 链接生成可执行文件
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo Build complete: $(TARGET)

# 编译源文件
$(BUILD_DIR)/main.o: $(SRC_DIR)/main.cpp
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/main.cpp -o $(BUILD_DIR)/main.o

$(BUILD_DIR)/http_parser.o: $(SRC_DIR)/http_parser.cpp $(INCLUDE_DIR)/http_parser.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/http_parser.cpp -o $(BUILD_DIR)/http_parser.o

$(BUILD_DIR)/cache_manager.o: $(SRC_DIR)/cache_manager.cpp $(INCLUDE_DIR)/cache_manager.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/cache_manager.cpp -o $(BUILD_DIR)/cache_manager.o

$(BUILD_DIR)/filter_manager.o: $(SRC_DIR)/filter_manager.cpp $(INCLUDE_DIR)/filter_manager.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/filter_manager.cpp -o $(BUILD_DIR)/filter_manager.o

$(BUILD_DIR)/socket_manager.o: $(SRC_DIR)/socket_manager.cpp $(INCLUDE_DIR)/socket_manager.h
	$(CXX) $(CXXFLAGS) -c $(SRC_DIR)/socket_manager.cpp -o $(BUILD_DIR)/socket_manager.o

# 清理编译文件
clean:
	@if exist $(BUILD_DIR) rmdir /s /q $(BUILD_DIR)
	@if exist $(BIN_DIR) rmdir /s /q $(BIN_DIR)
	@echo Cleaned build artifacts

# 运行程序
run: $(TARGET)
	@cd $(BIN_DIR) && ProxyServer.exe

# 重新编译
rebuild: clean all

# 帮助信息
help:
	@echo Available targets:
	@echo   all      - Build the project (default)
	@echo   clean    - Remove build artifacts
	@echo   run      - Build and run the proxy server
	@echo   rebuild  - Clean and rebuild the project
	@echo   help     - Show this help message

.PHONY: all directories clean run rebuild help
