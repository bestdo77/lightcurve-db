#!/bin/bash

# 多线程 TDengine HealPix 导入器编译脚本

echo "🔧 编译多线程 TDengine HealPix 数据导入器..."

# 检查必需的依赖
if ! command -v pkg-config &> /dev/null; then
    echo "❌ 需要安装 pkg-config"
    exit 1
fi

# 设置编译参数
CXX=${CXX:-g++}
CXXFLAGS="-std=c++17 -O3 -Wall -Wextra -pthread"
LDFLAGS="-pthread"

# 检查 HealPix 库
if pkg-config --exists healpix_cxx; then
    HEALPIX_CFLAGS=$(pkg-config --cflags healpix_cxx)
    HEALPIX_LIBS=$(pkg-config --libs healpix_cxx)
    echo "✅ 找到 HealPix 库"
else
    echo "⚠️  使用默认 HealPix 路径"
    HEALPIX_CFLAGS="-I/usr/local/include"
    HEALPIX_LIBS="-lhealpix_cxx -lcfitsio"
fi

# TDengine 库设置
TDENGINE_CFLAGS="-I/usr/local/taos/include"
TDENGINE_LIBS="-L/usr/local/taos/lib -ltaos"

# 编译命令
COMPILE_CMD="$CXX $CXXFLAGS $HEALPIX_CFLAGS $TDENGINE_CFLAGS quick_import.cpp -o quick_import_mt $HEALPIX_LIBS $TDENGINE_LIBS $LDFLAGS"

echo "🚀 执行编译命令:"
echo "$COMPILE_CMD"
echo ""

# 执行编译
if $COMPILE_CMD; then
    echo ""
    echo "✅ 编译成功！生成可执行文件: quick_import_mt"
    echo ""
    echo "📋 使用方法:"
    echo "  ./quick_import_mt --input data.csv --db test_db --threads 8"
    echo ""
    echo "🧵 多线程特性:"
    echo "  - 支持 1-64 个并发线程"
    echo "  - 自动连接池管理"
    echo "  - 线程安全的统计和进度报告"
    echo "  - 批量插入优化"
    echo ""
    echo "⚡ 性能建议:"
    echo "  - 线程数通常设置为 CPU 核数的 2-4 倍"
    echo "  - 对于大数据集，建议使用 8-16 个线程"
    echo "  - 可以通过 --batch_size 调整批处理大小"
else
    echo ""
    echo "❌ 编译失败！"
    echo ""
    echo "🔍 可能的解决方案:"
    echo "1. 安装 HealPix C++ 库:"
    echo "   sudo apt-get install libhealpix-cxx-dev  # Ubuntu/Debian"
    echo "   或手动编译安装 HealPix"
    echo ""
    echo "2. 安装 TDengine 客户端库:"
    echo "   从 https://www.taosdata.com 下载并安装"
    echo ""
    echo "3. 检查库路径:"
    echo "   export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:\$PKG_CONFIG_PATH"
    echo "   export LD_LIBRARY_PATH=/usr/local/taos/lib:\$LD_LIBRARY_PATH"
    exit 1
fi 