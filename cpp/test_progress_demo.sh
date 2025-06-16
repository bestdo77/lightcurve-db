#!/bin/bash

# 进度条功能快速演示脚本

echo "🎭 进度条功能演示 - 使用小数据集"
echo "=================================="

cd healpix_spatial_analysis/cpp

# 检查文件
if [ ! -f "./quick_import_mt" ]; then
    echo "❌ 可执行文件不存在，正在编译..."
    ./compile_multithreaded.sh
fi

if [ ! -f "../../data/small_test.csv" ]; then
    echo "❌ 小测试文件不存在"
    exit 1
fi

echo ""
echo "🚀 演示1: 直接导入（可以看到完整进度条）"
echo "=============================================="

./quick_import_mt \
    --input "../../data/small_test.csv" \
    --db "progress_demo" \
    --threads 4 \
    --batch_size 10 \
    --drop_db

echo ""
echo "🚀 演示2: 基准测试（多线程对比，每个都有进度条）"
echo "=================================================="

# 修改基准测试脚本，只测试少数线程数
sed 's/THREAD_COUNTS=(1 2 4 8 16)/THREAD_COUNTS=(2 4)/' benchmark_multithreaded.sh > benchmark_demo.sh
chmod +x benchmark_demo.sh

./benchmark_demo.sh "../../data/small_test.csv" demo_benchmark

echo ""
echo "🎉 演示完成！"
echo ""
echo "💡 现在基准测试的改进："
echo "  ✅ 可以看到实时进度条"
echo "  ✅ 同时保存完整日志"
echo "  ✅ 显示测试进度 (x/total)"
echo "  ✅ 更好的视觉分隔"
echo ""
echo "🔧 对于大文件测试，运行："
echo "  ./benchmark_multithreaded.sh \"../../data/test_data.csv\" full_benchmark"

# 清理
rm -f benchmark_demo.sh 