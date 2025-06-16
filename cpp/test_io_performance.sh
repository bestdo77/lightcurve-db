#!/bin/bash

echo "🚀 TDengine 导入器 IO性能对比测试"
echo "=================================="

# 检查是否有测试数据
if [ ! -f "../data/test_data_100M.csv" ]; then
    echo "❌ 需要测试数据文件: ../data/test_data_100M.csv"
    echo "请先生成测试数据"
    exit 1
fi

# 编译优化版本
echo "🔧 编译IO优化版本..."
g++ -std=c++17 -O3 -march=native -DNDEBUG -o quick_import_optimized quick_import.cpp -ltaos -lhealpix_cxx -lgsl -lgslcblas -lcfitsio -ltbb

if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi

echo "✅ 编译成功"
echo ""

# 测试参数
TEST_DATA="../data/test_data_100M.csv"
DB_NAME="test_io_performance"

echo "📊 测试配置:"
echo "   - 数据文件: $TEST_DATA"
echo "   - 数据库: $DB_NAME"
echo "   - 测试模式: IO优化版本"
echo ""

# 运行IO优化版本测试
echo "🔥 开始IO优化版本测试..."
echo "========================"

# 记录开始时间
start_time=$(date +%s)

# 运行导入测试
timeout 1800 ./quick_import_optimized \
    --input "$TEST_DATA" \
    --db "$DB_NAME" \
    --threads 8 \
    --batch_size 1000 \
    --drop_db \
    > io_optimized_test.log 2>&1

exit_code=$?
end_time=$(date +%s)
duration=$((end_time - start_time))

echo ""
echo "📈 测试结果分析:"
echo "==============="

if [ $exit_code -eq 0 ]; then
    echo "✅ IO优化测试成功完成"
    echo "⏱️  总耗时: ${duration} 秒"
    
    # 提取关键指标
    if [ -f "io_optimized_test.log" ]; then
        echo ""
        echo "🔍 关键性能指标:"
        
        # 文件加载时间
        file_load_info=$(grep "文件已加载到内存" io_optimized_test.log)
        if [ -n "$file_load_info" ]; then
            echo "   📁 $file_load_info"
        fi
        
        # 解析性能
        parse_info=$(grep "高性能IO完成" io_optimized_test.log)
        if [ -n "$parse_info" ]; then
            echo "   🔄 $parse_info"
        fi
        
        # 导入性能
        import_info=$(grep "成功导入" io_optimized_test.log | tail -1)
        if [ -n "$import_info" ]; then
            echo "   ✅ $import_info"
        fi
        
        # 速度统计
        speed_info=$(grep "平均速度" io_optimized_test.log)
        if [ -n "$speed_info" ]; then
            echo "   ⚡ $speed_info"
        fi
        
        # 线程优化
        thread_info=$(grep "使用优化线程数" io_optimized_test.log)
        if [ -n "$thread_info" ]; then
            echo "   🧵 $thread_info"
        fi
    fi
    
elif [ $exit_code -eq 124 ]; then
    echo "⚠️  测试超时（30分钟）"
    echo "⏱️  运行时间: ${duration} 秒"
else
    echo "❌ 测试失败，退出码: $exit_code"
    echo "⏱️  运行时间: ${duration} 秒"
fi

echo ""
echo "📄 详细日志文件:"
echo "   - IO优化版本: io_optimized_test.log"

# 性能总结
echo ""
echo "🎯 IO优化重点:"
echo "============="
echo "1. 🚀 一次性文件加载：整个文件读入内存，避免逐行IO"
echo "2. 🔄 并行解析：多线程并行处理CSV解析"
echo "3. 💾 预分配内存：避免vector动态增长"
echo "4. ⚡ 快速字符串分割：使用string_view避免拷贝"
echo "5. 🎯 批量插入优化：增大批次大小，减少网络往返"
echo "6. 🧵 动态线程调整：根据数据量优化线程数"

echo ""
echo "💡 预期性能提升:"
echo "   - 文件解析速度: 3-5倍提升"
echo "   - 内存使用效率: 显著改善"
echo "   - 数据库插入: 2-3倍提升"
echo "   - 总体导入时间: 2-4倍提升"

echo ""
echo "🎉 IO性能测试完成！" 