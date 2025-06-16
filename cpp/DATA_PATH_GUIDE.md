# 数据路径使用指南

## 📁 数据文件路径处理

您的数据文件位于 `data/test_data` 目录下，这里提供几种使用方法：

## 🚀 方法1：使用交互式脚本（推荐）

```bash
# 运行交互式数据处理脚本
./run_with_data.sh
```

这个脚本会：
- 自动检测您的数据文件
- 显示文件大小和推荐配置
- 提供交互式菜单选择
- 根据文件大小智能推荐线程数和批处理大小

## 🎯 方法2：直接指定路径

### 相对路径方式：
```bash
# 使用相对路径（从当前cpp目录）
./quick_import_mt --input "../../data/test_data.csv" --db healpix_test --threads 8

# 如果是small_test.csv
./quick_import_mt --input "../../data/small_test.csv" --db healpix_small --threads 4

# 如果是merged_all.csv
./quick_import_mt --input "../../data/merged_all.csv" --db healpix_merged --threads 16
```

### 绝对路径方式：
```bash
# 使用绝对路径
./quick_import_mt --input "/home/sw7777/TDengine/data/test_data.csv" --db healpix_test --threads 8
```

## 📊 根据文件大小选择配置

根据您的数据文件：

| 文件名 | 大小 | 推荐配置 |
|--------|------|----------|
| `small_test.csv` | ~5KB | 线程数：2-4，批处理：500 |
| `merged_all.csv` | ~274MB | 线程数：8-12，批处理：1000 |
| `test_data.csv` | ~954MB | 线程数：16-24，批处理：2000 |

## 🧵 线程数选择建议

```bash
# 小文件 (< 10MB)
./quick_import_mt --input "../../data/small_test.csv" --db test_small --threads 4 --batch_size 500

# 中等文件 (10MB - 500MB)  
./quick_import_mt --input "../../data/merged_all.csv" --db test_merged --threads 8 --batch_size 1000

# 大文件 (> 500MB)
./quick_import_mt --input "../../data/test_data.csv" --db test_large --threads 16 --batch_size 2000
```

## 🔧 完整命令示例

### 示例1：导入测试数据（推荐从这个开始）
```bash
./quick_import_mt \
    --input "../../data/small_test.csv" \
    --db "healpix_small_test" \
    --threads 4 \
    --batch_size 500 \
    --drop_db
```

### 示例2：导入大数据集
```bash
./quick_import_mt \
    --input "../../data/test_data.csv" \
    --db "healpix_production" \
    --threads 16 \
    --batch_size 2000 \
    --nside_base 128 \
    --nside_fine 512 \
    --drop_db
```

## 🚀 性能基准测试

```bash
# 对特定文件运行性能测试
./benchmark_multithreaded.sh "../../data/test_data.csv" benchmark_test
```

## 💡 常见问题解决

### 问题1：路径错误
```bash
# 检查文件是否存在
ls -la "../../data/"

# 如果路径不对，使用绝对路径
pwd  # 显示当前路径
ls -la /home/sw7777/TDengine/data/
```

### 问题2：权限问题
```bash
# 检查文件权限
ls -la "../../data/test_data.csv"

# 如果需要，修改权限
chmod 644 "../../data/test_data.csv"
```

### 问题3：内存不足
```bash
# 对于大文件，减少线程数和批处理大小
./quick_import_mt \
    --input "../../data/test_data.csv" \
    --db "healpix_safe" \
    --threads 8 \
    --batch_size 500
```

## 📋 快速开始

1. **首次使用**（推荐）：
   ```bash
   ./run_with_data.sh
   ```

2. **熟悉后直接使用**：
   ```bash
   ./quick_import_mt --input "../../data/small_test.csv" --db test_db --threads 4 --drop_db
   ```

3. **查看详细帮助**：
   ```bash
   ./quick_import_mt --help
   ```

## 🎊 享受高性能导入！

现在您可以轻松处理不同路径的数据文件，并享受多线程带来的性能提升！ 