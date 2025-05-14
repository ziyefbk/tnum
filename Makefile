CC = gcc
CFLAGS = -Wall -Wextra -I./include
LDFLAGS = -ljson-c

SRC_DIR = src
BUILD_DIR = build
RUST_JSON = ./build/rust_test_cases.json
C_JSON = ./build/c_test_results.json

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 编译tnum.c
$(BUILD_DIR)/tnum.o: $(SRC_DIR)/tnum.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 编译tnum_mul.c
$(BUILD_DIR)/tnum_mul: $(SRC_DIR)/tnum_mul.c $(BUILD_DIR)/tnum.o | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# 编译compare.c
$(BUILD_DIR)/compare: $(SRC_DIR)/compare.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# 生成测试用例（运行test.rs）
$(RUST_JSON):
	cargo run --release --bin test_mul -- 100 100

# 执行完整测试流程
test: build rust-test c-test compare-results

# 运行Rust测试
rust-test: $(RUST_JSON)
	@echo "✅ Rust测试完成，生成测试用例：$(RUST_JSON)"

# 运行C实现测试
c-test: $(BUILD_DIR)/tnum_mul rust-test
	@echo "🔍 运行C实现测试..."
	$(BUILD_DIR)/tnum_mul $(RUST_JSON)
	@echo "✅ C测试完成，生成结果：$(C_JSON)"

# 比较结果
compare-results: $(BUILD_DIR)/compare $(C_JSON)
	@echo "🔍 比较测试结果..."
	$(BUILD_DIR)/compare $(C_JSON)
	@echo "✅ 测试比较完成"

# 清理
clean:
	rm -rf $(BUILD_DIR) $(RUST_JSON) $(C_JSON)
	cargo clean

# 显示帮助
help:
	@echo "使用说明:"
	@echo "  make test       - 执行完整测试流程"
	@echo "  make rust-test  - 只运行Rust测试生成用例"
	@echo "  make c-test     - 运行C实现测试"
	@echo "  make clean      - 清理所有生成的文件"

.PHONY: test rust-test c-test compare-results clean help