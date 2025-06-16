#include "healpix_spatial_analysis/cpp/tdengine_healpix_importer.h"
#include "healpix_spatial_analysis/cpp/utils.h"
#include "healpix_spatial_analysis/cpp/healpix_utils.h"
#include "healpix_spatial_analysis/cpp/connection_pool.h"
#include "healpix_spatial_analysis/cpp/thread_safe_stats.h"
#include "healpix_spatial_analysis/cpp/intelligent_partitioning.h"
#include "healpix_spatial_analysis/cpp/optimized_record.h"

#include <taos.h>
#include <taosdef.h>
#include <taoserror.h>
#include <taosmsg.h>
#include <taosdef.h>
#include <taoserror.h>
#include <taosmsg.h>
#include <taos.h>
#include <taosdef.h>
#include <taoserror.h>
#include <taosmsg.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <execution>
#include <unordered_map>
#include <shared_mutex>
#include <condition_variable>
#include <limits>
#include <cmath>
#include <cstdint>

// 优化的导入器配置
class OptimizedTDengineHealpixImporter {
private:
    // 根据数据量调整配置
    static constexpr int OPTIMAL_BATCH_SIZE_100M = 10000;  // 一亿数据用更大批次
    static constexpr int OPTIMAL_THREAD_COUNT = std::min(32, (int)std::thread::hardware_concurrency());
    static constexpr int CONNECTION_POOL_SIZE = OPTIMAL_THREAD_COUNT * 2;
    
    // 内存优化的数据结构
    struct OptimizedRecord {
        uint64_t timestamp_ms;  // 使用毫秒时间戳，减少字符串开销
        uint32_t source_id;
        float ra, dec, mag;     // 使用float而不是double，节省50%内存
        double jd_tcb;
        uint64_t healpix_id;
        
        // 内存对齐优化
    } __attribute__((packed));
    
    // 预编译语句缓存
    std::unordered_map<uint64_t, TAOS_STMT*> prepared_statements;
    std::mutex stmt_mutex;

public:
    // 使用预编译语句批量插入
    void processImportTaskOptimized(const ImportTask& task, ThreadSafeStats& stats) {
        TAOS* task_conn = conn_pool->getConnection();
        
        try {
            // 获取或创建预编译语句
            uint64_t table_key = (task.healpix_id << 32) | task.source_id;
            TAOS_STMT* stmt = getOrCreatePreparedStatement(task_conn, table_key, task.healpix_id, task.source_id);
            
            // 批量绑定和执行
            size_t processed = 0;
            for (size_t i = 0; i < task.records.size(); i += batch_size) {
                size_t end_idx = std::min(i + batch_size, task.records.size());
                
                // 批量绑定参数
                for (size_t j = i; j < end_idx; ++j) {
                    const auto& record = *task.records[j];
                    
                    // 绑定参数（比字符串拼接快很多）
                    taos_stmt_bind_param_batch(stmt, createParamBatch(record));
                }
                
                // 执行批量插入
                if (taos_stmt_execute(stmt) == 0) {
                    stats.addSuccess(end_idx - i);
                    processed += (end_idx - i);
                } else {
                    stats.addError(end_idx - i);
                }
            }
            
        } catch (...) {
            stats.addError(task.records.size());
        }
        
        conn_pool->returnConnection(task_conn);
    }
    
private:
    TAOS_STMT* getOrCreatePreparedStatement(TAOS* conn, uint64_t table_key, 
                                          long healpix_id, int source_id) {
        std::lock_guard<std::mutex> lock(stmt_mutex);
        
        auto it = prepared_statements.find(table_key);
        if (it != prepared_statements.end()) {
            return it->second;
        }
        
        // 创建表和预编译语句
        std::string table_name = table_name + "_" + std::to_string(healpix_id) + "_" + std::to_string(source_id);
        std::string create_sql = "CREATE TABLE IF NOT EXISTS " + table_name + 
                               " USING " + table_name + " TAGS (" + std::to_string(healpix_id) + 
                               ", " + std::to_string(source_id) + ")";
        
        TAOS_RES* result = taos_query(conn, create_sql.c_str());
        taos_free_result(result);
        
        // 创建预编译语句
        TAOS_STMT* stmt = taos_stmt_init(conn);
        std::string insert_sql = "INSERT INTO " + table_name + " VALUES (?, ?, ?, ?, ?)";
        taos_stmt_prepare(stmt, insert_sql.c_str(), insert_sql.length());
        
        prepared_statements[table_key] = stmt;
        return stmt;
    }
};