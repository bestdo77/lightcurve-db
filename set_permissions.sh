#!/bin/bash

# 设置脚本文件执行权限

echo "🔧 设置脚本文件执行权限..."

# 当前目录的脚本
chmod +x organize_healpix_project.sh
chmod +x healpix_db_info.sh
chmod +x db_info.sh 2>/dev/null || true
chmod +x temporal_query_test.py 2>/dev/null || true

echo "✅ 权限设置完成！"
echo ""
echo "现在可以运行："
echo "  bash organize_healpix_project.sh  (组织项目结构)"
echo "  python3 temporal_query_test.py --help  (查看时间查询测试帮助)" 