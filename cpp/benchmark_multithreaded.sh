#!/bin/bash

# 多线程性能基准测试脚本

echo "🚀 TDengine HealPix 多线程性能基准测试"
echo "======================================================="

# 检查参数
if [ $# -lt 2 ]; then
    echo "用法: $0 <输入CSV文件> <数据库前缀>"
    echo "示例: $0 data.csv benchmark_test"
    exit 1
fi

INPUT_FILE="$1"
DB_PREFIX="$2"
EXECUTABLE="./quick_import_mt"

# 检查文件和可执行程序是否存在
if [ ! -f "$INPUT_FILE" ]; then
    echo "❌ 输入文件不存在: $INPUT_FILE"
    exit 1
fi

if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ 可执行文件不存在: $EXECUTABLE"
    echo "请先运行 compile_multithreaded.sh 编译程序"
    exit 1
fi

# 测试配置
THREAD_COUNTS=(1 2 4 8 16)
BATCH_SIZE=1000
NSIDE_BASE=64
NSIDE_FINE=256

# 结果记录
RESULTS_FILE="benchmark_results_$(date +%Y%m%d_%H%M%S).txt"

echo "📋 测试配置:"
echo "  - 输入文件: $INPUT_FILE"
echo "  - 数据库前缀: $DB_PREFIX"
echo "  - 批处理大小: $BATCH_SIZE"
echo "  - 基础NSIDE: $NSIDE_BASE"
echo "  - 细分NSIDE: $NSIDE_FINE"
echo "  - 线程数测试: ${THREAD_COUNTS[*]}"
echo ""

# 初始化结果文件
echo "TDengine HealPix 多线程性能基准测试报告" > "$RESULTS_FILE"
echo "=======================================================" >> "$RESULTS_FILE"
echo "测试时间: $(date)" >> "$RESULTS_FILE"
echo "输入文件: $INPUT_FILE" >> "$RESULTS_FILE"
echo "文件大小: $(du -h "$INPUT_FILE" | cut -f1)" >> "$RESULTS_FILE"
echo "" >> "$RESULTS_FILE"
echo "线程数 | 耗时(秒) | 速度(行/秒) | 数据库名" >> "$RESULTS_FILE"
echo "-------|----------|------------|----------" >> "$RESULTS_FILE"

# 循环测试不同线程数
total_tests=${#THREAD_COUNTS[@]}
current_test=0

for threads in "${THREAD_COUNTS[@]}"; do
    current_test=$((current_test + 1))
    echo ""
    echo "=================================================================================="
    echo "🧵 测试 $current_test/$total_tests: $threads 个线程"
    echo "📊 预计剩余测试: $((total_tests - current_test)) 个"
    echo "=================================================================================="
    
    DB_NAME="${DB_PREFIX}_${threads}t"
    
    # 记录开始时间
    start_time=$(date +%s)
    
    # 运行导入程序（使用 tee 同时显示进度条和保存日志）
    echo "🎬 开始 $threads 线程测试，可以看到实时进度条..."
    if $EXECUTABLE \
        --input "$INPUT_FILE" \
        --db "$DB_NAME" \
        --threads "$threads" \
        --batch_size "$BATCH_SIZE" \
        --nside_base "$NSIDE_BASE" \
        --nside_fine "$NSIDE_FINE" \
        --drop_db 2>&1 | tee "test_output_${threads}t.log"; then
        
        # 记录结束时间
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        
        # 提取成功导入的行数
        success_count=$(grep "成功导入:" "test_output_${threads}t.log" | grep -o '[0-9]\+' | head -1)
        
        if [ -n "$success_count" ] && [ "$success_count" -gt 0 ]; then
            speed=$((success_count / duration))
            echo ""
            echo "🎊 测试完成！"
            echo "✅ $threads 线程: ${duration}秒, ${speed}行/秒, 成功导入 $success_count 行"
            printf "%-6s | %-8s | %-10s | %s\n" "$threads" "$duration" "$speed" "$DB_NAME" >> "$RESULTS_FILE"
        else
            echo ""
            echo "❌ $threads 线程: 导入失败"
            printf "%-6s | %-8s | %-10s | %s\n" "$threads" "失败" "-" "$DB_NAME" >> "$RESULTS_FILE"
        fi
    else
        echo ""
        echo "❌ $threads 线程: 程序执行失败"
        printf "%-6s | %-8s | %-10s | %s\n" "$threads" "失败" "-" "$DB_NAME" >> "$RESULTS_FILE"
    fi
    
    # 显示当前进度
    echo "📈 当前测试进度: $current_test/$total_tests 完成"
    if [ $current_test -lt $total_tests ]; then
        echo "⏭️ 准备下一个测试..."
        sleep 2
    fi
done

echo "" >> "$RESULTS_FILE"
echo "详细日志文件: test_output_*t.log" >> "$RESULTS_FILE"

echo "🎊 基准测试完成！"
echo "📄 结果已保存到: $RESULTS_FILE"
echo ""
echo "📊 性能汇总:"
cat "$RESULTS_FILE" | tail -n +8

echo ""
echo "💡 分析建议:"
echo "  - 对比不同线程数的性能表现"
echo "  - 通常在 CPU 核数的 2-4 倍时达到最佳性能"
echo "  - 过多线程可能导致资源竞争，性能下降"
echo "  - 可以查看详细日志文件了解更多信息" 