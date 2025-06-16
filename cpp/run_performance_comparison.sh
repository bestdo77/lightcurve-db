#!/bin/bash

echo "🚀 TDengine HealPix 同步 vs 异步性能对比测试"
echo "=============================================="

# 检查数据文件是否存在
if [ ! -f "../data/test_data_100M.csv" ]; then
    echo "❌ 数据文件不存在: ../data/test_data_100M.csv"
    echo "请先生成一亿条测试数据"
    exit 1
fi

echo "📁 发现测试数据文件，开始性能对比..."
echo ""

# 1. 运行同步版本测试
echo "🔄 第一轮：运行同步版本测试 (原始性能)"
echo "======================================="
echo "⏱️  开始时间: $(date)"
echo ""

timeout 600 ./query_test1 > sync_test_results.log 2>&1
sync_exit_code=$?

if [ $sync_exit_code -eq 124 ]; then
    echo "⚠️  同步测试超时（10分钟），这表明性能较慢"
elif [ $sync_exit_code -eq 0 ]; then
    echo "✅ 同步测试完成"
else
    echo "❌ 同步测试出错，退出码: $sync_exit_code"
fi

echo "⏱️  结束时间: $(date)"
echo ""

# 2. 等待一段时间让系统休息
echo "💤 等待30秒让系统休息..."
sleep 30

# 3. 运行异步版本测试
echo "🔄 第二轮：运行异步版本测试 (优化性能)"
echo "======================================="
echo "⏱️  开始时间: $(date)"
echo ""

timeout 600 ./build/query_test > async_test_results.log 2>&1
async_exit_code=$?

if [ $async_exit_code -eq 124 ]; then
    echo "⚠️  异步测试超时（10分钟）"
elif [ $async_exit_code -eq 0 ]; then
    echo "✅ 异步测试完成"
else
    echo "❌ 异步测试出错，退出码: $async_exit_code"
fi

echo "⏱️  结束时间: $(date)"
echo ""

# 4. 分析结果
echo "📊 性能对比分析"
echo "==============="

echo ""
echo "🔍 同步版本关键指标:"
if [ -f "sync_test_results.log" ]; then
    grep -E "(最近邻.*总耗时|成功查询|锥形检索.*总耗时)" sync_test_results.log | head -5
else
    echo "❌ 同步测试结果文件不存在"
fi

echo ""
echo "🔍 异步版本关键指标:"
if [ -f "async_test_results.log" ]; then
    grep -E "(最近邻查询完成|锥形检索.*完成|耗时)" async_test_results.log | head -5
else
    echo "❌ 异步测试结果文件不存在"
fi

echo ""
echo "📄 详细结果文件:"
echo "   - 同步版本: sync_test_results.log"
echo "   - 异步版本: async_test_results.log"
echo "   - 对比报告: performance_comparison.md"

echo ""
echo "🎉 性能对比测试完成！"
echo "💡 查看 performance_comparison.md 获取详细分析" 