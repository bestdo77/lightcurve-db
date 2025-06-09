#!/bin/bash

# TDengine Healpix 空间分析项目快速启动脚本

echo "🚀 TDengine Healpix 空间分析项目"
echo "================================"

# 检查 TDengine 服务
if ! pgrep -x "taosd" > /dev/null; then
    echo "⚠️  TDengine 服务未运行，正在启动..."
    sudo systemctl start taosd
    sleep 3
fi

echo "📁 确保输出目录存在"
mkdir -p output/logs output/performance_reports output/query_results
echo "   ✅ 输出目录已创建"
echo ""

echo "1. 生成测试数据（可选）"
echo "   python3 scripts/generate_astronomical_data.py --num_sources 10000 --records_per_source 100 --output data/test_data.csv"
echo "   💡 数据生成报告将保存到: output/logs/"
echo ""

echo "2. 导入数据到 TDengine"
echo "   python3 scripts/quick_import.py --input data/test_data.csv --db sensor_db_healpix"
echo "   💡 导入报告将保存到: output/logs/"
echo ""

echo "3. 查看数据库状态"
echo "   bash scripts/db_info.sh"
echo "   💡 数据库信息报告将保存到: output/logs/"
echo ""

echo "4. 运行空间查询性能测试"
echo "   python3 scripts/query_test.py"
echo "   💡 性能测试报告将保存到: output/performance_reports/"
echo ""

echo "5. 查看时间查询示例"
echo "   taos -f scripts/temporal_queries_examples.sql"
echo ""

echo "📊 所有输出结果保存在 output/ 目录中"
echo "📖 详细使用说明请参考 README.md"
