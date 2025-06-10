#!/bin/bash
# HealPix C++ 依赖安装脚本

echo "🔧 安装 HealPix C++ 依赖包..."

# 更新包管理器
sudo apt update

# 安装基础开发工具
sudo apt install -y build-essential cmake pkg-config

# 安装 CFITSIO (HealPix 的核心依赖)
sudo apt install -y libcfitsio-dev

# 安装数学库
sudo apt install -y libgsl-dev

# 安装 HealPix C++ 包
sudo apt install -y libhealpix-cxx-dev

# 如果上面的包不可用，可以从源码编译
echo "📦 检查 HealPix 安装..."
if ! pkg-config --exists healpix_cxx; then
    echo "⚠️ 系统包管理器中没有 HealPix，将从源码安装..."
    
    # 创建临时目录
    mkdir -p /tmp/healpix_build
    cd /tmp/healpix_build
    
    # 下载 HealPix 源码
    wget https://sourceforge.net/projects/healpix/files/Healpix_3.82/Healpix_3.82_2022Jul28.tar.gz
    tar -xzf Healpix_3.82_2022Jul28.tar.gz
    cd Healpix_3.82
    
    # 配置编译选项
    echo "export HEALPIX=/usr/local/healpix" >> ~/.bashrc
    echo "export PATH=\$HEALPIX/bin:\$PATH" >> ~/.bashrc
    echo "export LD_LIBRARY_PATH=\$HEALPIX/lib:\$LD_LIBRARY_PATH" >> ~/.bashrc
    
    # 编译 C++ 部分
    cd src/cxx
    make
    sudo make install
    
    echo "✅ HealPix C++ 从源码安装完成"
else
    echo "✅ HealPix C++ 已通过包管理器安装"
fi

# 安装 TDengine 开发库
echo "📦 安装 TDengine 开发库..."
wget https://www.taosdata.com/assets-download/3.0/TDengine-client-3.0.5.0-Linux-x64.tar.gz
tar -xzf TDengine-client-3.0.5.0-Linux-x64.tar.gz
cd TDengine-client-3.0.5.0
sudo ./install_client.sh

echo "🎉 所有依赖安装完成！"
echo "请运行 'source ~/.bashrc' 或重新登录来使环境变量生效" 