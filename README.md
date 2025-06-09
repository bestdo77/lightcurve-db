# TDengine Healpix 空间分析项目

## 项目概述

本项目专门用于处理天体观测数据的空间分析，基于 **Healpix 空间分区技术**实现高效的空间检索与查询。项目结合 TDengine 时序数据库，提供完整的天体数据导入、存储、检索和性能测试解决方案。

## 核心特性

- ✅ **Healpix 空间分区**：自适应空间索引，支持最近邻和锥形检索
- ✅ **高性能空间查询**：相比全表扫描，查询效率提升 2-10 倍
- ✅ **大规模数据支持**：支持百万级天体观测数据
- ✅ **智能分区策略**：根据天体密度自动调整分区粒度
- ✅ **完整性能测试**：提供详细的空间查询性能对比分析

## 项目文件结构

### 核心脚本

| 文件名 | 主要功能 | 使用说明 |
|--------|----------|----------|
| `quick_import_healpix.py` | **主要导入脚本**，将天体数据按 healpix 分区导入 TDengine | `python3 quick_import_healpix.py --input data.csv --db sensor_db_healpix` |
| `generate_astronomical_data.py` | 生成天体观测模拟数据 | `python3 generate_astronomical_data.py --num_sources 1000 --records_per_source 1000` |
| `db_info.sh` | **数据库状态查询脚本**，获取数据库详细信息 | `bash db_info.sh` |
| `temporal_query_test.py` | **时间查询性能测试脚本**，测试不同时间范围的查询性能 | `python3 temporal_query_test.py --db sensor_db_healpix` |
| `temporal_queries_examples.sql` | 时间查询示例 SQL，包含各种时间查询模式 | `taos -f temporal_queries_examples.sql` |
| `spatial_queries.sql` | 空间查询性能测试结果展示 | 查看空间查询测试结果 |

### 数据文件

| 文件名 | 内容说明 | 大小 |
|--------|----------|------|
| `generated_data_large.csv` | **主要数据集**：大规模天体观测数据 | ~955MB |
| `merged_all.csv` | 合并后的观测数据集 | ~274MB |
| `sourceid_healpix_map.csv` | source_id 与 healpix_id 的映射表 | ~1.1MB |

### 文档与配置

| 文件名 | 内容说明 |
|--------|----------|
| `README.md` | 原项目完整文档 |
| `文件说明.md` | 详细的文件功能说明 |
| `数据库结构.md` | TDengine 数据库结构文档 |
| `timestamp_solution_summary.md` | 时间戳处理方案详细说明 |
| `command.md` | 常用命令记录 |

## Healpix 空间分区原理

### 什么是 Healpix？
Healpix (Hierarchical Equal Area isoLatitude Pixelization) 是一种球面分区算法，将球面等面积划分为多个像素，广泛用于天文学和地理信息系统。

### 项目中的应用
1. **初始分区**：使用 nside=64 进行粗分区
2. **自适应细分**：当某区域天体数 > 10,000 时，自动提升到 nside=256
3. **唯一标识**：生成全局唯一的 healpix_id
4. **空间索引**：支持快速的最近邻和锥形检索

```python
# 核心算法示例
if count_in_base_healpix > 10000:
    fine_id = healpy.ang2pix(nside_fine, ra, dec, nest=True)
    healpix_id = (base_healpix << 32) + fine_id
else:
    healpix_id = base_healpix
```

## 数据库结构

### 超级表定义
```sql
CREATE STABLE sensor_data (
    ts TIMESTAMP,           -- 观测时间戳
    ra DOUBLE,              -- 赤经
    dec DOUBLE,             -- 赤纬  
    mag DOUBLE,             -- 亮度
    jd_tcb DOUBLE          -- 儒略日
) TAGS (
    healpix_id BIGINT,     -- 空间分区编号
    source_id BIGINT       -- 天体源编号
);
```

### 子表命名规则
- 子表格式：`sensor_{healpix_id}_{source_id}`
- 示例：`sensor_12345_1001`
- 每个 (healpix_id, source_id) 组合对应一个子表

## 快速开始

