#!/bin/bash

# 设置颜色
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}========== TDengine 数据库关键信息 ==========${NC}"

echo -e "\n${BLUE}1. 数据库基本信息${NC}"
echo -e "数据库列表:"
taos -s "SHOW DATABASES;" 2>/dev/null | grep -E "^\s*(sensor_db_healpix|test|information_schema|performance_schema)" || echo "无法获取数据库列表"

echo -e "\n${BLUE}2. 数据统计${NC}"
echo -e "总记录数:"
count_result=$(taos -s "USE sensor_db_healpix; SELECT COUNT(*) FROM sensor_data;" 2>/dev/null | grep -E "^\s*[0-9]+\s*\|" | awk -F'|' '{print $1}' | tr -d ' ')
if [ -n "$count_result" ]; then
    echo "  $count_result 条记录"
else
    echo "  数据库为空或查询失败"
fi

echo -e "子表数量:"
subtable_result=$(taos -s "USE sensor_db_healpix; SELECT COUNT(*) FROM (SELECT TBNAME FROM sensor_data GROUP BY TBNAME);" 2>/dev/null | grep -E "^\s*[0-9]+\s*\|" | awk -F'|' '{print $1}' | tr -d ' ')
if [ -n "$subtable_result" ]; then
    echo "  $subtable_result 个子表"
else
    echo "  无子表或查询失败"
fi

echo -e "\n${BLUE}3. 数据范围${NC}"
echo -e "时间范围:"
time_range=$(taos -s "USE sensor_db_healpix; SELECT FIRST(ts), LAST(ts) FROM sensor_data;" 2>/dev/null | grep -E "[0-9]{4}-[0-9]{2}-[0-9]{2}")
if [ -n "$time_range" ]; then
    echo "  $time_range"
else
    echo "  无数据或查询失败"
fi

echo -e "经纬度范围:"
coord_range=$(taos -s "USE sensor_db_healpix; SELECT MIN(ra) as min_ra, MAX(ra) as max_ra, MIN(dec) as min_dec, MAX(dec) as max_dec FROM sensor_data;" 2>/dev/null | grep -E "[0-9]+\.[0-9]+")
if [ -n "$coord_range" ]; then
    echo "  $coord_range"
else
    echo "  无数据或查询失败"
fi

echo -e "\n${BLUE}4. 数据库配置信息${NC}"
echo -e "数据库详细信息:"
db_info=$(taos -s "SHOW DATABASES;" 2>/dev/null | grep sensor_db_healpix)
if [ -n "$db_info" ]; then
    echo "  $db_info"
else
    echo "  sensor_db_healpix 不存在"
fi

echo -e "\n${BLUE}5. 表结构信息${NC}"
echo -e "超级表结构:"
table_desc=$(taos -s "USE sensor_db_healpix; DESCRIBE sensor_data;" 2>/dev/null | grep -v "Query OK")
if [ -n "$table_desc" ]; then
    echo "$table_desc"
else
    echo "  sensor_data 表不存在或查询失败"
fi

echo -e "\n${BLUE}6. 系统状态${NC}"
echo -e "数据节点状态:"
dnode_status=$(taos -s "SHOW DNODES;" 2>/dev/null | grep -E "ready|offline" | head -5)
if [ -n "$dnode_status" ]; then
    echo "$dnode_status"
else
    echo "  无法获取节点状态"
fi

echo -e "\n${BLUE}7. TDengine进程信息${NC}"
echo -e "进程状态:"
taosd_process=$(ps aux | grep taosd | grep -v grep)
if [ -n "$taosd_process" ]; then
    echo "$taosd_process"
    echo -e "\n内存占用："
    ps aux | grep taosd | grep -v grep | awk '{sum+=$6} END {if(sum>0) print "RSS总计: " sum/1024 " MB"; else print "无法计算内存使用"}'
else
    echo "  TDengine服务未运行"
fi

echo -e "\n${GREEN}========== 诊断信息 ==========${NC}"
# 检查数据库是否真的为空
echo -e "${YELLOW}数据库诊断：${NC}"
if [ "$count_result" = "0" ] || [ -z "$count_result" ]; then
    echo -e "${RED}⚠️  数据库为空！${NC}"
    echo "可能的原因："
    echo "  1. 数据还未导入"
    echo "  2. 导入过程失败"
    echo "  3. 数据被清空"
    echo ""
    echo "建议操作："
    echo "  - 检查是否有 .csv 数据文件"
    echo "  - 运行 python3 timestamp_solution.py 重新导入数据"
else
    echo -e "${GREEN}✅ 数据库包含 $count_result 条记录${NC}"
fi

# 生成数据库信息报告
timestamp=$(date +"%Y%m%d_%H%M%S")
report_file="output/logs/db_info_report_${timestamp}.txt"

# 确保输出目录存在
mkdir -p output/logs

echo -e "\n${BLUE}正在生成详细报告...${NC}"

# 生成详细报告
{
    echo "================================================================================"
    echo "🗄️  TDengine HealPix 数据库信息报告"
    echo "================================================================================"
    echo "查询时间: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "数据库: sensor_db_healpix"
    echo ""
    
    echo "📊 数据库列表:"
    taos -s "SHOW DATABASES;" 2>/dev/null | grep -E "^\s*(sensor_db_healpix|test|information_schema|performance_schema)" || echo "无法获取数据库列表"
    
    echo ""
    echo "📈 数据统计:"
    echo "总记录数: $count_result 条记录"
    echo "子表数量: $subtable_result 个子表"
    
    echo ""
    echo "⏰ 数据时间范围:"
    echo "$time_range"
    
    echo ""
    echo "🌍 空间覆盖范围:"
    echo "$coord_range"
    
    echo ""
    echo "🏗️  表结构信息:"
    echo "$table_desc"
    
    echo ""
    echo "🔧 系统状态:"
    echo "$dnode_status"
    
    echo ""
    echo "💻 TDengine进程信息:"
    echo "$taosd_process"
    
    echo ""
    echo "📋 诊断结果:"
    if [ "$count_result" = "0" ] || [ -z "$count_result" ]; then
        echo "⚠️  数据库为空！"
        echo "建议: 检查数据文件并重新导入"
    else
        echo "✅ 数据库正常，包含 $count_result 条记录"
    fi
    
} > "$report_file"

echo -e "\n${GREEN}========== 查询完成 ==========${NC}"
echo -e "${BLUE}📄 详细报告已保存到: $report_file${NC}"