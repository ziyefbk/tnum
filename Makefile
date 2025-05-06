.PHONY: test clean

# 默认参数
N ?= 1000
ITERATIONS ?= 1000

test: build
	@echo "运行测试，生成 $(N) 个测试用例，每个重复 $(ITERATIONS) 次..."
	@./target/release/test_mul $(N) $(ITERATIONS)

build:
	cargo build --release

clean:
	cargo clean
	rm -f mul_test_results.json

help:
	@echo "使用方法:"
	@echo "  make test N=1000 ITERATIONS=1000  
	@echo "  make clean                       