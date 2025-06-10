# TDengine HealPix 空间分析工具 (C++ 版本)

这是 TDengine HealPix 空间分析项目的 C++ 重构版本，提供了高性能的天文数据处理和空间索引功能。

## 📋 项目概述

本项目包含三个核心工具：

1. **数据生成器** (`generate_astronomical_data`) - 生成天文观测数据
2. **数据导入器** (`quick_import`) - 导入数据到 TDengine 并创建 HealPix 空间索引
3. **查询测试器** (`query_test`) - 测试空间查询性能

## 🛠️ 依赖要求

### 系统依赖
- **操作系统**: Linux (推荐 Ubuntu 18.04+)
- **编译器**: GCC 7+ 或 Clang 6+ (支持 C++17)
- **CMake**: 3.10+

### 必需库
- **HealPix C++**: 天球像素化库
- **CFITSIO**: FITS 文件 I/O 库 (HealPix 依赖)
- **TDengine 客户端**: 时序数据库客户端库
- **pkg-config**: 用于库依赖管理

## 🚀 快速安装

### 1. 自动安装依赖

```bash
# 给安装脚本执行权限
chmod +x install_dependencies.sh

# 运行安装脚本（需要 sudo 权限）
./install_dependencies.sh

# 重新加载环境变量
source ~/.bashrc
```

### 2. 手动安装依赖

如果自动安装失败，可以手动安装：

```bash
# 更新包管理器
sudo apt update

# 安装基础开发工具
sudo apt install -y build-essential cmake pkg-config

# 安装 CFITSIO
sudo apt install -y libcfitsio-dev

# 安装数学库
sudo apt install -y libgsl-dev

# 尝试安装 HealPix C++（如果可用）
sudo apt install -y libhealpix-cxx-dev

# 如果上面失败，需要从源码编译 HealPix
wget https://sourceforge.net/projects/healpix/files/Healpix_3.82/Healpix_3.82_2022Jul28.tar.gz
tar -xzf Healpix_3.82_2022Jul28.tar.gz
cd Healpix_3.82/src/cxx
make
sudo make install

# 安装 TDengine 客户端
wget https://www.taosdata.com/assets-download/3.0/TDengine-client-3.0.5.0-Linux-x64.tar.gz
tar -xzf TDengine-client-3.0.5.0-Linux-x64.tar.gz
cd TDengine-client-3.0.5.0
sudo ./install_client.sh
```

## 🔨 编译项目

```bash
# 给编译脚本执行权限
chmod +x build.sh

# 运行编译脚本
./build.sh
```

编译成功后，可执行文件将生成在 `build/` 目录下。

## 💻 使用方法

### 1. 生成测试数据

```bash
# 生成 1000 个天体，每个 100 条记录
./build/generate_astronomical_data \
    --num_sources 1000 \
    --records_per_source 100 \
    --output data/test_data.csv

# 查看帮助
./build/generate_astronomical_data --help
```

**主要参数：**
- `--num_sources`: 天体数量
- `--records_per_source`: 每个天体的观测记录数
- `--output`: 输出 CSV 文件路径

### 2. 导入数据

```bash
# 导入数据到 TDengine（删除现有数据库）
./build/quick_import \
    --input data/test_data.csv \
    --db sensor_db_healpix \
    --drop_db

# 自定义 HealPix 参数
./build/quick_import \
    --input data/test_data.csv \
    --db sensor_db_healpix \
    --nside_base 128 \
    --nside_fine 512 \
    --count_threshold 5000

# 查看帮助
./build/quick_import --help
```

**主要参数：**
- `--input`: 输入 CSV 文件路径
- `--db`: TDengine 数据库名
- `--nside_base`: 基础 HealPix 分辨率 (默认: 64)
- `--nside_fine`: 细分 HealPix 分辨率 (默认: 256)
- `--count_threshold`: 细分阈值 (默认: 10000)
- `--batch_size`: 批处理大小 (默认: 500)
- `--drop_db`: 导入前删除数据库

### 3. 运行性能测试

```bash
# 运行性能测试（确保已导入数据）
./build/query_test

# 查看帮助
./build/query_test --help
```

性能测试包括：
- **最近邻检索**: 测试 HealPix 邻居查找性能
- **锥形检索**: 测试不同半径的圆盘查询性能
- **时间区间查询**: 测试时间序列查询性能

## 📊 输出文件

程序运行后会生成以下文件：

### 数据文件
- `data/generated_data_large.csv`: 生成的天文数据
- `sourceid_healpix_map.csv`: 天体 ID 与 HealPix ID 映射表

### 报告文件
- `output/logs/data_generation_report_*.txt`: 数据生成报告
- `output/logs/import_report_*.txt`: 数据导入报告
- `output/performance_reports/healpix_performance_report_*.txt`: 性能测试报告

## 🏗️ 项目结构

```
cpp/
├── generate_astronomical_data.cpp    # 数据生成器源码
├── query_test.cpp                    # 查询测试器源码
├── quick_import.cpp                  # 数据导入器源码
├── CMakeLists.txt                    # CMake 构建文件
├── build.sh                          # 编译脚本
├── install_dependencies.sh           # 依赖安装脚本
├── README.md                         # 本文档
└── build/                            # 编译输出目录
    ├── generate_astronomical_data    # 可执行文件
    ├── query_test                    # 可执行文件
    └── quick_import                  # 可执行文件
```

## 🔧 技术特性

### HealPix 空间索引
- 使用真正的 HealPix C++ 库
- 自适应空间分区策略
- 支持多级分辨率 (基础 + 细分)
- 高效的邻居查找和圆盘查询

### TDengine 集成
- 原生 TDengine C API
- 超级表和子表结构
- 批量数据插入优化
- 完整的错误处理

### 性能优化
- C++17 现代特性
- 智能指针内存管理
- 并发和批处理优化
- 详细的性能指标

## 🐛 故障排除

### 编译错误

1. **HealPix 头文件未找到**
   ```bash
   # 检查 pkg-config
   pkg-config --exists healpix_cxx
   pkg-config --cflags healpix_cxx
   
   # 如果失败，需要重新安装 HealPix
   ```

2. **TDengine 库未找到**
   ```bash
   # 检查 TDengine 安装
   ls -la /usr/lib/libtaos.so
   ls -la /usr/local/lib/libtaos.so
   
   # 重新安装 TDengine 客户端
   ```

### 运行时错误

1. **TDengine 连接失败**
   - 确保 TDengine 服务正在运行
   - 检查连接参数（主机、端口、用户、密码）

2. **内存不足**
   - 减少 `--num_sources` 或 `--records_per_source`
   - 调整 `--batch_size` 参数

## 📚 参考资料

- [HealPix Official Website](https://healpix.sourceforge.io/)
- [TDengine Documentation](https://docs.taosdata.com/)
- [CMake Documentation](https://cmake.org/documentation/)

## 🤝 贡献

欢迎提交 Issue 和 Pull Request 来改进项目！

## 📄 许可证

本项目采用 MIT 许可证 - 详见 [LICENSE](../LICENSE) 文件。 