### 1. 环境准备
```bash
# 确保 TDengine 服务运行
sudo systemctl start taosd

# 安装 Python 依赖
pip install pandas taos healpy numpy tqdm
```

### 2. 生成测试数据
```bash
# 生成 1000 个天体，每个 1000 条记录（数据将保存到 data/ 目录）
python3 scripts/generate_astronomical_data.py --num_sources 1000 --records_per_source 1000 --output data/test_data.csv
```

### 3. 导入数据（Healpix 分区）
```bash
# 导入数据并自动计算 healpix 分区（报告将保存到 output/ 目录）
python3 scripts/quick_import.py --input data/test_data.csv --db sensor_db_healpix
```

### 4. 查看数据库状态
```bash
# 查看导入结果和数据库信息（报告将保存到 output/logs/ 目录）
bash scripts/db_info.sh
```

### 5. 运行空间查询性能测试
```bash
# 测试 HealPix 空间索引性能（报告将保存到 output/performance_reports/ 目录）
python3 scripts/query_test.py
```

### 6. 学习时间查询示例
```bash
# 运行各种时间查询示例
taos -f scripts/temporal_queries_examples.sql

# 或查看文件内容学习SQL语法
cat scripts/temporal_queries_examples.sql
```

## 输出目录结构

项目运行后，所有结果将保存在 `output/` 目录中：

```
output/
├── logs/                          # 运行日志和报告
│   ├── data_generation_report_*   # 数据生成报告
│   ├── import_report_*            # 数据导入报告
│   └── db_info_report_*           # 数据库信息报告
├── performance_reports/           # 性能测试报告
│   └── healpix_performance_report_* # HealPix性能测试报告
└── query_results/                 # 查询结果和映射文件
    └── sourceid_healpix_map.csv   # 天体源ID与HealPix ID映射表
```

### 报告文件说明
- **数据生成报告**：包含生成的测试数据统计信息
- **导入报告**：记录数据导入过程的详细统计
- **数据库信息报告**：数据库状态、表结构、数据分布等信息
- **性能测试报告**：HealPix空间索引的性能测试结果

## 空间查询性能

### 测试场景
根据提供的 `spatial_queries.sql` 测试结果，以下是空间查询性能对比：

#### 最近邻检索（100个天体）
| 方法 | 平均速度 | 总耗时 |
|------|----------|--------|
| Healpix 索引 | 81.17 star/s | 1.2秒 |
| 全表扫描 | 32.98 star/s | 3.0秒 |
| **性能提升** | **2.46倍** | **2.5倍加速** |

#### 锥形检索（不同半径）
| 半径 | Healpix 方法耗时 | 全表扫描耗时 | 性能提升 |
|------|------------------|--------------|----------|
| 0.01° | 0.0秒 | 3.4秒 | **无限倍** |
| 0.05° | 0.2秒 | 3.2秒 | **16倍** |
| 0.1° | 0.9秒 | 3.0秒 | **3.3倍** |
| 0.5° | 1.8秒 | 2.7秒 | **1.5倍** |
| 1.0° | 1.8秒 | 2.6秒 | **1.4倍** |

#### 时间区间查询
| 时间范围 | 查询次数 | 总耗时 | 平均耗时 |
|----------|----------|--------|----------|
| 近一月 | 100次 | 3.190秒 | 0.032秒/次 |
| 近一季度 | 100次 | 4.713秒 | 0.047秒/次 |
| 近半年 | 100次 | 7.236秒 | 0.072秒/次 |

## 时间查询性能测试

### 测试功能
`temporal_query_test.py` 脚本提供全面的时间查询性能测试，包括：

1. **单天体时间序列查询**：测试指定天体不同时间范围的查询性能
2. **多天体聚合查询**：测试多个天体的统计聚合性能  
3. **时间范围覆盖**：从最近1小时到全部历史数据的完整测试
4. **查询类型多样**：计数查询、数据查询、聚合查询的性能对比

