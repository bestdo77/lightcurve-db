#!/usr/bin/env python3
"""
TDengine Healpix 空间分析项目配置文件
"""

# TDengine 连接配置
TDENGINE_CONFIG = {
    'host': 'localhost',
    'user': 'root', 
    'password': 'taosdata',
    'port': 6030
}

# Healpix 分区配置
HEALPIX_CONFIG = {
    'nside_base': 64,      # 基础分辨率
    'nside_fine': 256,     # 细分分辨率
    'count_threshold': 10000,  # 细分阈值
    'nest': True           # 使用 NEST 排序
}

# 导入配置
IMPORT_CONFIG = {
    'batch_size': 500,     # 批处理大小
    'progress_interval': 100,  # 进度显示间隔
    'retry_times': 3       # 重试次数
}

# 数据验证配置
VALIDATION_CONFIG = {
    'ra_range': (0, 360),      # 赤经范围
    'dec_range': (-90, 90),    # 赤纬范围
    'mag_range': (-5, 30),     # 亮度范围
    'required_fields': ['ts', 'ra', 'dec', 'mag', 'jd_tcb', 'source_id']
}

# 查询优化配置
QUERY_CONFIG = {
    'max_healpix_per_query': 1000,  # 单次查询最大分区数
    'spatial_cache_size': 10000,    # 空间缓存大小
    'enable_parallel': True         # 启用并行查询
}
