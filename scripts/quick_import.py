#!/usr/bin/env python3
"""
TDengine Healpix 空间分析数据导入器（增强版）
功能：
1. 自动计算 healpix 分区
2. 自适应分区策略
3. 高性能批量导入
4. 完整错误处理和进度显示
5. 生成映射表和统计信息

使用方法：
python3 quick_import_healpix_enhanced.py --input data.csv --db sensor_db_healpix --nside_base 64 --nside_fine 256
"""

import pandas as pd
import taos
import time
import argparse
import healpy as hp
import numpy as np
from tqdm import tqdm
import os
import sys
from datetime import datetime

# 配置参数
DEFAULT_HOST = "localhost"
DEFAULT_USER = "root"
DEFAULT_PASSWORD = "taosdata"
DEFAULT_PORT = 6030

# 自适应分块参数
DEFAULT_NSIDE_BASE = 64
DEFAULT_NSIDE_FINE = 256
DEFAULT_COUNT_THRESHOLD = 10000  # 每个区块最大天体数
DEFAULT_BATCH_SIZE = 500


def print_banner():
    """打印程序标题"""
    print("=" * 80)
    print("🌟 TDengine Healpix 空间分析数据导入器（增强版）")
    print("📊 支持自适应空间分区、高性能批量导入")
    print("🚀 版本：2.0")
    print("=" * 80)


def drop_target_database(db_name, host=DEFAULT_HOST, user=DEFAULT_USER, password=DEFAULT_PASSWORD, port=DEFAULT_PORT):
    """删除目标数据库"""
    try:
        conn = taos.connect(host=host, user=user, password=password, port=port)
        cursor = conn.cursor()
        cursor.execute("SHOW DATABASES")
        dbs = [row[0] for row in cursor.fetchall()]
        if db_name in dbs:
            print(f"⚠️  正在删除数据库: {db_name}")
            cursor.execute(f"DROP DATABASE IF EXISTS {db_name}")
            print(f"✅ 数据库 {db_name} 已删除")
        else:
            print(f"ℹ️  数据库 {db_name} 不存在，无需删除")
        conn.close()
        return True
    except Exception as e:
        print(f"❌ 删除数据库失败: {e}")
        return False


def create_super_table(db_name, host=DEFAULT_HOST, user=DEFAULT_USER, password=DEFAULT_PASSWORD, port=DEFAULT_PORT):
    """创建超级表"""
    try:
        conn = taos.connect(host=host, user=user, password=password, port=port)
        cursor = conn.cursor()
        cursor.execute(f"CREATE DATABASE IF NOT EXISTS {db_name}")
        cursor.execute(f"USE {db_name}")
        cursor.execute("""
            CREATE STABLE IF NOT EXISTS sensor_data (
                ts TIMESTAMP,
                ra DOUBLE,
                dec DOUBLE,
                mag DOUBLE,
                jd_tcb DOUBLE
            ) TAGS (healpix_id BIGINT, source_id BIGINT)
        """)
        conn.close()
        print(f"✅ 超级表 sensor_data 已创建，标签为 healpix_id, source_id")
        return True
    except Exception as e:
        print(f"❌ 创建超级表失败: {e}")
        return False


