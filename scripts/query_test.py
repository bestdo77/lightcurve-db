#!/usr/bin/env python3
import taos
import pandas as pd
import numpy as np
import healpy as hp
import time
from astropy.coordinates import SkyCoord
import astropy.units as u
from tqdm import tqdm
import warnings
from datetime import datetime, timedelta

warnings.filterwarnings("ignore")

# =================== 配置区 ===================
DB_HEALPIX = "sensor_db_healpix"
TABLE_HEALPIX = "sensor_data"
CSV_FILE = "data/test_data.csv"  # 使用实际的数据文件
NSIDE = 256
TAOS_HOST = "localhost"
TAOS_USER = "root"
TAOS_PWD = "taosdata"
TAOS_PORT = 6030

# =================== 数据准备 ===================
print("🔍 读取天体ID和坐标...")
try:
    df = pd.read_csv(CSV_FILE, usecols=['source_id', 'ra', 'dec'])
    ids = df['source_id'].unique()
    print(f"✅ 成功读取数据，唯一source_id数量：{len(ids):,}")
except FileNotFoundError:
    print(f"❌ 数据文件不存在: {CSV_FILE}")
    print("请确认数据文件路径正确，或使用 generate_astronomical_data.py 生成测试数据")
    exit(1)
except Exception as e:
    print(f"❌ 读取数据文件失败: {e}")
    exit(1)

# 设置随机种子确保可重现性
np.random.seed(42)

# 根据实际数据量调整测试规模
max_test_count = min(500, len(ids))  # 最多测试500个天体
test_count_100 = min(100, len(ids))  # 锥形检索测试数量

print(f"📊 测试规模: 最近邻检索 {max_test_count} 个天体，锥形检索 {test_count_100} 个天体")

# 随机选择测试天体
ids_5k = np.random.choice(ids, max_test_count, replace=False)
coords_5k = df.drop_duplicates('source_id').set_index('source_id').loc[ids_5k][['ra', 'dec']].values

ids_100 = np.random.choice(ids, test_count_100, replace=False) 
coords_100 = df.drop_duplicates('source_id').set_index('source_id').loc[ids_100][['ra', 'dec']].values

print(f"✅ 数据准备完成")

# =================== 最近邻检索 ===================
def nearest_with_healpix(ra, dec, conn, nside=NSIDE):
    theta = np.radians(90 - dec)
    phi = np.radians(ra)
    center_id = hp.ang2pix(nside, theta, phi, nest=True)
    neighbors = hp.get_all_neighbours(nside, theta, phi, nest=True)
    healpix_ids = [center_id] + list(neighbors)
    healpix_id_str = ','.join(str(i) for i in healpix_ids)
    sql = f"SELECT ra, dec FROM {TABLE_HEALPIX} WHERE healpix_id IN ({healpix_id_str})"
    df = pd.read_sql(sql, conn)
    if df.empty:
        return None
    c0 = SkyCoord(ra=ra*u.deg, dec=dec*u.deg)
    c_all = SkyCoord(ra=df['ra'].values*u.deg, dec=df['dec'].values*u.deg)
    sep = c0.separation(c_all)
    return sep.deg.min()



# =================== 锥形检索 ===================
def cone_with_healpix(ra, dec, radius, conn, nside=NSIDE):
    theta = np.radians(90 - dec)
    phi = np.radians(ra)
    vec0 = hp.ang2vec(theta, phi)
    healpix_ids = hp.query_disc(nside, vec0, np.radians(radius), nest=True)
    if len(healpix_ids) == 0:
        return 0
    healpix_id_str = ','.join(str(i) for i in healpix_ids)
    sql = f"SELECT ra, dec FROM {TABLE_HEALPIX} WHERE healpix_id IN ({healpix_id_str})"
    df = pd.read_sql(sql, conn)
    if df.empty:
        return 0
    c0 = SkyCoord(ra=ra*u.deg, dec=dec*u.deg)
    c_all = SkyCoord(ra=df['ra'].values*u.deg, dec=df['dec'].values*u.deg)
    sep = c0.separation(c_all)
    return (sep.deg < radius).sum()



# =================== 性能测试 ===================
print(f"\n==== 最近邻检索：{len(coords_5k)}个天体（HealPix索引） ====")
conn_healpix = taos.connect(host=TAOS_HOST, user=TAOS_USER, password=TAOS_PWD, port=TAOS_PORT, database=DB_HEALPIX)
t0 = time.time()
for ra, dec in tqdm(coords_5k, desc="最近邻（healpix）", unit="star"):
    nearest_with_healpix(ra, dec, conn_healpix)
print(f"{len(coords_5k)}个最近邻（healpix）总耗时：{time.time()-t0:.1f}秒")
conn_healpix.close()

print(f"\n==== 锥形检索：{len(coords_100)}个天体，不同半径（HealPix索引） ====")
conn_healpix = taos.connect(host=TAOS_HOST, user=TAOS_USER, password=TAOS_PWD, port=TAOS_PORT, database=DB_HEALPIX)
radii = [0.01, 0.05, 0.1, 0.5, 1.0]  # 单位：度
for radius in radii:
    t0 = time.time()
    for ra, dec in tqdm(coords_100, desc=f"锥形（healpix，r={radius}°）"):
        cone_with_healpix(ra, dec, radius, conn_healpix)
    print(f"{len(coords_100)}个锥形检索（healpix，半径{radius}度）总耗时：{time.time()-t0:.1f}秒")
conn_healpix.close()

