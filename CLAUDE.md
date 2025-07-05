# Bash命令
- ./build_libcxx.sh: 构建项目

# 示例代码
- example/ 目录下为示例diamagnetic

# 代码风格
- google 编码规范
- 函数返回值不要返回大对象，尽量返回值简单
- 使用std::expected 作为函数返回值
- 使用std::error_code 标识错误
- 使用标准库 memory_resource 进行内存管理
- 不要使用异常
- 使用c++23等高版本特性
-

# 工作流程
- 在完成一系列代码更改后务必进行运行 ./build_libcxx.sh 进行编译检查
- 为了代码改动没有问题，必须运行整个单元测试


# 单元测试
- tests/ 目录下为googletest 单元测试模块
- 对所有的接口进行单元测试、压力测试、性能测试