def assign_adaptive_healpix_id(df, nside_base=DEFAULT_NSIDE_BASE, nside_fine=DEFAULT_NSIDE_FINE, count_threshold=DEFAULT_COUNT_THRESHOLD):
    """自适应分配 healpix ID"""
    print(f"🔧 开始自适应 healpix 分区计算...")
    print(f"   - 基础分辨率: nside={nside_base}")
    print(f"   - 细分分辨率: nside={nside_fine}")
    print(f"   - 细分阈值: {count_threshold} 天体/区块")
    
    # 1. 先用低分辨率分区
    dec_clip = np.clip(df['dec'], -90, 90)
    df['base_healpix'] = hp.ang2pix(nside_base, np.radians(90 - dec_clip), np.radians(df['ra']), nest=True)
    
    # 2. 统计每个区块天体数
    counts = df.groupby('base_healpix').size().to_dict()
    
    print(f"📊 基础分区统计:")
    print(f"   - 总区块数: {len(counts)}")
    print(f"   - 平均天体/区块: {np.mean(list(counts.values())):.1f}")
    print(f"   - 最大天体/区块: {max(counts.values())}")
    
    # 统计需要细分的区块
    large_blocks = {k: v for k, v in counts.items() if v > count_threshold}
    if large_blocks:
        print(f"⚡ 需要细分的区块: {len(large_blocks)} 个")
        for block_id, count in sorted(large_blocks.items(), key=lambda x: x[1], reverse=True)[:5]:
            print(f"   - 区块 {block_id}: {count} 天体")
    else:
        print(f"✅ 无需细分，所有区块天体数 ≤ {count_threshold}")
    
    # 3. 对于天体数>阈值的区块，提升nside细分
    def get_adaptive_healpix(row):
        dec_clip = np.clip(row['dec'], -90, 90)
        if counts[row['base_healpix']] > count_threshold:
            fine_id = hp.ang2pix(
                nside_fine,
                np.radians(90 - dec_clip),
                np.radians(row['ra']),
                nest=True
            )
            return (row['base_healpix'] << 32) + fine_id
        else:
            return row['base_healpix']
    
    tqdm.pandas(desc="🧮 分配自适应healpix_id")
    df['healpix_id'] = df.progress_apply(get_adaptive_healpix, axis=1)
    
    # 生成 source_id 与 healpix_id 的唯一映射表
    mapping_df = df.drop_duplicates('source_id')[['source_id', 'healpix_id']]
    
    # 确保输出目录存在
    os.makedirs('output/query_results', exist_ok=True)
    
    # 保存到项目根目录（向后兼容）和output目录
    root_mapping_file = 'sourceid_healpix_map.csv'
    output_mapping_file = 'output/query_results/sourceid_healpix_map.csv'
    
    mapping_df.to_csv(root_mapping_file, index=False)
    mapping_df.to_csv(output_mapping_file, index=False)
    print(f"💾 已保存映射表到 {root_mapping_file} 和 {output_mapping_file}，共 {len(mapping_df)} 条记录")
    
    return df


def validate_data(df):
    """验证数据有效性"""
    print("🔍 验证数据有效性...")
    
    # 检查必需字段
    required_fields = ['ts', 'ra', 'dec', 'mag', 'jd_tcb', 'source_id']
    missing_fields = [field for field in required_fields if field not in df.columns]
    if missing_fields:
        print(f"❌ 缺少必需字段: {missing_fields}")
        return False
    
    # 检查数据范围
    invalid_ra = df[(df['ra'] < 0) | (df['ra'] > 360)]
    invalid_dec = df[(df['dec'] < -90) | (df['dec'] > 90)]
    
    if len(invalid_ra) > 0:
        print(f"⚠️  发现 {len(invalid_ra)} 条无效赤经数据 (应在 0-360°)")
    if len(invalid_dec) > 0:
        print(f"⚠️  发现 {len(invalid_dec)} 条无效赤纬数据 (应在 -90°到+90°)")
    
    # 检查空值
    null_counts = df.isnull().sum()
    if null_counts.sum() > 0:
        print(f"⚠️  发现空值: {dict(null_counts[null_counts > 0])}")
    
    print(f"✅ 数据验证完成，共 {len(df)} 条记录")
    return True


