# 🚀 HealPix + TDengine 离线部署包

## 📋 包内容

这是一个完全自包含的离线部署包，适用于无网络连接的目标机器。

### 📦 文件清单

- **healpix-minimal-offline.tar** (612MB) - HealPix应用镜像
- **tdengine-3.3.6.6.tar** (906MB) - TDengine数据库镜像  
- **docker-compose.yml** - 服务编排配置
- **deploy.sh** - 一键部署脚本
- **OFFLINE_DEPLOY.md** - 详细部署指南
- **offline_deps/** - 离线依赖包

**总大小**: 1.6GB

## 🚀 快速部署

### 1. 环境要求
- Docker Engine (≥20.10)
- Docker Compose (≥1.29)
- 存储空间 ≥3GB

### 2. 一键部署
```bash
# 授权执行
chmod +x deploy.sh

# 执行部署
./deploy.sh
```

## 🧪 功能测试

部署成功后，可以使用以下命令测试：

```bash
# 生成测试数据
docker exec -it healpix-offline /app/bin/generate_astronomical_data --num_sources 10000 --records_per_source 100 --output /app/data/test_data.csv

# 多线程导入数据
docker exec -it healpix-offline /app/bin/quick_import --input /app/data/test_data.csv --db sensor_db_healpix --host tdengine-offline --threads 8 --drop_db

# 异步查询测试
docker exec -it healpix-offline /app/bin/query_test_async --nearest 500 --cone 100 --host tdengine-offline --db sensor_db_healpix
```

## 📊 性能指标

基于之前的测试结果：

### ⚡ 异步查询性能
- **最近邻查询**: 500次/0.71秒 = 704查询/秒
- **锥形检索**: 平均4.4ms/查询
- **时间区间查询**: 0.044ms/查询

### 💾 资源占用
- **应用镜像**: 629MB
- **TDengine镜像**: 906MB
- **运行时内存**: ~2GB
- **磁盘空间**: ~3GB

## 🛠️ 管理命令

```bash
# 查看服务状态
docker-compose ps

# 查看日志
docker-compose logs -f

# 停止服务
docker-compose down

# 进入容器调试
docker-compose exec healpix-minimal shell
```

## ✅ 优势特性

- 🔒 **完全离线**: 无需目标机器联网
- 📦 **自包含**: 所有依赖预先打包
- 🚀 **即插即用**: 一键部署启动
- 💪 **高性能**: 异步查询+多线程导入
- 🛡️ **企业级**: 适用于内网环境

## 🆘 故障排除

### 服务启动失败
```bash
# 检查Docker状态
systemctl status docker

# 重新导入镜像
docker load < healpix-minimal-offline.tar
docker load < tdengine-3.3.6.6.tar
```

### 连接问题
```bash
# 检查网络
docker network ls

# 重启服务
docker-compose restart
```

---
**制作时间**: $(date)  
**版本**: v1.0 离线版  
**支持**: 企业级时空数据分析 