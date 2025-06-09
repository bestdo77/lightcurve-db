# 项目目录结构

```
healpix_spatial_analysis/
├── README.md                    # 项目主要文档
├── quick_start.sh              # 快速启动脚本
├── PROJECT_STRUCTURE.md        # 本文件：项目结构说明
├── set_permissions.sh          # 权限设置脚本
├── .gitignore                  # Git 忽略规则
├── sourceid_healpix_map.csv    # source_id 与 healpix_id 映射表（根目录副本）
│
├── scripts/                    # 脚本文件目录
│   ├── quick_import.py         # TDengine HealPix 数据导入脚本
│   ├── generate_astronomical_data.py     # 天文观测数据生成脚本
│   ├── query_test.py           # HealPix 空间索引性能测试脚本
│   ├── db_info.sh              # 数据库信息查询脚本
│   └── temporal_queries_examples.sql    # 时间查询示例SQL
│
├── data/                       # 数据文件目录
│   ├── test_data.csv           # 测试数据文件
│   ├── merged_all.csv          # 合并数据文件
│   └── *.csv                   # 其他CSV数据文件
│
├── docs/                       # 文档目录
│   ├── original_README.md      # 原始项目文档
│   ├── 文件说明.md             # 文件功能说明
│   ├── 数据库结构.md           # 数据库结构文档
│   ├── timestamp_solution_summary.md  # 时间戳解决方案
│   └── command.md              # 常用命令
│
├── configs/                    # 配置文件目录
│   └── healpix_config.py       # 项目配置文件
│
└── output/                     # 输出结果目录
    ├── README.md               # 输出目录说明
    ├── logs/                   # 运行日志和报告
    │   ├── data_generation_report_*     # 数据生成报告
    │   ├── import_report_*              # 数据导入报告
    │   └── db_info_report_*             # 数据库信息报告
    ├── performance_reports/    # 性能测试报告
    │   └── healpix_performance_report_* # HealPix性能测试报告
    └── query_results/          # 查询结果和映射文件
        └── sourceid_healpix_map.csv     # 天体源ID与HealPix ID映射表
```

## 使用流程

1. **环境准备**：确保 TDengine 服务运行，安装必要的 Python 包
2. **数据准备**：使用 `scripts/generate_astronomical_data.py` 生成测试数据
   - 数据将保存到 `data/` 目录
   - 生成报告保存到 `output/logs/`
3. **数据导入**：使用 `scripts/quick_import.py` 导入数据到TDengine
   - 自动计算HealPix分区
   - 导入报告保存到 `output/logs/`
   - 映射文件保存到 `output/query_results/`
4. **状态检查**：使用 `scripts/db_info.sh` 检查数据库状态
   - 数据库信息报告保存到 `output/logs/`
5. **性能测试**：使用 `scripts/query_test.py` 进行HealPix空间索引性能测试
   - 性能测试报告保存到 `output/performance_reports/`

## 快速开始

运行 `bash quick_start.sh` 查看快速启动指南。