def import_data_to_super_table(csv_file, db_name, batch_size=DEFAULT_BATCH_SIZE, 
                              nside_base=DEFAULT_NSIDE_BASE, nside_fine=DEFAULT_NSIDE_FINE,
                              count_threshold=DEFAULT_COUNT_THRESHOLD,
                              host=DEFAULT_HOST, user=DEFAULT_USER, password=DEFAULT_PASSWORD, port=DEFAULT_PORT):
    """导入数据到超级表"""
    print(f"\n🚀 开始导入数据到超级表（标签为healpix_id, source_id）")
    start_time = time.time()
    
    try:
        # 读取数据
        print(f"📖 读取数据文件: {csv_file}")
        df = pd.read_csv(csv_file)
        print(f"✅ 成功读取 {len(df):,} 条记录")
        
        # 验证数据
        if not validate_data(df):
            print("❌ 数据验证失败，停止导入")
            return False
        
    except Exception as e:
        print(f"❌ 无法读取数据文件: {e}")
        return False

    # 计算 healpix 分区（如果没有）
    if 'healpix_id' not in df.columns:
        print("🔧 输入CSV文件缺少 healpix_id 列，开始自动计算...")
        df = assign_adaptive_healpix_id(df, nside_base, nside_fine, count_threshold)

    # 连接数据库
    try:
        conn = taos.connect(
            host=host,
            user=user,
            password=password,
            port=port,
            database=db_name
        )
        cursor = conn.cursor()
        print("✅ 数据库连接成功")
    except Exception as e:
        print(f"❌ 数据库连接失败: {e}")
        return False

    # 统计信息
    total_success = 0
    total_error = 0
    unique_healpix = df['healpix_id'].nunique()
    unique_sources = df['source_id'].nunique()
    
    print(f"📊 导入统计预览:")
    print(f"   - 总记录数: {len(df):,}")
    print(f"   - 唯一 healpix 分区: {unique_healpix:,}")
    print(f"   - 唯一天体源: {unique_sources:,}")
    print(f"   - 批处理大小: {batch_size}")

    try:
        # 按 healpix_id 和 source_id 分组处理
        grouped = df.groupby(['healpix_id', 'source_id'])
        total_groups = len(grouped)
        processed_groups = 0
        
        print(f"\n🔄 开始分组导入，共 {total_groups} 个子表...")
        
        for (healpix_id, source_id), group in tqdm(grouped, total=total_groups, desc="导入分区/天体"):
            processed_groups += 1
            
            # 创建子表
            table_name = f"sensor_{healpix_id}_{source_id}"
            try:
                cursor.execute(
                    f"CREATE TABLE IF NOT EXISTS {table_name} USING sensor_data TAGS ({int(healpix_id)}, {int(source_id)})"
                )
            except Exception as e:
                print(f"\n❌ 创建子表 {table_name} 失败: {e}")
                continue
            
            # 批量插入数据
            for i in range(0, len(group), batch_size):
                batch = group.iloc[i:i+batch_size]
                values_list = []
                
                for _, row in batch.iterrows():
                    values_list.append(
                        f"('{row['ts']}', {row['ra']}, {row['dec']}, {row['mag']}, {row['jd_tcb']})"
                    )
                
                sql = f"INSERT INTO {table_name} VALUES {','.join(values_list)}"
                
                try:
                    cursor.execute(sql)
                    total_success += len(batch)
                except Exception as e:
                    # 批量失败时逐条尝试
                    for _, row in batch.iterrows():
                        try:
                            single_sql = f"INSERT INTO {table_name} VALUES ('{row['ts']}', {row['ra']}, {row['dec']}, {row['mag']}, {row['jd_tcb']})"
                            cursor.execute(single_sql)
                            total_success += 1
                        except Exception as single_e:
                            total_error += 1
            
            # 定期显示进度
            if processed_groups % 100 == 0 or processed_groups == total_groups:
                elapsed = time.time() - start_time
                rate = total_success / elapsed if elapsed > 0 else 0
                print(f"\n📈 进度: {processed_groups}/{total_groups} 分组")
                print(f"   - 成功: {total_success:,}")
                print(f"   - 失败: {total_error:,}")
                print(f"   - 速度: {rate:.0f} 行/秒")
                
    except Exception as e:
        print(f"\n❌ 导入过程出错: {e}")
    finally:
        conn.close()
    
    # 最终统计
    total_time = time.time() - start_time
    success_rate = (total_success/(total_success+total_error)*100) if (total_success+total_error) > 0 else 0
    
    # 生成导入报告
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    report_file = f"output/logs/import_report_{timestamp}.txt"
    os.makedirs("output/logs", exist_ok=True)
    
    with open(report_file, 'w', encoding='utf-8') as f:
        f.write("=" * 80 + "\n")
        f.write("🌟 TDengine HealPix 数据导入报告\n")
        f.write("=" * 80 + "\n")
        f.write(f"导入时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"源文件: {csv_file}\n")
        f.write(f"目标数据库: {db_name}\n")
        f.write(f"基础NSIDE: {nside_base}\n")
        f.write(f"细分NSIDE: {nside_fine}\n")
        f.write(f"细分阈值: {count_threshold}\n")
        f.write(f"批处理大小: {batch_size}\n\n")
        
        f.write("📊 导入统计:\n")
        f.write(f"  - 总记录数: {len(df):,}\n")
        f.write(f"  - 成功导入: {total_success:,}\n")
        f.write(f"  - 失败记录: {total_error:,}\n")
        f.write(f"  - 成功率: {success_rate:.2f}%\n")
        f.write(f"  - 总耗时: {total_time:.1f} 秒\n")
        f.write(f"  - 导入速度: {total_success/total_time:.0f} 行/秒\n" if total_time > 0 else "  - 导入速度: N/A\n")
        
        f.write("\n🏗️ 表结构统计:\n")
        f.write(f"  - HealPix分区数: {unique_healpix:,}\n")
        f.write(f"  - 天体源数: {unique_sources:,}\n")
        f.write(f"  - 子表数量: {total_groups:,}\n")
        
        f.write("\n📁 输出文件:\n")
        f.write(f"  - 映射文件: sourceid_healpix_map.csv\n")
        f.write(f"  - 映射文件副本: output/query_results/sourceid_healpix_map.csv\n")
        f.write(f"  - 导入报告: {report_file}\n")

    print(f"\n{'='*60}")
    print(f"🎉 导入完成！")
    print(f"✅ 成功导入: {total_success:,} 条")
    print(f"❌ 失败: {total_error:,} 条")
    print(f"📊 成功率: {success_rate:.2f}%")
    print(f"⏱️  总耗时: {total_time:.1f} 秒")
    print(f"🚀 导入速度: {total_success/total_time:.0f} 行/秒" if total_time > 0 else "N/A")
    print(f"📁 子表数量: {total_groups}")
    print(f"💾 映射文件: sourceid_healpix_map.csv")
    print(f"📄 导入报告: {report_file}")
    print(f"{'='*60}")
    
    return total_success > 0


def main():
    parser = argparse.ArgumentParser(
        description="TDengine Healpix 空间分析数据导入器（增强版）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例用法:
  python3 %(prog)s --input data.csv --db sensor_db_healpix
  python3 %(prog)s --input data.csv --db sensor_db_healpix --nside_base 128 --count_threshold 5000
  python3 %(prog)s --input data.csv --db sensor_db_healpix --batch_size 1000 --host 192.168.1.100
        """
    )
    
    # 必需参数
    parser.add_argument("--input", type=str, required=True, help="输入CSV文件路径")
    parser.add_argument("--db", type=str, required=True, help="TDengine数据库名")
    
    # 可选参数
    parser.add_argument("--nside_base", type=int, default=DEFAULT_NSIDE_BASE, 
                       help=f"基础healpix分辨率 (默认: {DEFAULT_NSIDE_BASE})")
    parser.add_argument("--nside_fine", type=int, default=DEFAULT_NSIDE_FINE, 
                       help=f"细分healpix分辨率 (默认: {DEFAULT_NSIDE_FINE})")
    parser.add_argument("--count_threshold", type=int, default=DEFAULT_COUNT_THRESHOLD, 
                       help=f"细分阈值 (默认: {DEFAULT_COUNT_THRESHOLD})")
    parser.add_argument("--batch_size", type=int, default=DEFAULT_BATCH_SIZE, 
                       help=f"批处理大小 (默认: {DEFAULT_BATCH_SIZE})")
    
    # 数据库连接参数
    parser.add_argument("--host", type=str, default=DEFAULT_HOST, help=f"TDengine主机 (默认: {DEFAULT_HOST})")
    parser.add_argument("--user", type=str, default=DEFAULT_USER, help=f"用户名 (默认: {DEFAULT_USER})")
    parser.add_argument("--password", type=str, default=DEFAULT_PASSWORD, help=f"密码 (默认: {DEFAULT_PASSWORD})")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help=f"端口 (默认: {DEFAULT_PORT})")
    
    # 操作选项
    parser.add_argument("--drop_db", action="store_true", help="导入前删除数据库")
    parser.add_argument("--skip_validation", action="store_true", help="跳过数据验证")
    
    args = parser.parse_args()
    
    # 打印程序信息
    print_banner()
    
    # 检查输入文件
    if not os.path.exists(args.input):
        print(f"❌ 输入文件不存在: {args.input}")
        sys.exit(1)
    
    file_size = os.path.getsize(args.input) / (1024 * 1024)  # MB
    print(f"📁 输入文件: {args.input} ({file_size:.1f} MB)")
    print(f"🎯 目标数据库: {args.db}")
    print(f"🏠 TDengine主机: {args.host}:{args.port}")
    
    # 删除数据库（如果指定）
    if args.drop_db:
        if not drop_target_database(args.db, args.host, args.user, args.password, args.port):
            print("❌ 删除数据库失败，停止执行")
            sys.exit(1)
    
    # 创建超级表
    if not create_super_table(args.db, args.host, args.user, args.password, args.port):
        print("❌ 创建超级表失败，停止执行")
        sys.exit(1)
    
    # 导入数据
    success = import_data_to_super_table(
        args.input, args.db, args.batch_size, 
        args.nside_base, args.nside_fine, args.count_threshold,
        args.host, args.user, args.password, args.port
    )
    
    if success:
        print("\n🎊 数据导入成功完成！")
        print(f"💡 下一步：运行 'bash db_info.sh' 查看数据库状态")
        sys.exit(0)
    else:
        print("\n💥 数据导入失败！")
        sys.exit(1)


if __name__ == "__main__":
    main() 