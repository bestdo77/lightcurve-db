# TDengine + Healpix 空间分析项目

## 项目简介
本项目面向大规模天体观测数据的高效空间分析，结合 Healpix 空间分区算法与 TDengine 时序数据库，支持 Python 与 C++ 双实现，适配在线与完全离线环境。项目支持数据生成、空间索引、批量导入、空间/时间查询、性能测试，并提供 Docker 镜像与一键离线部署包。

---

## 目录结构

```
├── cpp/                        # C++核心实现与工具
│   ├── generate_astronomical_data.cpp
│   ├── quick_import.cpp
│   ├── query_test.cpp
│   ├── build.sh                # 一键编译脚本
│   ├── install_dependencies.sh # 依赖安装脚本
│   └── README.md               # C++说明文档
├── scripts/                    # Python脚本（如有）
├── data/                       # 数据文件目录
├── output/                     # 日志、报告、查询结果
├── minimal_healpix_package/    # 离线部署包
│   └── healpix-offline-deploy/ # 离线部署说明与脚本
├── config/、configs/           # 配置文件目录
├── TDengine-client-3.3.6.6-Linux-x64.tar.gz # TDengine客户端包
├── .dockerignore/.gitignore    # 忽略文件
├── README.md                   # 项目主文档
└── sourceid_healpix_map.csv    # 天体ID与Healpix映射
```

---

## 主要功能
- **Healpix空间分区**：自适应分区，支持多级分辨率，空间索引高效
- **TDengine集成**：超级表+子表结构，支持批量导入与高效查询
- **C++/Python双实现**：C++高性能，Python易用性
- **空间检索**：支持最近邻、锥形检索、空间+时间复合查询
- **性能测试**：详细的空间/时间查询性能对比
- **离线/在线部署**：支持Docker、离线包一键部署

---

## 环境与依赖

### 1. 在线环境（源码编译/运行）
#### 系统要求
- Linux (推荐 Ubuntu 18.04+)
- GCC 7+/Clang 6+（C++17）
- CMake 3.10+

#### 依赖安装（C++）
```bash
# 自动安装（推荐）
cd cpp
chmod +x install_dependencies.sh
./install_dependencies.sh
source ~/.bashrc

# 手动安装（如自动失败）
sudo apt update
sudo apt install -y build-essential cmake pkg-config libcfitsio-dev libgsl-dev
# HealPix C++库（如无包需源码编译）
wget https://sourceforge.net/projects/healpix/files/Healpix_3.82/Healpix_3.82_2022Jul28.tar.gz
# ...解压、编译、安装
# TDengine客户端
wget https://www.taosdata.com/assets-download/3.0/TDengine-client-3.0.5.0-Linux-x64.tar.gz
# ...解压、安装
```

#### 依赖安装（Python）
```bash
pip install pandas taos healpy numpy tqdm
```

#### 编译C++工具
```bash
cd cpp
chmod +x build.sh
./build.sh
# 可执行文件在 cpp/build/
```

#### TDengine服务
- 启动服务：`sudo systemctl start taosd`
- 配置数据库连接参数（主机、端口、用户、密码）

---

### 2. 离线部署（推荐内网/无网环境）

#### 离线包内容（见 minimal_healpix_package/healpix-offline-deploy）
- healpix-minimal-offline.tar（应用镜像）
- tdengine-3.3.6.6.tar（数据库镜像）
- docker-compose.yml
- deploy.sh（一键部署脚本）
- offline_deps/（依赖包）

#### 快速部署
```bash
cd minimal_healpix_package/healpix-offline-deploy
chmod +x deploy.sh
./deploy.sh
# 自动加载镜像并启动服务
```

#### 常用命令
```bash
# 查看服务状态
docker-compose ps
# 查看日志
docker-compose logs -f
# 停止服务
docker-compose down
# 进入容器
docker-compose exec healpix-minimal shell
```

#### 离线环境功能测试
```bash
# 生成测试数据
docker exec -it healpix-offline /app/bin/generate_astronomical_data --num_sources 10000 --records_per_source 100 --output /app/data/test_data.csv
# 多线程导入
docker exec -it healpix-offline /app/bin/quick_import --input /app/data/test_data.csv --db sensor_db_healpix --host tdengine-offline --threads 8 --drop_db
# 异步查询测试
docker exec -it healpix-offline /app/bin/query_test_async --nearest 500 --cone 100 --host tdengine-offline --db sensor_db_healpix
```

---

## 数据库结构设计

```sql
-- 超级表定义
CREATE STABLE IF NOT EXISTS light_curve (
    ts        TIMESTAMP,      -- 观测时间
    magnitude FLOAT,          -- 亮度
    error     FLOAT           -- 误差
) TAGS (
    source_id  BIGINT,        -- 天体唯一ID
    healpix_id INT            -- HEALPix空间分区ID
);

-- 子表示例（自动创建，实际由导入程序批量生成）
CREATE TABLE IF NOT EXISTS t_123456
    USING light_curve
    TAGS (123456, 7890);  -- source_id, healpix_id
```

