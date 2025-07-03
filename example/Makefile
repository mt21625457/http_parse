# HTTP Parse Library Examples Makefile
# 编译HTTP/1.x和HTTP/2协议示例程序

CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -O2 -g
INCLUDES = -I../include
LDFLAGS = -pthread

# 目标文件
TARGETS = http1_example http2_example comprehensive_example

# 默认目标
all: $(TARGETS)

# 编译HTTP/1.x示例
http1_example: http1_example.cpp
	@echo "🔨 编译HTTP/1.x示例..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)
	@echo "✅ HTTP/1.x示例编译完成"

# 编译HTTP/2示例  
http2_example: http2_example.cpp
	@echo "🔨 编译HTTP/2示例..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)
	@echo "✅ HTTP/2示例编译完成"

# 编译综合示例
comprehensive_example: comprehensive_example.cpp
	@echo "🔨 编译综合示例..."
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $< $(LDFLAGS)
	@echo "✅ 综合示例编译完成"

# 运行所有示例
run: all
	@echo "\n🚀 运行HTTP/1.x示例:"
	@echo "========================"
	./http1_example
	@echo "\n🚀 运行HTTP/2示例:"
	@echo "========================"
	./http2_example  
	@echo "\n🚀 运行综合示例:"
	@echo "========================"
	./comprehensive_example

# 运行单个示例
run-http1: http1_example
	@echo "🚀 运行HTTP/1.x示例:"
	./http1_example

run-http2: http2_example
	@echo "🚀 运行HTTP/2示例:"
	./http2_example

run-comprehensive: comprehensive_example
	@echo "🚀 运行综合示例:"
	./comprehensive_example

# 清理
clean:
	@echo "🧹 清理编译文件..."
	rm -f $(TARGETS)
	@echo "✅ 清理完成"

# 检查依赖
check:
	@echo "🔍 检查编译环境..."
	@$(CXX) --version
	@echo "✅ 编译器检查完成"
	@echo "📁 检查头文件..."
	@test -f ../include/http_parse.hpp && echo "✅ 主头文件存在" || echo "❌ 主头文件缺失"
	@test -d ../include/http_parse && echo "✅ 头文件目录存在" || echo "❌ 头文件目录缺失"

# 帮助信息
help:
	@echo "HTTP Parse Library Examples"
	@echo "=========================="
	@echo ""
	@echo "可用目标:"
	@echo "  all                 - 编译所有示例"
	@echo "  http1_example       - 编译HTTP/1.x示例"
	@echo "  http2_example       - 编译HTTP/2示例"
	@echo "  comprehensive_example - 编译综合示例"
	@echo ""
	@echo "运行目标:"
	@echo "  run                 - 运行所有示例"
	@echo "  run-http1           - 运行HTTP/1.x示例"
	@echo "  run-http2           - 运行HTTP/2示例"
	@echo "  run-comprehensive   - 运行综合示例"
	@echo ""
	@echo "其他目标:"
	@echo "  clean               - 清理编译文件"
	@echo "  check               - 检查编译环境"
	@echo "  help                - 显示此帮助信息"
	@echo ""
	@echo "编译要求:"
	@echo "  - C++23兼容编译器 (GCC 13+, Clang 16+)"
	@echo "  - pthread支持"

.PHONY: all run run-http1 run-http2 run-comprehensive clean check help