use serde::{Serialize, Deserialize};
use rand::Rng;
use std::time::{Duration, Instant};
use std::fs::File;
use std::io::Write;
use crate::tnum::{Tnum, tnum_mul, tnum_mul_opt, xtnum_mul, tnum_mul_rec};

/// 可序列化的 Tnum 结构体
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
struct SerializableTnum {
    value: u64,
    mask: u64,
}

impl From<Tnum> for SerializableTnum {
    fn from(t: Tnum) -> Self {
        SerializableTnum {
            value: t.value(),
            mask: t.mask(),
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
struct TestCase {
    input_a: SerializableTnum,
    input_b: SerializableTnum,
    results: Vec<MulResult>,
}

#[derive(Debug, Serialize, Deserialize)]
struct MulResult {
    method: String,
    output: SerializableTnum,
    duration_ns: u128,
}

fn generate_test_case() -> (Tnum, Tnum) {
    let mut rng = rand::thread_rng();
    let a: u64 = rng.gen();
    let b: u64 = rng.gen();
    
    (
        Tnum::new(a, (a & b) ^ b),
        Tnum::new(b, (a & b) ^ a)
    )
}

fn run_test(n: usize) -> Vec<TestCase> {
    let mut test_cases = Vec::with_capacity(n);
    
    for _ in 0..n {
        let (a, b) = generate_test_case();
        let mut results = Vec::new();
        
        // 测试基础版本
        let start = Instant::now();
        let result = tnum_mul(a, b);
        results.push(MulResult {
            method: "basic".to_string(),
            output: SerializableTnum::from(result),
            duration_ns: start.elapsed().as_nanos(),
        });
        
        // 测试优化版本
        let start = Instant::now();
        let result = tnum_mul_opt(a, b);
        results.push(MulResult {
            method: "optimized".to_string(),
            output: SerializableTnum::from(result),
            duration_ns: start.elapsed().as_nanos(),
        });
        
        // 测试扩展版本
        let start = Instant::now();
        let result = xtnum_mul(
            a, 
            a.mask().count_ones() as u64, 
            b, 
            b.mask().count_ones() as u64,
            a.mask().count_ones() as u64 + b.mask().count_ones() as u64
        );
        results.push(MulResult {
            method: "extended".to_string(),
            output: SerializableTnum::from(result),
            duration_ns: start.elapsed().as_nanos(),
        });
        
        // 测试递归版本
        let start = Instant::now();
        let result = tnum_mul_rec(a, b);
        results.push(MulResult {
            method: "recursive".to_string(),
            output: SerializableTnum::from(result),
            duration_ns: start.elapsed().as_nanos(),
        });
        
        test_cases.push(TestCase {
            input_a: SerializableTnum::from(a),
            input_b: SerializableTnum::from(b),
            results,
        });
    }
    
    test_cases
}

fn main() {
    let n: usize = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "100".to_string())
        .parse()
        .unwrap_or(100);
        
    println!("开始生成 {} 个测试用例...", n);
    let test_cases = run_test(n);
    
    println!("序列化测试结果...");
    let json = serde_json::to_string_pretty(&test_cases).unwrap();
    
    let output_file = "mul_test_results.json";
    let mut file = File::create(output_file).unwrap();
    file.write_all(json.as_bytes()).unwrap();
    
    println!("测试完成！结果已保存到：{}", output_file);
}