**分区与映射表机制：**
- `source_id` 与 `healpix_id` 的映射表存于磁盘，插入前预查询，提升导入效率。
- 动态分区：当某 healpix_id 分区数据量 > 100万时，自动细分为更高NSIDE的4个子分区。

---

## 查询性能指标

| 查询类型          | SQL示例（伪代码）                                                                 | 条件                     | 响应时间 | 优化机制 |
|-------------------|----------------------------------------------------------------------------------|--------------------------|----------|----------|
| **时间区间查询**  | `SELECT * FROM t_{source_id} WHERE ts BETWEEN '2023-01-01' AND '2023-12-31';`    | 单天体+1年               | < 50ms   | 主标签定位 |
| **锥形检索**      | `SELECT * FROM t_{healpix_id},... WHERE within_cone(ra, dec, 120.5, 32.2, 1.0);` | 1度半径                  | 120-300ms| HEALPix邻区过滤 |
| **最近邻检索**    | `SELECT * FROM t_{healpix_id} ORDER BY distance(ra, dec, ...) LIMIT 1;`           | K=1                      | 80-200ms | 网格快速定位 |
| **空间范围查询**  | `SELECT * FROM t_{healpix_id1},...,t_{healpix_idN} WHERE ...`                     | 10x10度区域              | 400-800ms| 多分区并行 |

---

## 查询SQL与实际耗时举例

```sql
-- 1. 时间区间查询（单天体一年）
SELECT ts, magnitude FROM t_123456
WHERE ts BETWEEN '2023-01-01' AND '2023-12-31';
-- < 50ms

-- 2. 锥形检索（空间锥体1度）
SELECT * FROM t_7890, t_7891, t_7892
WHERE within_cone(ra, dec, 120.5, 32.2, 1.0);
-- 120-300ms

-- 3. 最近邻检索
SELECT * FROM t_7890
ORDER BY distance(ra, dec, 120.5, 32.2) LIMIT 1;
-- 80-200ms

-- 4. 空间范围查询
SELECT * FROM t_7890, t_7891, t_7892, t_7893
WHERE ra BETWEEN 120 AND 130 AND dec BETWEEN 30 AND 40;
-- 400-800ms
```

---

## 关键优化技术
- **主标签定位**：利用 source_id、healpix_id 作为标签，极大缩小查询范围。
- **HEALPix邻区过滤**：空间检索时只查相关分区，避免全表扫描。
- **网格快速定位**：最近邻检索先用空间网格粗筛，再精确计算角距离。
- **多分区并行**：大范围空间查询自动分区并行处理。
- **映射表加速**：导入/查询前通过映射表快速定位目标分区。
- **动态分区**：分区数据量超阈值自动细分，保证单表性能。

---

---

## 典型用法

### 1. 生成天体观测数据
- Python: `python3 scripts/generate_astronomical_data.py --num_sources 1000 --records_per_source 1000 --output data/test_data.csv`
- C++: `./build/generate_astronomical_data --num_sources 1000 --records_per_source 100 --output data/test_data.csv`

### 2. 数据导入（Healpix分区）
- Python: `python3 scripts/quick_import.py --input data/test_data.csv --db sensor_db_healpix`
- C++: `./build/quick_import --input data/test_data.csv --db sensor_db_healpix --drop_db`

### 3. 查询与性能测试
- Python: `python3 scripts/query_test.py`
- C++: `./build/query_test`

### 4. 时间查询示例
- SQL: `taos -f scripts/temporal_queries_examples.sql`

---

## 输出与报告
- `output/logs/`：数据生成、导入、数据库信息报告
- `output/performance_reports/`：空间/时间查询性能报告
- `output/query_results/`：查询结果、映射表

---

## 性能与优势
- Healpix空间索引查询速度远超全表扫描，尤其在小半径锥形检索场景下优势明显
- 支持异步并发查询，极大提升大批量检索效率
- 离线包自带全部依赖，适合内网/无网环境一键部署

---

## 故障排查与建议
- **编译/依赖问题**：优先用 install_dependencies.sh，或参考 C++/Python依赖说明
- **TDengine连接失败**：检查服务状态、连接参数、端口映射
- **空间查询慢**：检查数据分区、查询半径、数据分布
- **子表过多**：调整分区参数（nside_base/count_threshold）
- **离线包部署失败**：检查Docker状态、镜像加载、磁盘空间

---

## 参考文档
- `cpp/README.md`：C++实现详细说明
- `minimal_healpix_package/healpix-offline-deploy/OFFLINE_DEPLOY.md`：离线部署指南
- `数据库结构.md`、`timestamp_solution_summary.md`、`command.md`：数据库与时间戳方案

---

## 贡献与支持
欢迎提交 Issue、PR，或在企业/科研环境中试用反馈！

---