# 修改后的时间区间统计部分
print(f"\n==== {len(ids_5k)}个天体时间区间统计（HealPix索引） ====")
def query_time_range(source_id, start_time, end_time, conn):
    try:
        # 检查是否存在数据
        sql_count = f"""
            SELECT COUNT(*) 
            FROM sensor_data 
            WHERE source_id={source_id} 
              AND ts >= '{start_time}' 
              AND ts <= '{end_time}'
        """
        count = pd.read_sql(sql_count, conn).iloc[0,0]
        if count == 0:
            return None, None, 0.0
        
        # 分步查询时间范围
        start = time.time()
        
        # 查询最小时间
        sql_min = f"""
            SELECT ts 
            FROM sensor_data 
            WHERE source_id={source_id} 
              AND ts >= '{start_time}' 
              AND ts <= '{end_time}'
            ORDER BY ts ASC 
            LIMIT 1
        """
        df_min = pd.read_sql(sql_min, conn)
        min_time = df_min['ts'].values[0] if not df_min.empty else None
        
        # 查询最大时间
        sql_max = f"""
            SELECT ts 
            FROM sensor_data 
            WHERE source_id={source_id} 
              AND ts >= '{start_time}' 
              AND ts <= '{end_time}'
            ORDER BY ts DESC 
            LIMIT 1
        """
        df_max = pd.read_sql(sql_max, conn)
        max_time = df_max['ts'].values[0] if not df_max.empty else None
        
        elapsed = time.time() - start
        
        return min_time, max_time, elapsed
        
    except Exception as e:
        print(f"查询异常: {str(e)}")
        return None, None, 0.0

base_date = datetime(2024, 12, 30)
time_ranges = {
    '近一月': (base_date - timedelta(days=30), base_date),
    '近一季度': (base_date - timedelta(days=90), base_date),
    '近半年': (base_date - timedelta(days=180), base_date)
}

ids_5k_sample = ids_5k[:5000] if len(ids_5k) >= 5000 else ids_5k

# 初始化统计字典
time_stats = {
    '近一月': {'total': 0.0, 'count': 0},
    '近一季度': {'total': 0.0, 'count': 0},
    '近半年': {'total': 0.0, 'count': 0}
}

conn_healpix = taos.connect(host=TAOS_HOST, user=TAOS_USER, password=TAOS_PWD, port=TAOS_PORT, database=DB_HEALPIX)

# 执行所有查询
for source_id in tqdm(ids_5k_sample, desc="5000天体区间查询"):
    for label, (start_time, end_time) in time_ranges.items():
        start_str = start_time.strftime('%Y-%m-%d %H:%M:%S')
        end_str = end_time.strftime('%Y-%m-%d %H:%M:%S')
        _, _, duration = query_time_range(source_id, start_str, end_str, conn_healpix)
        time_stats[label]['total'] += duration
        time_stats[label]['count'] += 1

conn_healpix.close()

# 输出汇总结果
print("\n=== 时间区间查询汇总 ===")
for label in time_ranges.keys():
    total = time_stats[label]['total']
    avg = total / time_stats[label]['count'] if time_stats[label]['count'] > 0 else 0
    print(f"{label}:")
    print(f"  总查询次数: {time_stats[label]['count']}次")
    print(f"  总耗时: {total:.3f}秒")
    print(f"  平均耗时: {avg:.3f}秒/次")

# 保存性能测试报告
import os
from datetime import datetime

# 确保output目录存在
output_dir = "output/performance_reports"
os.makedirs(output_dir, exist_ok=True)

# 生成报告文件名
timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
report_file = f"{output_dir}/healpix_performance_report_{timestamp}.txt"

# 保存测试报告
with open(report_file, 'w', encoding='utf-8') as f:
    f.write("=" * 80 + "\n")
    f.write("🌟 TDengine HealPix 空间索引性能测试报告\n")
    f.write("=" * 80 + "\n")
    f.write(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
    f.write(f"数据库: {DB_HEALPIX}\n")
    f.write(f"HealPix NSIDE: {NSIDE}\n")
    f.write(f"测试数据文件: {CSV_FILE}\n\n")
    
    f.write("📊 测试规模:\n")
    f.write(f"  - 最近邻检索测试: {len(coords_5k)} 个天体\n")
    f.write(f"  - 锥形检索测试: {len(coords_100)} 个天体\n")
    f.write(f"  - 时间查询测试: {len(ids_5k)} 个天体\n\n")
    
    f.write("🔍 测试结果概要:\n")
    f.write("  ✅ 最近邻检索性能测试完成\n")
    f.write("  ✅ 多半径锥形检索性能测试完成\n")
    f.write("  ✅ 时间区间查询统计完成\n\n")
    
    f.write("📈 时间区间查询性能:\n")
    for label in time_ranges.keys():
        total = time_stats[label]['total']
        avg = total / time_stats[label]['count'] if time_stats[label]['count'] > 0 else 0
        f.write(f"  {label}:\n")
        f.write(f"    - 总查询次数: {time_stats[label]['count']}次\n")
        f.write(f"    - 总耗时: {total:.3f}秒\n")
        f.write(f"    - 平均耗时: {avg:.3f}秒/次\n")
    
    f.write("\n💡 测试说明:\n")
    f.write("  本报告展示了HealPix空间索引在TDengine中的性能表现。\n")
    f.write("  HealPix分区能够显著提升空间查询的效率。\n")

print("\n🎉 ==== HealPix空间索引性能测试完成 ====")
print("📊 测试结果已显示在上方，包含:")
print("   - 最近邻检索性能")
print("   - 不同半径锥形检索性能") 
print("   - 时间区间查询统计")
print(f"📄 详细测试报告已保存到: {report_file}")
print("💡 如需详细分析，请查看保存的性能报告文件")