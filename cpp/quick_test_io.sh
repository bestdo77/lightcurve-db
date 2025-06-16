#!/bin/bash

echo "🚀 快速IO性能测试"
echo "=================="

# 检查是否有测试数据
if [ ! -f "../data/test_data_100M.csv" ]; then
    echo "❌ 需要测试数据文件: ../data/test_data_100M.csv"
    exit 1
fi

echo "📁 发现测试数据文件"
echo "📊 准备进行小规模IO性能测试..."
echo ""

# 创建小规模测试数据（取前100万行）
TEST_SMALL="../data/test_data_1M.csv"
if [ ! -f "$TEST_SMALL" ]; then
    echo "🔧 创建100万行测试数据..."
    head -n 1000001 "../data/test_data_100M.csv" > "$TEST_SMALL"
    echo "✅ 创建完成: $TEST_SMALL"
fi

echo ""
echo "🔥 开始IO优化版本测试（100万条数据）..."
echo "======================================"

# 记录开始时间
start_time=$(date +%s)

# 运行优化版本导入测试
./quick_import_optimized \
    --input "$TEST_SMALL" \
    --db "test_io_small" \
    --threads 4 \
    --batch_size 1000 \
    --drop_db

exit_code=$?
end_time=$(date +%s)
duration=$((end_time - start_time))

echo ""
echo "📈 测试结果:"
echo "==========="

if [ $exit_code -eq 0 ]; then
    echo "✅ IO优化测试成功完成"
    echo "⏱️  总耗时: ${duration} 秒"
    echo "🚀 处理速度: 约 $((1000000 / duration)) 条/秒"
    
    # 估算一亿数据耗时
    estimated_100m_seconds=$((duration * 100))
    estimated_minutes=$((estimated_100m_seconds / 60))
    
    echo ""
    echo "📊 性能估算:"
    echo "   - 100万条数据: ${duration} 秒"
    echo "   - 1亿条数据预计: ${estimated_minutes} 分钟"
    
else
    echo "❌ 测试失败，退出码: $exit_code"
fi

echo ""
echo "💡 IO优化要点:"
echo "============="
echo "1. 🚀 一次性文件加载 - 避免逐行IO"
echo "2. 🧵 多线程解析 - 充分利用CPU"
echo "3. 💾 内存预分配 - 减少动态分配开销"
echo "4. ⚡ 零拷贝字符串 - string_view优化"
echo "5. 🎯 批量数据库插入 - 减少网络往返"

echo ""
echo "🎉 快速IO测试完成！" 