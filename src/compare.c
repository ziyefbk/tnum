#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <json-c/json.h>

// 定义方法名称
const char *METHOD_NAMES[] = {
    "C_tnum_mul",
    "tnum_mul",
    "tnum_mul_opt",
    "xtnum_mul_top",
    "xtnum_mul_high_top"};
#define NUM_METHODS 5

// 统计信息结构体
typedef struct
{
    const char *method;
    int correct_count;
    int total_count;
    double total_time;
    double avg_time;
} MethodStats;

// 不一致结果结构体
typedef struct
{
    int case_number;
    uint64_t input_a_value;
    uint64_t input_a_mask;
    uint64_t input_b_value;
    uint64_t input_b_mask;
    uint64_t c_output_value; // C结果作为基准
    uint64_t c_output_mask;
    uint64_t rust_output_value; // 不一致的Rust结果
    uint64_t rust_output_mask;
    const char *method_name; // 哪个方法不一致
} Inconsistency;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <c_test_results.json>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *input_file = argv[1];

    // 打开JSON文件
    FILE *fp = fopen(input_file, "r");
    if (!fp)
    {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    // 读取文件内容
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *json_data = (char *)malloc(file_size + 1);
    if (!json_data)
    {
        perror("Memory allocation failed");
        fclose(fp);
        return EXIT_FAILURE;
    }

    fread(json_data, 1, file_size, fp);
    json_data[file_size] = '\0';
    fclose(fp);

    // 解析JSON
    struct json_object *root = json_tokener_parse(json_data);
    if (!root)
    {
        fprintf(stderr, "Error parsing JSON data\n");
        free(json_data);
        return EXIT_FAILURE;
    }

    // 初始化统计数据
    MethodStats stats[NUM_METHODS];
    for (int i = 0; i < NUM_METHODS; i++)
    {
        stats[i].method = METHOD_NAMES[i];
        stats[i].correct_count = 0;
        stats[i].total_count = 0;
        stats[i].total_time = 0.0;
        stats[i].avg_time = 0.0;
    }

    // 不一致结果存储
    Inconsistency *inconsistencies = NULL;
    int inconsistency_count = 0;
    int max_inconsistencies = 100;

    inconsistencies = (Inconsistency *)malloc(sizeof(Inconsistency) * max_inconsistencies);
    if (!inconsistencies)
    {
        perror("Memory allocation for inconsistencies failed");
        json_object_put(root);
        free(json_data);
        return EXIT_FAILURE;
    }

    // 遍历所有测试用例
    int array_length = json_object_array_length(root);
    printf("分析 %d 个测试用例...\n", array_length);

    for (int i = 0; i < array_length; i++)
    {
        struct json_object *test_case = json_object_array_get_idx(root, i);

        // 获取输入值
        struct json_object *input_a, *input_b;
        json_object_object_get_ex(test_case, "input_a", &input_a);
        json_object_object_get_ex(test_case, "input_b", &input_b);

        struct json_object *input_a_value, *input_a_mask, *input_b_value, *input_b_mask;
        json_object_object_get_ex(input_a, "value", &input_a_value);
        json_object_object_get_ex(input_a, "mask", &input_a_mask);
        json_object_object_get_ex(input_b, "value", &input_b_value);
        json_object_object_get_ex(input_b, "mask", &input_b_mask);

        uint64_t a_value = json_object_get_uint64(input_a_value);
        uint64_t a_mask = json_object_get_uint64(input_a_mask);
        uint64_t b_value = json_object_get_uint64(input_b_value);
        uint64_t b_mask = json_object_get_uint64(input_b_mask);

        // 获取结果数组
        struct json_object *results;
        json_object_object_get_ex(test_case, "results", &results);
        int results_count = json_object_array_length(results);

        // 先找C_tnum_mul的结果作为基准
        struct json_object *c_result = NULL;
        uint64_t c_output_value = 0, c_output_mask = 0;
        double c_time = 0.0;

        for (int j = 0; j < 5; j++)
        {
            struct json_object *result = json_object_array_get_idx(results, j);
            struct json_object *method_obj;
            json_object_object_get_ex(result, "method", &method_obj);
            const char *method = json_object_get_string(method_obj);
            // printf("分析方法: %s\n", method);
            if (strcmp(method, "C_tnum_mul") == 0)
            {
                c_result = result;

                struct json_object *output_obj, *time_obj;
                json_object_object_get_ex(result, "output", &output_obj);
                json_object_object_get_ex(result, "avg_time_ns", &time_obj);

                struct json_object *value_obj, *mask_obj;
                json_object_object_get_ex(output_obj, "value", &value_obj);
                json_object_object_get_ex(output_obj, "mask", &mask_obj);

                c_output_value = json_object_get_uint64(value_obj);
                c_output_mask = json_object_get_uint64(mask_obj);
                c_time = json_object_get_double(time_obj);

                // 更新C_tnum_mul的统计信息
                for (int k = 0; k < NUM_METHODS; k++)
                {
                    if (strcmp(METHOD_NAMES[k], "C_tnum_mul") == 0)
                    {
                        stats[k].total_count++;
                        stats[k].correct_count++; 
                        stats[k].total_time += c_time;
                        break;
                    }
                }

                break;
            }
        }

        // 遍历所有其他方法的结果，与C_tnum_mul比较
        for (int j = 0; j < results_count; j++)
        {
            struct json_object *result = json_object_array_get_idx(results, j);

            struct json_object *method_obj, *output_obj, *time_obj;
            json_object_object_get_ex(result, "method", &method_obj);
            json_object_object_get_ex(result, "output", &output_obj);
            json_object_object_get_ex(result, "avg_time_ns", &time_obj);

            const char *method = json_object_get_string(method_obj);
            double time = json_object_get_double(time_obj);

            // 跳过C_tnum_mul自己
            if (strcmp(method, "C_tnum_mul") == 0)
            {
                continue;
            }

            // 获取输出信息
            struct json_object *value_obj, *mask_obj;
            json_object_object_get_ex(output_obj, "value", &value_obj);
            json_object_object_get_ex(output_obj, "mask", &mask_obj);

            uint64_t output_value = json_object_get_uint64(value_obj);
            uint64_t output_mask = json_object_get_uint64(mask_obj);

            // 比较结果
            bool correct = (output_value == c_output_value && output_mask == c_output_mask);

            // 更新统计信息
            for (int k = 0; k < NUM_METHODS; k++)
            {
                // printf("比较方法: %s %s\n", METHOD_NAMES[k], method);
                if (strcmp(method, METHOD_NAMES[k]) == 0)
                {
                    stats[k].total_count++;
                    stats[k].total_time += time;

                    if (correct)
                    {
                        stats[k].correct_count++;
                    }
                    else
                    {
                        // 记录不一致的结果
                        if (inconsistency_count < max_inconsistencies)
                        {
                            inconsistencies[inconsistency_count].case_number = i + 1;
                            inconsistencies[inconsistency_count].input_a_value = a_value;
                            inconsistencies[inconsistency_count].input_a_mask = a_mask;
                            inconsistencies[inconsistency_count].input_b_value = b_value;
                            inconsistencies[inconsistency_count].input_b_mask = b_mask;
                            inconsistencies[inconsistency_count].c_output_value = c_output_value;
                            inconsistencies[inconsistency_count].c_output_mask = c_output_mask;
                            inconsistencies[inconsistency_count].rust_output_value = output_value;
                            inconsistencies[inconsistency_count].rust_output_mask = output_mask;
                            inconsistencies[inconsistency_count].method_name = method;
                            inconsistency_count++;
                        }
                        else if (inconsistency_count == max_inconsistencies)
                        {
                            // 扩展不一致结果的存储空间
                            max_inconsistencies *= 2;
                            Inconsistency *new_array = (Inconsistency *)realloc(
                                inconsistencies, sizeof(Inconsistency) * max_inconsistencies);
                            if (!new_array)
                            {
                                perror("内存重分配失败");
                                break;
                            }
                            inconsistencies = new_array;
                        }
                    }

                    break;
                }
            }
        }

        // 每处理10个用例显示一次进度
        // if ((i + 1) % 10 == 0 || i == array_length - 1)
        // {
        //     printf("\r处理进度: %d/%d (%.1f%%)", i + 1, array_length, (i + 1) * 100.0 / array_length);
        //     fflush(stdout);
        // }
    }

    // printf("\n\n");

    // 计算平均时间
    for (int i = 0; i < NUM_METHODS; i++)
    {
        if (stats[i].total_count > 0)
        {
            stats[i].avg_time = stats[i].total_time / stats[i].total_count;
        }
    }


    // 打印统计结果
    printf("%-24s%21s%s\n", "method", "average time(ns)", "accuracy");
    printf("------------------------------------------------------------------------\n");

    for (int i = 0; i < NUM_METHODS; i++)
    {
        if (stats[i].total_count > 0)
        {
            double accuracy = (double)stats[i].correct_count / stats[i].total_count * 100.0;

            printf("%-24s %-15.2f %.1f%% ",
                   stats[i].method,
                   stats[i].avg_time,
                   accuracy);

            printf("\n");
        }
    }

    if (inconsistency_count > 0)
    {
        // 创建JSON结构
        struct json_object *inconsistency_root = json_object_new_array();

        for (int i = 0; i < inconsistency_count; i++)
        {
            struct json_object *item = json_object_new_object();

            // 添加基本信息
            json_object_object_add(item, "case_number",
                                   json_object_new_int(inconsistencies[i].case_number));
            json_object_object_add(item, "method",
                                   json_object_new_string(inconsistencies[i].method_name));

            // 添加输入值
            struct json_object *input_a = json_object_new_object();
            json_object_object_add(input_a, "value",
                                   json_object_new_uint64(inconsistencies[i].input_a_value));
            json_object_object_add(input_a, "mask",
                                   json_object_new_uint64(inconsistencies[i].input_a_mask));
            json_object_object_add(item, "input_a", input_a);

            struct json_object *input_b = json_object_new_object();
            json_object_object_add(input_b, "value",
                                   json_object_new_uint64(inconsistencies[i].input_b_value));
            json_object_object_add(input_b, "mask",
                                   json_object_new_uint64(inconsistencies[i].input_b_mask));
            json_object_object_add(item, "input_b", input_b);

            // 添加C实现结果
            struct json_object *c_output = json_object_new_object();
            json_object_object_add(c_output, "value",
                                   json_object_new_uint64(inconsistencies[i].c_output_value));
            json_object_object_add(c_output, "mask",
                                   json_object_new_uint64(inconsistencies[i].c_output_mask));
            json_object_object_add(item, "c_output", c_output);

            // 添加Rust实现结果
            struct json_object *rust_output = json_object_new_object();
            json_object_object_add(rust_output, "value",
                                   json_object_new_uint64(inconsistencies[i].rust_output_value));
            json_object_object_add(rust_output, "mask",
                                   json_object_new_uint64(inconsistencies[i].rust_output_mask));
            json_object_object_add(item, "rust_output", rust_output);

            json_object_array_add(inconsistency_root, item);
        }

        char filename[] = "inconsistencies.json";
        // 输出到文件
        const char *json_string = json_object_to_json_string_ext(
            inconsistency_root, JSON_C_TO_STRING_PRETTY);

        FILE *out_file = fopen(filename, "w");
        if (out_file)
        {
            fprintf(out_file, "%s", json_string);
            fclose(out_file);
            printf("\n不一致结果已保存到: %s\n", filename);
        }
        else
        {
            perror("无法创建输出文件");
        }

        // 释放JSON资源
        json_object_put(inconsistency_root);

        // 释放资源
        free(inconsistencies);
        json_object_put(root);
        free(json_data);

        return EXIT_SUCCESS;
    }
}