#!/bin/bash
# C++ 版本编译脚本

echo "🔨 开始编译 TDengine HealPix 空间分析工具 (C++ 版本)..."

# 检查是否已安装依赖
echo "🔍 检查依赖..."

# 检查 CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake 未安装，请先安装: sudo apt install cmake"
    exit 1
fi

# 检查 pkg-config
if ! command -v pkg-config &> /dev/null; then
    echo "❌ pkg-config 未安装，请先安装: sudo apt install pkg-config"
    exit 1
fi

# 检查 HealPix C++
if ! pkg-config --exists healpix_cxx; then
    echo "❌ HealPix C++ 未安装"
    echo "请运行安装脚本: chmod +x install_dependencies.sh && ./install_dependencies.sh"
    exit 1
fi

# 检查 CFITSIO
if ! pkg-config --exists cfitsio; then
    echo "❌ CFITSIO 未安装，请先安装: sudo apt install libcfitsio-dev"
    exit 1
fi

# 检查 TDengine
if [ ! -f "/usr/lib/libtaos.so" ] && [ ! -f "/usr/local/lib/libtaos.so" ]; then
    echo "❌ TDengine 客户端库未安装"
    echo "请访问 https://www.taosdata.com/ 下载并安装 TDengine 客户端"
    exit 1
fi

echo "✅ 所有依赖检查通过"

# 创建构建目录
mkdir -p build
cd build

# 清理之前的构建
echo "🧹 清理之前的构建..."
rm -rf *

# 配置项目
echo "⚙️ 配置项目..."
cmake .. -DCMAKE_BUILD_TYPE=Release

if [ $? -ne 0 ]; then
    echo "❌ CMake 配置失败"
    exit 1
fi

# 编译
echo "🔨 开始编译..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi

echo "✅ 编译成功！"
echo ""
echo "🎉 可执行文件已生成:"
echo "   - 数据生成器: build/generate_astronomical_data"
echo "   - 查询测试器: build/query_test"  
echo "   - 数据导入器: build/quick_import"
echo ""
echo "💡 使用方法:"
echo "   # 生成测试数据"
echo "   ./build/generate_astronomical_data --num_sources 1000 --records_per_source 100 --output data/test_data.csv"
echo ""
echo "   # 导入数据"
echo "   ./build/quick_import --input data/test_data.csv --db sensor_db_healpix --drop_db"
echo ""
echo "   # 运行性能测试"
echo "   ./build/query_test"
echo ""
echo "📚 获取帮助: ./build/<程序名> --help"

cd .. 