### 测试命令
```bash
# 基础测试（自动选择5个天体）
python3 temporal_query_test.py --db sensor_db_healpix

# 指定天体测试
python3 temporal_query_test.py --db sensor_db_healpix --source_ids 1001,1002,1003

# 自定义输出文件
python3 temporal_query_test.py --db sensor_db_healpix --output my_temporal_test.csv
```

### 测试结果解读
测试完成后会生成 CSV 报告和控制台汇总，关键指标包括：

- **查询效率**：条/秒，越高越好（推荐 >10K 条/秒）
- **响应时间**：毫秒级，越低越好（推荐 <100ms）
- **数据量适应性**：不同时间范围下的性能稳定性

### 优化建议
- **时间索引**：TDengine 自动为时间戳建立索引，查询时优先使用时间条件
- **分区查询**：结合 `healpix_id` 和 `source_id` 可进一步提升性能
- **批量处理**：大范围查询建议分批处理，避免内存占用过高

## 空间检索算法

### 1. 最近邻检索流程
```python
def nearest_neighbor_search(target_ra, target_dec):
    # 1. 计算目标点的 healpix 区块
    center_healpix = healpy.ang2pix(nside, target_ra, target_dec)
    
    # 2. 获取相邻区块
    neighbors = healpy.get_all_neighbours(nside, center_healpix)
    
    # 3. 只在相关区块中搜索
    search_healpix = [center_healpix] + list(neighbors)
    
    # 4. 精确计算角距离
    return find_closest_in_healpix_regions(search_healpix)
```

### 2. 锥形检索流程
```python
def cone_search(center_ra, center_dec, radius):
    # 1. 计算覆盖该圆锥的所有 healpix 区块
    vec = healpy.ang2vec(center_ra, center_dec)
    healpix_list = healpy.query_disc(nside, vec, radius)
    
    # 2. 只查询相关区块
    candidates = query_healpix_regions(healpix_list)
    
    # 3. 精确过滤
    return filter_by_angular_distance(candidates, radius)
```

## 使用建议

### 数据规模建议
- **小规模**（< 10万天体）：使用 nside=64 即可
- **中规模**（10万-100万天体）：推荐自适应分区策略
- **大规模**（> 100万天体）：考虑使用更高 nside 或多层次分区

### 查询优化建议
1. **最近邻查询**：优先使用 healpix 方法，比全表扫描快 2-3 倍
2. **小半径锥形查询**（< 0.1°）：healpix 方法有巨大优势
3. **大半径锥形查询**（> 1°）：两种方法性能接近，可根据数据分布选择

### 运维建议
1. **监控子表数量**：避免过多小表影响元数据管理
2. **定期统计**：使用 `db_info.sh` 检查数据分布
3. **备份映射表**：`sourceid_healpix_map.csv` 对数据恢复很重要

## 常见问题

### Q1: 导入时出现 healpix 计算错误
**解决方法**：
- 检查 ra/dec 数据范围是否正确（ra: 0-360°, dec: -90°到+90°）
- 安装最新版本的 healpy：`pip install --upgrade healpy`

### Q2: 空间查询性能不如预期
**检查项目**：
- 数据是否按 healpix 正确分区
- 查询半径是否过大（建议 < 1°）
- 数据分布是否均匀

### Q3: 子表数量过多
**优化方案**：
- 调整自适应分区阈值（降低 count_threshold）
- 使用更低的 nside_base
- 考虑按时间分区与空间分区结合

## 技术特点

- **高效空间索引**：基于 Healpix 的层次化空间分区
- **自适应分区**：根据数据密度动态调整分区粒度  
- **海量数据支持**：支持千万级天体观测记录
- **完整性能测试**：提供详细的查询性能对比
- **易于扩展**：模块化设计，支持自定义空间算法

---

**本项目为天体观测数据的空间分析提供了完整的解决方案，适用于天文台、科研院所以及任何需要处理空间时序数据的场景。**

## 贡献与支持

本项目的 Healpix 空间分区方案已在大规模天体数据中验证，可直接用于：
- 天文观测数据管理系统
- 地理信息空间分析
- IoT 设备位置数据处理
- 任何需要高效空间检索的时序数据场景

如需技术支持或功能扩展，欢迎提供反馈！ 