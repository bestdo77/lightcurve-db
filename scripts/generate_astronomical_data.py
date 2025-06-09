#!/usr/bin/env python3
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
import random
import argparse

# 生成天文观测数据

def generate_astronomical_data(num_sources=100, records_per_source=100000):
    """生成天文观测数据"""
    print(f"生成 {num_sources} 个天体，每个 {records_per_source} 条记录的数据...")
    
    # 初始化数据列表
    data = []
    
    # 生成数据
    for source_id in range(1, num_sources + 1):
        # 使用2024年的数据，避免时间戳超出范围
        base_time = datetime(2024, 1, 1)
        for _ in range(records_per_source):
            # 随机生成观测时间 - 分布在2024年全年
            time_offset = timedelta(days=random.randint(0, 364), 
                                  hours=random.randint(0, 23),
                                  minutes=random.randint(0, 59),
                                  seconds=random.randint(0, 59))
            obs_time = base_time + time_offset
            
            # 随机生成观测数据
            ra = random.uniform(0, 360)  # 赤经
            dec = random.uniform(-90, 90)  # 赤纬
            mag = random.uniform(8, 18)  # 星等（天文观测中常见范围）
            jd_tcb = 2460311.0 + random.uniform(0, 365)  # 2024年对应的儒略日
            
            # 添加到数据列表
            data.append((obs_time, source_id, ra, dec, mag, jd_tcb))
    
    # 创建DataFrame
    df = pd.DataFrame(data, columns=['ts', 'source_id', 'ra', 'dec', 'mag', 'jd_tcb'])
    
    # 按时间排序，模拟真实观测数据
    df = df.sort_values('ts').reset_index(drop=True)
    
    # 保存为CSV
    output_file = "generated_data_large.csv"
    df.to_csv(output_file, index=False)
    print(f"数据已保存到 {output_file}")
    print(f"总记录数: {len(df):,}")
    print(f"时间范围: {df['ts'].min()} 到 {df['ts'].max()}")
    print(f"源数量: {df['source_id'].nunique()}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--num_sources", type=int, default=100000, help="天体数量")
    parser.add_argument("--records_per_source", type=int, default=100, help="每个天体的记录数")
    parser.add_argument("--output", type=str, default="data/generated_data_large.csv", help="输出文件名（相对于项目根目录）")
    args = parser.parse_args()

    print(f"生成 {args.num_sources} 个天体，每个 {args.records_per_source} 条记录的数据...")

    data = []
    base_time = datetime(2024, 1, 1)
    for source_id in range(1, args.num_sources + 1):
        # 为每个天体生成一个中心坐标，均匀分布在全天球
        ra_center = random.uniform(0, 360)
        dec_center = random.uniform(-90, 90)
        for _ in range(args.records_per_source):
            time_offset = timedelta(days=random.randint(0, 364),
                                   hours=random.randint(0, 23),
                                   minutes=random.randint(0, 59),
                                   seconds=random.randint(0, 59))
            obs_time = base_time + time_offset
            # 在中心附近微小扰动
            ra = ra_center + random.uniform(-0.001, 0.001)
            dec = dec_center + random.uniform(-0.001, 0.001)
            mag = random.uniform(8, 18)
            jd_tcb = 2460311.0 + random.uniform(0, 365)
            data.append((obs_time, source_id, ra, dec, mag, jd_tcb))

    df = pd.DataFrame(data, columns=['ts', 'source_id', 'ra', 'dec', 'mag', 'jd_tcb'])
    df = df.sort_values('ts').reset_index(drop=True)
    
    # 确保输出目录存在
    import os
    output_dir = os.path.dirname(args.output)
    if output_dir:
        os.makedirs(output_dir, exist_ok=True)
    
    df.to_csv(args.output, index=False)
    print(f"✅ 数据已保存到 {args.output}")
    print(f"📊 总记录数: {len(df):,}")
    print(f"⏰ 时间范围: {df['ts'].min()} 到 {df['ts'].max()}")
    print(f"🌟 源数量: {df['source_id'].nunique()}")
    
    # 同时生成数据统计报告到output目录
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    report_file = f"output/logs/data_generation_report_{timestamp}.txt"
    os.makedirs("output/logs", exist_ok=True)
    
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write("=" * 60 + "\n")
        f.write("🌟 天文观测数据生成报告\n")
        f.write("=" * 60 + "\n")
        f.write(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"输出文件: {args.output}\n")
        f.write(f"天体数量: {args.num_sources:,}\n")
        f.write(f"每天体记录数: {args.records_per_source:,}\n")
        f.write(f"总记录数: {len(df):,}\n")
        f.write(f"时间范围: {df['ts'].min()} ~ {df['ts'].max()}\n")
        f.write(f"赤经范围: {df['ra'].min():.3f}° ~ {df['ra'].max():.3f}°\n")
        f.write(f"赤纬范围: {df['dec'].min():.3f}° ~ {df['dec'].max():.3f}°\n")
        f.write(f"星等范围: {df['mag'].min():.2f} ~ {df['mag'].max():.2f}\n")
        f.write(f"文件大小: {os.path.getsize(args.output) / (1024*1024):.1f} MB\n")
    
    print(f"📄 生成报告已保存到: {report_file}") 