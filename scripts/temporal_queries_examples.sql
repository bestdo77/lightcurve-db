-- =====================================================
-- TDengine Healpix 空间分析 - 时间查询示例 SQL
-- =====================================================
-- 使用方法: taos -f temporal_queries_examples.sql
-- 或者复制单个查询到 TDengine 控制台执行

-- 使用数据库
USE sensor_db_healpix;

-- =====================================================
-- 1. 基础时间查询
-- =====================================================

-- 查询数据时间范围
SELECT 
    MIN(ts) as earliest_time,
    MAX(ts) as latest_time,
    COUNT(*) as total_records,
    COUNT(DISTINCT source_id) as unique_sources
FROM sensor_data;

-- 查询最近1天的数据统计
SELECT 
    COUNT(*) as record_count,
    COUNT(DISTINCT source_id) as source_count,
    MIN(mag) as min_magnitude,
    MAX(mag) as max_magnitude,
    AVG(mag) as avg_magnitude
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(1d);

-- 查询最近1周的数据统计  
SELECT 
    COUNT(*) as record_count,
    COUNT(DISTINCT source_id) as source_count,
    COUNT(DISTINCT healpix_id) as healpix_count
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(7d);

-- 查询最近1月的数据统计
SELECT 
    COUNT(*) as record_count,
    COUNT(DISTINCT source_id) as source_count
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(30d);

-- =====================================================
-- 2. 单天体时间序列查询
-- =====================================================

-- 查询指定天体的完整时间序列（替换1001为实际的source_id）
SELECT ts, ra, dec, mag, jd_tcb 
FROM sensor_data 
WHERE source_id = 1001 
ORDER BY ts 
LIMIT 100;

-- 查询指定天体最近1天的观测数据
SELECT ts, ra, dec, mag, jd_tcb 
FROM sensor_data 
WHERE source_id = 1001 
AND ts >= NOW - INTERVAL(1d)
ORDER BY ts;

-- 查询指定天体最近1周的统计信息
SELECT 
    COUNT(*) as obs_count,
    MIN(ts) as first_obs,
    MAX(ts) as last_obs,
    MIN(mag) as brightest,
    MAX(mag) as faintest,
    AVG(mag) as avg_mag,
    STDDEV(mag) as mag_stddev
FROM sensor_data 
WHERE source_id = 1001 
AND ts >= NOW - INTERVAL(7d);

-- =====================================================
-- 3. 多天体时间查询
-- =====================================================

-- 查询多个天体最近1天的观测统计
SELECT 
    source_id,
    COUNT(*) as obs_count,
    MIN(mag) as min_mag,
    MAX(mag) as max_mag,
    AVG(mag) as avg_mag
FROM sensor_data 
WHERE source_id IN (1001, 1002, 1003, 1004, 1005)
AND ts >= NOW - INTERVAL(1d)
GROUP BY source_id
ORDER BY obs_count DESC;

-- 查询TOP 10最活跃天体（最近1周）
SELECT 
    source_id,
    COUNT(*) as obs_count,
    MIN(ts) as first_obs,
    MAX(ts) as last_obs
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(7d)
GROUP BY source_id
ORDER BY obs_count DESC 
LIMIT 10;

-- =====================================================
-- 4. 按时间分组的统计查询
-- =====================================================

-- 按小时统计观测数量（最近1天）
SELECT 
    TO_CHAR(ts, 'YYYY-MM-DD HH24:00:00') as hour_group,
    COUNT(*) as obs_count,
    COUNT(DISTINCT source_id) as source_count
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(1d)
GROUP BY TO_CHAR(ts, 'YYYY-MM-DD HH24:00:00')
ORDER BY hour_group;

-- 按天统计观测数量（最近1月）
SELECT 
    TO_CHAR(ts, 'YYYY-MM-DD') as date_group,
    COUNT(*) as obs_count,
    COUNT(DISTINCT source_id) as source_count,
    AVG(mag) as avg_magnitude
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(30d)
GROUP BY TO_CHAR(ts, 'YYYY-MM-DD')
ORDER BY date_group;

-- =====================================================
-- 5. 结合 Healpix 分区的时间查询
-- =====================================================

-- 查询指定 healpix 分区最近1天的数据
SELECT 
    COUNT(*) as record_count,
    COUNT(DISTINCT source_id) as source_count,
    MIN(mag) as min_mag,
    MAX(mag) as max_mag
