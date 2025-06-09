# TDengine 时间戳精度超限和重复时间戳问题解决方案

## 问题背景

在使用TDengine导入大量天文观测数据时，遇到了以下问题：
1. **时间戳精度超限**：2011年的历史数据超出了TDengine默认的时间戳支持范围
2. **重复时间戳冲突**：同一个source_id的观测记录可能有相同的时间戳，导致数据覆盖

## 根本原因分析

### TDengine时间戳范围限制
TDengine的时间戳支持范围不是固定的1970-2038，而是动态的：
- **允许的时间戳范围**：`[Now - KEEP, Now + 100年)`
- **KEEP参数**：控制数据保留时间，默认3650天（约10年）
- **关键原理**：通过调整KEEP参数，可以支持更早的历史数据

### 重复时间戳处理机制
TDengine对重复时间戳的处理行为：
- 相同时间戳的数据会**相互覆盖**（后插入的覆盖先插入的）
- 必须通过**微秒/毫秒级差异**来确保时间戳唯一性

## 解决方案

### 1. 数据库配置优化
```python
# 计算需要的KEEP值（支持2011年数据）
days_since_2011 = (datetime.now() - datetime(2011, 1, 1)).days
keep_days = int(days_since_2011 * 1.5)  # 安全缓冲

# 创建支持历史数据的数据库
cursor.execute("DROP DATABASE IF EXISTS sensor_db")
cursor.execute("CREATE DATABASE sensor_db")
cursor.execute("USE sensor_db")
cursor.execute(f"ALTER DATABASE sensor_db KEEP {keep_days}")
```

### 2. 时间戳唯一性处理
```python
def generate_unique_timestamp_2011(self, source_id, base_time_str):
    """为每个source_id生成唯一的时间戳，保持2011年"""
    cache_key = f"{source_id}_{base_time_str}"
    
    if cache_key in self.timestamp_cache:
        # 递增毫秒确保唯一性
        self.timestamp_cache[cache_key] += 1
        milliseconds = self.timestamp_cache[cache_key]
    else:
        self.timestamp_cache[cache_key] = 0
        milliseconds = 0
    
    # 解析并添加毫秒偏移
    base_dt = datetime.strptime(base_time_str, "%Y-%m-%d %H:%M:%S")
    unique_dt = base_dt + timedelta(milliseconds=milliseconds)
    
    return unique_dt.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
```

### 3. 数据导入优化
```python
# 使用较小的批次大小避免SQL过长
batch_size = 500

# 失败时尝试单条插入来定位问题数据
try:
    cursor.execute(batch_sql)
    total_success += len(batch)
except Exception as e:
    # 单条插入定位问题
    for _, row in batch.iterrows():
        try:
            single_sql = f"INSERT INTO sensor_{source_id} VALUES (...)"
            cursor.execute(single_sql)
            total_success += 1
        except Exception as single_e:
            print(f"单条插入失败: {row['formatted_time']}, 错误: {single_e}")
            total_error += 1
```

## 测试结果

### 成功指标
- ✅ **2011年时间戳插入成功**：支持从2011-01-01到2011-12-31的所有时间戳
- ✅ **数据库配置成功**：KEEP设置为7890天，覆盖2011年至今的所有时间
- ✅ **时间戳唯一性**：100万条记录中0个重复时间戳
- ✅ **数据完整性**：保持原始的2011年时间，只通过毫秒差异确保唯一性

### 性能表现
- **处理速度**：100万条记录的时间戳优化在几分钟内完成
- **内存使用**：合理的缓存机制，不会造成内存溢出
- **导入效率**：批量插入+单条容错机制，确保最大成功率

## 最佳实践建议

### 1. 数据库设计
- **提前规划KEEP值**：根据历史数据的最早时间来设置
- **监控时间戳范围**：定期检查数据的时间戳是否在允许范围内
- **备份策略**：重要的历史数据做好备份

### 2. 数据导入
- **预处理时间戳**：导入前先解决重复时间戳问题
- **分批处理**：使用适当的批次大小（推荐500-1000条）
- **错误处理**：实现容错机制，单条插入定位问题数据

### 3. 应用开发
- **缓存管理**：合理使用时间戳缓存避免内存泄漏
- **精度控制**：根据业务需求选择合适的时间精度（毫秒/微秒）
- **监控告警**：监控导入成功率和时间戳冲突

## 关键配置参数

| 参数 | 作用 | 推荐值 | 说明 |
|------|------|--------|------|
| KEEP | 数据保留时间 | 根据历史数据计算 | 支持历史数据的关键参数 |
| DAYS | 数据文件时间跨度 | 10天 | 影响查询性能 |
| batch_size | 批量插入大小 | 500-1000 | 平衡性能和稳定性 |
| precision | 时间戳精度 | 'ms' | 毫秒精度通常足够 |

## 常见错误处理

### 错误：Timestamp data out of range
**原因**：时间戳超出了`[Now - KEEP, Now + 100年)`范围
**解决**：调整KEEP参数或时间戳映射

### 错误：Database not exist
**原因**：数据库创建失败或连接配置错误
**解决**：检查数据库创建语句和连接参数

### 错误：语法错误
**原因**：TDengine版本间语法差异
**解决**：使用简化的SQL语句，分步骤执行

## 总结

通过调整TDengine的KEEP参数和实现智能的时间戳唯一性处理，我们成功解决了：
1. 2011年历史数据的时间戳支持问题
2. 大规模数据导入中的时间戳冲突问题
3. 提供了完整的错误处理和性能优化方案

这个解决方案不仅适用于天文观测数据，也可以推广到其他需要处理历史时间序列数据的场景。 