use rand::Rng;
use serde::{Deserialize, Serialize};
use std::fs::File;
use std::io::Write;
use std::time::Instant;
use tnum::tnum::{tnum_mul, tnum_mul_opt, tnum_mul_rec, xtnum_mul, Tnum};

/// Tnum结构
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
struct TestTnum {
    value: u64,
    mask: u64,
}

/// 包含原始输入、转换后的输入和结果
#[derive(Debug, Serialize, Deserialize)]
struct TestCase {
    raw_input_a: u64,
    raw_input_b: u64,
    input_a: TestTnum,
    input_b: TestTnum,
    results: Vec<MethodResult>,
}

/// 测试方法结果结构
#[derive(Debug, Serialize, Deserialize)]
struct MethodResult {
    method: String,
    output: TestTnum,
    avg_time_ns: f64,
    correct: bool,
}

fn run_method_test(
    method_name: &str,
    mul_fn: impl Fn(Tnum, Tnum) -> Tnum,
    a: Tnum,
    b: Tnum,
    iterations: usize,
    base_output: Option<&TestTnum>,
) -> MethodResult {
    let mut times = Vec::with_capacity(iterations);
    let mut result = None;

    for _ in 0..iterations {
        let start = Instant::now();
        result = Some(mul_fn(a, b));
        times.push(start.elapsed().as_nanos());
    }

    let result = result.unwrap();
    let output = TestTnum {
        value: result.value(),
        mask: result.mask(),
    };

    let correct = base_output.map_or(true, |base| {
        output.value == base.value && output.mask == base.mask
    });

    MethodResult {
        method: method_name.to_string(),
        output,
        avg_time_ns: times.iter().sum::<u128>() as f64 / iterations as f64,
        correct,
    }
}

fn main() {
    // 解析N
    let n: usize = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "1000".to_string())
        .parse()
        .unwrap_or(100);

    // 解析Iterations
    let iterations: usize = std::env::args()
        .nth(2)
        .unwrap_or_else(|| "1000".to_string())
        .parse()
        .unwrap_or(1000);

    println!("开始测试 {} 个用例，每个用例重复 {} 次...", n, iterations);
    let mut test_cases = Vec::with_capacity(n);
    
    // 用于统计的变量
    let methods = ["基础乘法", "优化乘法", "扩展乘法", "递归乘法"];
    let mut total_times = vec![0.0; methods.len()];
    let mut correct_counts = vec![0; methods.len()];

    for i in 0..n {
        // 随机生成两个64位整数
        let mut rng = rand::rng();
        let raw_a: u64 = rng.random();
        let raw_b: u64 = rng.random();

        // 生成Tnum对象
        let a = Tnum::new(raw_a, (raw_a & raw_b) ^ raw_b);
        let b = Tnum::new(raw_b, (raw_a & raw_b) ^ raw_a);

        let mut case_results = Vec::new();

        // 首先运行基础乘法(对结果进行对拍)
        let base_result = run_method_test("基础乘法", tnum_mul, a, b, iterations, None);
        let base_output = base_result.output;
        case_results.push(base_result);

        // 测试其他实现
        let implementations = vec![
            ("优化乘法", tnum_mul_opt as fn(Tnum, Tnum) -> Tnum),
            ("扩展乘法", |x, y| {
                xtnum_mul(
                    x,
                    x.mask().count_ones() as u64,
                    y,
                    y.mask().count_ones() as u64,
                    x.mask().count_ones() as u64 + y.mask().count_ones() as u64,
                )
            }),
            ("递归乘法", tnum_mul_rec),
        ];

        for (name, func) in implementations {
            case_results.push(run_method_test(name, func, a, b, iterations, Some(&base_output)));
        }

        // 更新统计信息
        for (j, result) in case_results.iter().enumerate() {
            total_times[j] += result.avg_time_ns;
            if result.correct {
                correct_counts[j] += 1;
            }
        }

        // 打印当前测试用例结果
        println!("\n测试用例 {}/{}", i + 1, n);
        for result in &case_results {
            println!(
                "  {}: {:.2} ns {}",
                result.method,
                result.avg_time_ns,
                if result.correct { "✓" } else { "✗" }
            );
        }

        test_cases.push(TestCase {
            raw_input_a: raw_a,  
            raw_input_b: raw_b,  
            input_a: TestTnum {
                value: a.value(),
                mask: a.mask(),
            },
            input_b: TestTnum {
                value: b.value(),
                mask: b.mask(),
            },
            results: case_results,
        });
    }

    // 打印总体统计信息
    println!("\n总体统计:");
    println!("方法\t\t平均时间(ns)\t正确率");
    println!("----------------------------------------");
    for i in 0..methods.len() {
        let avg_time = total_times[i] / n as f64;
        let accuracy = (correct_counts[i] as f64 / n as f64) * 100.0;
        println!(
            "{}\t{:.2}\t\t{:.1}%",
            methods[i], avg_time, accuracy
        );
    }

    // 保存结果到 JSON 文件
    let json = serde_json::to_string_pretty(&test_cases).unwrap();
    let output_file = "mul_test_results.json";
    let mut file = File::create(output_file).unwrap();
    file.write_all(json.as_bytes()).unwrap();

    println!("\n详细结果已保存到：{}", output_file);
}