FROM sensor_data 
WHERE healpix_id = 12345  -- 替换为实际的healpix_id
AND ts >= NOW - INTERVAL(1d);

-- 查询最活跃的 healpix 分区（最近1周）
SELECT 
    healpix_id,
    COUNT(*) as obs_count,
    COUNT(DISTINCT source_id) as source_count
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(7d)
GROUP BY healpix_id
ORDER BY obs_count DESC 
LIMIT 10;

-- 结合空间和时间的复合查询
SELECT 
    healpix_id,
    source_id,
    COUNT(*) as obs_count,
    MIN(ts) as first_obs,
    MAX(ts) as last_obs,
    AVG(mag) as avg_mag
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(7d)
GROUP BY healpix_id, source_id
HAVING COUNT(*) > 10  -- 只显示观测次数>10的
ORDER BY obs_count DESC 
LIMIT 20;

-- =====================================================
-- 6. 时间范围性能测试查询
-- =====================================================

-- 测试不同时间范围的查询性能（计数查询）
SELECT '最近1小时' as time_range, COUNT(*) as record_count 
FROM sensor_data WHERE ts >= NOW - INTERVAL(1h)
UNION ALL
SELECT '最近1天' as time_range, COUNT(*) as record_count 
FROM sensor_data WHERE ts >= NOW - INTERVAL(1d)
UNION ALL
SELECT '最近1周' as time_range, COUNT(*) as record_count 
FROM sensor_data WHERE ts >= NOW - INTERVAL(7d)
UNION ALL
SELECT '最近1月' as time_range, COUNT(*) as record_count 
FROM sensor_data WHERE ts >= NOW - INTERVAL(30d)
UNION ALL
SELECT '全部数据' as time_range, COUNT(*) as record_count 
FROM sensor_data;

-- =====================================================
-- 7. 高级时间分析查询
-- =====================================================

-- 查询观测频率分析（每个天体的观测间隔）
SELECT 
    source_id,
    COUNT(*) as total_obs,
    (MAX(ts) - MIN(ts)) / COUNT(*) as avg_interval_seconds,
    MIN(ts) as first_obs,
    MAX(ts) as last_obs
FROM sensor_data 
WHERE source_id IN (
    SELECT source_id FROM sensor_data 
    GROUP BY source_id 
    HAVING COUNT(*) > 100
)
GROUP BY source_id
ORDER BY avg_interval_seconds;

-- 查询观测密度热图数据（按小时和天体统计）
SELECT 
    HOUR(ts) as hour_of_day,
    COUNT(*) as obs_count,
    COUNT(DISTINCT source_id) as active_sources
FROM sensor_data 
WHERE ts >= NOW - INTERVAL(7d)
GROUP BY HOUR(ts)
ORDER BY hour_of_day;

-- =====================================================
-- 8. 数据质量检查查询
-- =====================================================

-- 检查时间戳重复情况
SELECT 
    ts,
    source_id,
    COUNT(*) as duplicate_count
FROM sensor_data 
GROUP BY ts, source_id
HAVING COUNT(*) > 1
ORDER BY duplicate_count DESC;

-- 检查异常时间戳（未来时间或过早时间）
SELECT 
    COUNT(*) as future_records
FROM sensor_data 
WHERE ts > NOW;

SELECT 
    COUNT(*) as old_records 
FROM sensor_data 
WHERE ts < '2020-01-01 00:00:00';

-- 检查观测间隔异常（间隔过长或过短）
SELECT 
    source_id,
    LAG(ts) OVER (PARTITION BY source_id ORDER BY ts) as prev_ts,
    ts as curr_ts,
    ts - LAG(ts) OVER (PARTITION BY source_id ORDER BY ts) as time_diff
FROM sensor_data
WHERE source_id = 1001  -- 替换为实际source_id
ORDER BY ts
LIMIT 100;

-- =====================================================
-- 使用说明
-- =====================================================
-- 1. 将上述查询中的 source_id 和 healpix_id 替换为实际值
-- 2. 根据数据时间范围调整 INTERVAL 参数
-- 3. 可以根据需要修改 LIMIT 数量
-- 4. 对于大数据量，建议先用计数查询测试性能
-- ===================================================== 