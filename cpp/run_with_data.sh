#!/bin/bash

# TDengine HealPix 多线程导入器 - 数据路径处理脚本

echo "📁 TDengine HealPix 多线程导入器 - 数据路径处理"
echo "=================================================="

# 数据目录路径
DATA_DIR="../../data"
EXECUTABLE="./quick_import_mt"

# 检查可执行文件
if [ ! -f "$EXECUTABLE" ]; then
    echo "❌ 可执行文件不存在，请先运行编译脚本:"
    echo "   ./compile_multithreaded.sh"
    exit 1
fi

# 检查数据目录
if [ ! -d "$DATA_DIR" ]; then
    echo "❌ 数据目录不存在: $DATA_DIR"
    echo "💡 请检查数据路径，或修改此脚本中的 DATA_DIR 变量"
    exit 1
fi

echo "📂 发现数据目录: $DATA_DIR"
echo "📋 可用的数据文件:"

# 列出所有CSV文件
csv_files=()
while IFS= read -r -d '' file; do
    csv_files+=("$file")
    size=$(du -h "$file" | cut -f1)
    basename_file=$(basename "$file")
    echo "   📄 $basename_file ($size)"
done < <(find "$DATA_DIR" -name "*.csv" -print0 2>/dev/null)

if [ ${#csv_files[@]} -eq 0 ]; then
    echo "❌ 未找到CSV文件"
    exit 1
fi

echo ""
echo "🎯 选择要导入的数据文件:"

# 显示文件选择菜单
for i in "${!csv_files[@]}"; do
    basename_file=$(basename "${csv_files[$i]}")
    size=$(du -h "${csv_files[$i]}" | cut -f1)
    echo "   $((i+1)). $basename_file ($size)"
done

echo "   0. 手动输入文件路径"
echo ""

# 获取用户选择
while true; do
    read -p "请选择文件 (0-${#csv_files[@]}): " choice
    
    if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 0 ] && [ "$choice" -le "${#csv_files[@]}" ]; then
        break
    else
        echo "❌ 无效选择，请输入 0-${#csv_files[@]} 之间的数字"
    fi
done

# 处理用户选择
if [ "$choice" -eq 0 ]; then
    read -p "请输入完整的文件路径: " INPUT_FILE
    if [ ! -f "$INPUT_FILE" ]; then
        echo "❌ 文件不存在: $INPUT_FILE"
        exit 1
    fi
else
    INPUT_FILE="${csv_files[$((choice-1))]}"
fi

echo ""
echo "✅ 选择的文件: $INPUT_FILE"

# 文件大小和预估时间
file_size_mb=$(du -m "$INPUT_FILE" | cut -f1)
echo "📊 文件大小: ${file_size_mb}MB"

# 根据文件大小推荐配置
if [ "$file_size_mb" -lt 10 ]; then
    recommended_threads=4
    recommended_batch=500
    echo "💡 推荐配置 (小文件): 4线程, 批处理500"
elif [ "$file_size_mb" -lt 100 ]; then
    recommended_threads=8
    recommended_batch=1000
    echo "💡 推荐配置 (中等文件): 8线程, 批处理1000"
else
    recommended_threads=16
    recommended_batch=2000
    echo "💡 推荐配置 (大文件): 16线程, 批处理2000"
fi

echo ""

# 获取数据库名
default_db="healpix_$(basename "$INPUT_FILE" .csv)"
read -p "🎯 数据库名 (默认: $default_db): " DB_NAME
DB_NAME=${DB_NAME:-$default_db}

# 获取线程数
read -p "🧵 线程数 (默认: $recommended_threads): " THREADS
THREADS=${THREADS:-$recommended_threads}

# 获取批处理大小
read -p "📦 批处理大小 (默认: $recommended_batch): " BATCH_SIZE
BATCH_SIZE=${BATCH_SIZE:-$recommended_batch}

# 是否删除现有数据库
read -p "🗑️ 是否删除现有数据库? (y/N): " -n 1 -r DROP_DB
echo
if [[ $DROP_DB =~ ^[Yy]$ ]]; then
    DROP_FLAG="--drop_db"
else
    DROP_FLAG=""
fi

echo ""
echo "🚀 准备执行导入命令:"
echo "=============================================="
echo "命令: $EXECUTABLE \\"
echo "  --input \"$INPUT_FILE\" \\"
echo "  --db \"$DB_NAME\" \\"
echo "  --threads $THREADS \\"
echo "  --batch_size $BATCH_SIZE \\"
echo "  $DROP_FLAG"
echo "=============================================="
echo ""

read -p "▶️ 开始导入? (Y/n): " -n 1 -r START_IMPORT
echo
if [[ ! $START_IMPORT =~ ^[Nn]$ ]]; then
    echo "🎬 开始多线程导入..."
    echo ""
    
    # 记录开始时间
    start_time=$(date +%s)
    
    # 执行导入
    if $EXECUTABLE \
        --input "$INPUT_FILE" \
        --db "$DB_NAME" \
        --threads "$THREADS" \
        --batch_size "$BATCH_SIZE" \
        $DROP_FLAG; then
        
        # 记录结束时间
        end_time=$(date +%s)
        duration=$((end_time - start_time))
        
        echo ""
        echo "🎊 导入完成！"
        echo "⏱️ 总耗时: ${duration}秒"
        echo "📈 平均速度: $((file_size_mb * 1024 / duration))KB/秒"
        echo ""
        echo "💡 下一步操作："
        echo "   - 查看导入报告: output/logs/"
        echo "   - 运行查询测试验证数据"
        echo "   - 检查源ID映射文件: sourceid_healpix_map.csv"
    else
        echo ""
        echo "❌ 导入失败！请检查错误信息"
        exit 1
    fi
else
    echo "⏹️ 导入已取消"
fi

echo ""
echo "📋 常用命令参考:"
echo "  # 查看帮助"
echo "  $EXECUTABLE --help"
echo ""
echo "  # 直接指定路径导入"
echo "  $EXECUTABLE --input \"$INPUT_FILE\" --db my_db --threads 8"
echo ""
echo "  # 运行性能基准测试"
echo "  ./benchmark_multithreaded.sh \"$INPUT_FILE\" benchmark_test" 