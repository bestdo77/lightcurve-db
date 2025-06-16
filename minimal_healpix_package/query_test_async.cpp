#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <memory>
#include <algorithm>
#include <map>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include <future>
#include <thread>

// TDengine 头文件
#include <taos.h>

// HealPix C++ 库头文件
#include <healpix_cxx/healpix_base.h>
#include <healpix_cxx/pointing.h>
#include <healpix_cxx/vec3.h>
#include <healpix_cxx/rangeset.h>
#include <healpix_cxx/arr.h>

const double PI = 3.14159265358979323846;

// 度数转弧度函数
inline double deg2rad(double deg) {
    return deg * PI / 180.0;
}

class SkyCoord {
public:
    double ra, dec;
    
    SkyCoord(double ra_deg, double dec_deg) : ra(ra_deg), dec(dec_deg) {}
    
    double separation(const SkyCoord& other) const {
        // 计算天球上两点间的角距离 (弧度)
        double ra1 = ra * PI / 180.0;
        double dec1 = dec * PI / 180.0;
        double ra2 = other.ra * PI / 180.0;
        double dec2 = other.dec * PI / 180.0;
        
        double dra = ra2 - ra1;
        double a = sin(dec1) * sin(dec2) + cos(dec1) * cos(dec2) * cos(dra);
        a = std::max(-1.0, std::min(1.0, a)); // 防止数值误差
        return acos(a) * 180.0 / PI; // 转换为度
    }
};

struct TestData {
    int source_id;
    double ra, dec;
};

// 异步查询上下文结构 - 增强版本，详细记录时间
struct AsyncQueryContext {
    std::string query_type;
    int query_id;
    double target_ra, target_dec;
    double radius;  // 用于锥形查询
    std::string sql_query;  // 记录实际执行的SQL
    
    // 结果存储
    std::vector<std::pair<double, double>> coordinates;
    int result_count;
    bool query_completed;
    bool query_success;
    std::string error_message;
    
    // 同步控制
    std::mutex result_mutex;
    std::condition_variable result_cv;
    
    // 详细时间统计 🔥 新增
    std::chrono::high_resolution_clock::time_point query_start_time;    // 查询开始
    std::chrono::high_resolution_clock::time_point query_callback_time; // 查询回调
    std::chrono::high_resolution_clock::time_point fetch_start_time;    // 获取开始  
    std::chrono::high_resolution_clock::time_point fetch_end_time;      // 获取结束
    
    // 分阶段耗时
    double query_execution_ms;  // SQL执行耗时
    double result_fetch_ms;     // 结果获取耗时
    double total_ms;           // 总耗时
    
    AsyncQueryContext(const std::string& type, int id, double ra, double dec, double r = 0.0)
        : query_type(type), query_id(id), target_ra(ra), target_dec(dec), radius(r),
          result_count(0), query_completed(false), query_success(false),
          query_execution_ms(0), result_fetch_ms(0), total_ms(0) {
        query_start_time = std::chrono::high_resolution_clock::now();
    }
    
    void markQueryCallback() {
        query_callback_time = std::chrono::high_resolution_clock::now();
        query_execution_ms = std::chrono::duration<double, std::milli>(
            query_callback_time - query_start_time).count();
    }
    
    void markFetchStart() {
        fetch_start_time = std::chrono::high_resolution_clock::now();
    }
    
    void markFetchEnd() {
        fetch_end_time = std::chrono::high_resolution_clock::now();
        result_fetch_ms = std::chrono::duration<double, std::milli>(
            fetch_end_time - fetch_start_time).count();
        total_ms = std::chrono::duration<double, std::milli>(
            fetch_end_time - query_start_time).count();
    }
};

// 全局回调函数声明
void async_query_callback(void* param, TAOS_RES* res, int code);
void async_fetch_callback(void* param, TAOS_RES* res, int numOfRows);

class AsyncTDengineQueryTester {
private:
    TAOS* conn;
    std::string db_name;
    std::string table_name;
    int nside;
    std::unique_ptr<Healpix_Base> healpix_map;
    std::vector<TestData> test_coords_5k;
    std::vector<TestData> test_coords_100;
    
    // 异步查询管理
    std::atomic<int> active_queries{0};
    std::atomic<int> completed_queries{0};
    std::vector<std::unique_ptr<AsyncQueryContext>> query_contexts;
    std::mutex contexts_mutex;
    
    // 时间统计
    std::chrono::high_resolution_clock::time_point connection_start_time;
    std::chrono::high_resolution_clock::time_point connection_end_time;
    
    // 性能统计 🔥 新增
    std::vector<double> query_times;
    std::vector<double> fetch_times;
    std::vector<int> result_counts;
    std::mutex stats_mutex;
    
    // 分类统计
    std::map<std::string, std::vector<double>> query_times_by_type;
    std::map<std::string, std::vector<int>> result_counts_by_type;
    
public:
    AsyncTDengineQueryTester(const std::string& host = "tdengine",
                           const std::string& user = "root", 
                           const std::string& password = "taosdata",
                           int port = 6030,
                           const std::string& database = "healpix_cpp_test",
                           const std::string& table = "sensor_data",
                           int nside_param = 64)
        : conn(nullptr), db_name(database), table_name(table), nside(nside_param) {
        
        std::cout << "🔧 正在初始化 HealPix..." << std::endl;
        // 初始化 HealPix
        healpix_map = std::make_unique<Healpix_Base>(nside, NEST, SET_NSIDE);
        std::cout << "✅ HealPix 初始化成功，NSIDE=" << nside << std::endl;
        
        std::cout << "🔧 正在初始化 TDengine..." << std::endl;
        // 初始化 TDengine
        taos_init();
        
        // 记录连接开始时间
        connection_start_time = std::chrono::high_resolution_clock::now();
        std::cout << "🔗 正在连接数据库 " << database << "..." << std::endl;
        
        // 连接数据库
        conn = taos_connect(host.c_str(), user.c_str(), password.c_str(), database.c_str(), port);
        if (conn == nullptr) {
            throw std::runtime_error("无法连接到 TDengine: " + std::string(taos_errstr(conn)));
        }
        
        // 记录连接结束时间
        connection_end_time = std::chrono::high_resolution_clock::now();
        
        auto connection_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            connection_end_time - connection_start_time);
        
        std::cout << "✅ TDengine 连接成功，耗时: " << connection_duration.count() << " ms" << std::endl;
    }
    
    ~AsyncTDengineQueryTester() {
        std::cout << "🔄 正在清理资源..." << std::endl;
        // 等待所有异步查询完成
        int wait_count = 0;
        while (active_queries > 0 && wait_count < 300) {  // 最多等30秒
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        if (conn) {
            taos_close(conn);
        }
        taos_cleanup();
        std::cout << "✅ 资源清理完成" << std::endl;
    }
    
    bool loadTestData(const std::string& csv_file) {
        std::cout << "🔍 读取大数据文件: " << csv_file << std::endl;
        
        std::ifstream file(csv_file);
        if (!file.is_open()) {
            std::cerr << "❌ 数据文件不存在: " << csv_file << std::endl;
            return false;
        }
        
        std::string line;
        std::getline(file, line); // 跳过头部
        std::cout << "📋 CSV头部: " << line << std::endl;
        
        std::map<int, TestData> unique_sources;
        int line_count = 0;
        int max_lines = 1000000;  // 🔥 读取100万行以获得足够的测试数据
        
        std::cout << "📖 开始读取数据（最多 " << max_lines << " 行）..." << std::endl;
        
        while (std::getline(file, line) && line_count < max_lines) {
            line_count++;
            
            if (line_count % 100000 == 0) {
                std::cout << "   已读取 " << line_count << " 行..." << std::endl;
            }
            
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> fields;
            
            while (std::getline(ss, item, ',')) {
                fields.push_back(item);
            }
            
            if (fields.size() >= 4) {  // 确保有足够的字段
                try {
                    int source_id = std::stoi(fields[1]);
                    double ra = std::stod(fields[2]);
                    double dec = std::stod(fields[3]);
                    
                    if (unique_sources.find(source_id) == unique_sources.end()) {
                        unique_sources[source_id] = {source_id, ra, dec};
                    }
                } catch (const std::exception& e) {
                    // 忽略解析错误的行
                    continue;
                }
            }
        }
        
        std::cout << "✅ 成功读取 " << line_count << " 行，唯一source_id数量：" << unique_sources.size() << std::endl;
        
        if (unique_sources.empty()) {
            std::cerr << "❌ 没有找到有效的数据行" << std::endl;
            return false;
        }
        
        // 准备测试数据
        std::vector<TestData> all_coords;
        for (const auto& pair : unique_sources) {
            all_coords.push_back(pair.second);
        }
        
        // 随机选择测试坐标
        std::random_device rd;
        std::mt19937 g(42); // 固定种子确保可重现性
        std::shuffle(all_coords.begin(), all_coords.end(), g);
        
        int max_test_count = std::min(500, static_cast<int>(all_coords.size()));  // 🔥 测试500个查询
        int test_count_100 = std::min(100, static_cast<int>(all_coords.size()));   
        
        test_coords_5k.assign(all_coords.begin(), all_coords.begin() + max_test_count);
        test_coords_100.assign(all_coords.begin(), all_coords.begin() + test_count_100);
        
        std::cout << "📊 测试规模: 最近邻检索 " << test_coords_5k.size() 
                 << " 个天体，锥形检索 " << test_coords_100.size() << " 个天体" << std::endl;
        
        return true;
    }
    
    void executeAsyncNearestQuery(double ra, double dec, int query_id) {
        // 验证和裁剪坐标值到有效范围
        ra = fmod(ra, 360.0);
        if (ra < 0) ra += 360.0;
        dec = std::max(-90.0, std::min(90.0, dec));
        
        // 创建查询上下文
        auto context = std::make_unique<AsyncQueryContext>("nearest", query_id, ra, dec);
        AsyncQueryContext* ctx_ptr = context.get();
        
        {
            std::lock_guard<std::mutex> lock(contexts_mutex);
            query_contexts.push_back(std::move(context));
        }
        
        // 计算 HealPix 邻居
        pointing pt(deg2rad(90.0 - dec), deg2rad(ra));
        long center_id = healpix_map->ang2pix(pt);
        
        // 构建异步SQL查询
        std::ostringstream oss;
        oss << "SELECT ra, dec FROM " << table_name << " WHERE healpix_id = " << center_id << " LIMIT 1000";
        
        ctx_ptr->sql_query = oss.str();  // 记录SQL
        
        // 执行异步查询 🔥 此处开始计时
        active_queries++;
        taos_query_a(conn, oss.str().c_str(), async_query_callback, ctx_ptr);
    }
    
    void executeAsyncConeQuery(double ra, double dec, double radius, int query_id) {
        // 验证和裁剪坐标值到有效范围
        ra = fmod(ra, 360.0);
        if (ra < 0) ra += 360.0;
        dec = std::max(-90.0, std::min(90.0, dec));
        
        // 创建查询上下文
        auto context = std::make_unique<AsyncQueryContext>("cone_" + std::to_string(radius), query_id, ra, dec, radius);
        AsyncQueryContext* ctx_ptr = context.get();
        
        {
            std::lock_guard<std::mutex> lock(contexts_mutex);
            query_contexts.push_back(std::move(context));
        }
        
        // 🔥 修复HealPix API调用 - 使用query_disc方法
        pointing center_pt(deg2rad(90.0 - dec), deg2rad(ra));
        double radius_rad = deg2rad(radius);
        
        // 使用query_disc获取锥形区域内的像素
        std::vector<int> pixels;
        healpix_map->query_disc(center_pt, radius_rad, pixels);
        
        // 如果没有找到像素，至少包含中心像素
        if (pixels.empty()) {
            int center_id = healpix_map->ang2pix(center_pt);
            pixels.push_back(center_id);
        }
        
        // 构建SQL查询
        std::ostringstream oss;
        oss << "SELECT ra, dec FROM " << table_name << " WHERE healpix_id IN (";
        for (size_t i = 0; i < pixels.size(); ++i) {
            if (i > 0) oss << ",";
            oss << pixels[i];
        }
        oss << ")";
        
        ctx_ptr->sql_query = oss.str();  // 记录SQL
        
        // 执行异步查询
        active_queries++;
        taos_query_a(conn, oss.str().c_str(), async_query_callback, ctx_ptr);
    }
    
    void executeAsyncTimeQuery(double ra, double dec, const std::string& time_condition, int query_id) {
        // 验证和裁剪坐标值到有效范围
        ra = fmod(ra, 360.0);
        if (ra < 0) ra += 360.0;
        dec = std::max(-90.0, std::min(90.0, dec));
        
        // 创建查询上下文
        auto context = std::make_unique<AsyncQueryContext>("time_" + time_condition, query_id, ra, dec);
        AsyncQueryContext* ctx_ptr = context.get();
        
        {
            std::lock_guard<std::mutex> lock(contexts_mutex);
            query_contexts.push_back(std::move(context));
        }
        
        // 计算 HealPix 邻居
        pointing pt(deg2rad(90.0 - dec), deg2rad(ra));
        long center_id = healpix_map->ang2pix(pt);
        
        // 构建时间查询SQL
        std::ostringstream oss;
        oss << "SELECT COUNT(*) FROM " << table_name 
            << " WHERE healpix_id = " << center_id 
            << " AND " << time_condition;
        
        ctx_ptr->sql_query = oss.str();  // 记录SQL
        
        // 执行异步查询
        active_queries++;
        taos_query_a(conn, oss.str().c_str(), async_query_callback, ctx_ptr);
    }
    
    void runAsyncNearestNeighborTest() {
        std::cout << "\n==== 📍 异步最近邻检索：" << test_coords_5k.size() << "个天体 ====" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 重置计数器
        completed_queries = 0;
        
        // 🔥 并发执行多个查询
        int concurrent_queries = 20;  // 同时执行20个查询
        int batch_size = 50;          // 每批50个查询
        int total_batches = (test_coords_5k.size() + batch_size - 1) / batch_size;
        
        std::cout << "📊 测试配置: 并发数=" << concurrent_queries 
                 << ", 批大小=" << batch_size 
                 << ", 总批数=" << total_batches << std::endl;
        
        for (int batch = 0; batch < total_batches; ++batch) {
            int start_idx = batch * batch_size;
            int end_idx = std::min(start_idx + batch_size, static_cast<int>(test_coords_5k.size()));
            
            std::cout << "--- 批次 " << (batch + 1) << "/" << total_batches 
                     << " (查询 " << start_idx << "-" << (end_idx-1) << ") ---" << std::endl;
            
            // 启动一批查询
            for (int i = start_idx; i < end_idx; ++i) {
                // 控制并发数
                while (active_queries >= concurrent_queries) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                
                executeAsyncNearestQuery(test_coords_5k[i].ra, test_coords_5k[i].dec, i);
            }
            
            // 等待当前批次完成
            while (active_queries > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            std::cout << "✅ 批次 " << (batch + 1) << " 完成，总完成: " << completed_queries.load() << std::endl;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "📊 最近邻查询完成: " << completed_queries.load() << "/" << test_coords_5k.size()
                 << ", 耗时: " << (duration.count() / 1000.0) << " 秒" << std::endl;
    }
    
    void runAsyncConeSearchTest() {
        std::cout << "\n==== 🎯 异步锥形检索：不同半径测试 ====" << std::endl;
        
        std::vector<double> radii = {0.01, 0.05, 0.1, 0.5, 1.0};  // 不同半径
        
        for (double radius : radii) {
            std::cout << "\n--- 锥形检索半径 " << radius << "° ---" << std::endl;
            
            auto start_time = std::chrono::high_resolution_clock::now();
            completed_queries = 0;  // 重置计数器
            
            int concurrent_queries = 15;  // 锥形查询更复杂，降低并发数
            
            for (size_t i = 0; i < test_coords_100.size(); ++i) {
                // 控制并发数
                while (active_queries >= concurrent_queries) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                
                executeAsyncConeQuery(test_coords_100[i].ra, test_coords_100[i].dec, radius, i);
                
                if ((i + 1) % 20 == 0) {
                    std::cout << "进度: " << (i + 1) << "/" << test_coords_100.size() << std::endl;
                }
            }
            
            // 等待所有查询完成
            while (active_queries > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            // 统计结果数量
            int total_results = 0;
            std::string cone_type = "cone_" + std::to_string(radius);
            if (result_counts_by_type.find(cone_type) != result_counts_by_type.end()) {
                for (int count : result_counts_by_type[cone_type]) {
                    total_results += count;
                }
            }
            
            std::cout << "✅ 锥形检索（r=" << radius << "°）完成: " 
                     << completed_queries.load() << "/" << test_coords_100.size()
                     << ", 耗时: " << (duration.count() / 1000.0) << " 秒"
                     << ", 总找到: " << total_results << " 个源" << std::endl;
        }
    }
    
    void runAsyncTimeIntervalTest() {
        std::cout << "\n==== ⏰ 异步时间区间查询测试 ====" << std::endl;
        
        // 时间条件
        std::vector<std::pair<std::string, std::string>> time_conditions = {
            {"近一月", "ts >= NOW() - INTERVAL(30, DAY)"},
            {"近一季度", "ts >= NOW() - INTERVAL(90, DAY)"},
            {"近半年", "ts >= NOW() - INTERVAL(180, DAY)"}
        };
        
        for (const auto& condition : time_conditions) {
            std::cout << "\n--- " << condition.first << " ---" << std::endl;
            
            auto start_time = std::chrono::high_resolution_clock::now();
            completed_queries = 0;  // 重置计数器
            
            int concurrent_queries = 25;  // 时间查询较快，可以更高并发
            
            for (size_t i = 0; i < test_coords_5k.size(); ++i) {
                // 控制并发数
                while (active_queries >= concurrent_queries) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                
                executeAsyncTimeQuery(test_coords_5k[i].ra, test_coords_5k[i].dec, condition.second, i);
                
                if ((i + 1) % 100 == 0) {
                    std::cout << "进度: " << (i + 1) << "/" << test_coords_5k.size() << std::endl;
                }
            }
            
            // 等待所有查询完成
            while (active_queries > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            std::cout << "✅ " << condition.first << " 完成: " 
                     << completed_queries.load() << "/" << test_coords_5k.size()
                     << ", 耗时: " << (duration.count() / 1000.0) << " 秒"
                     << ", 平均: " << (duration.count() / (double)test_coords_5k.size()) << " ms/查询" << std::endl;
        }
    }
    
    void analyzePerformanceStats(long long total_duration_ms) {
        std::lock_guard<std::mutex> lock(stats_mutex);
        
        std::cout << "\n📊 ===== 详细性能分析 =====" << std::endl;
        std::cout << "🔗 数据库初始连接时间: " 
                 << std::chrono::duration_cast<std::chrono::milliseconds>(
                    connection_end_time - connection_start_time).count() << " ms" << std::endl;
        
        // 按查询类型分析
        for (const auto& entry : query_times_by_type) {
            const std::string& type = entry.first;
            const std::vector<double>& times = entry.second;
            
            if (times.empty()) continue;
            
            double sum = 0, min_time = times[0], max_time = times[0];
            for (double t : times) {
                sum += t;
                min_time = std::min(min_time, t);
                max_time = std::max(max_time, t);
            }
            double avg = sum / times.size();
            
            std::cout << "\n📈 " << type << " 查询统计:" << std::endl;
            std::cout << "   - 查询数量: " << times.size() << std::endl;
            std::cout << "   - 平均SQL执行时间: " << std::fixed << std::setprecision(2) << avg << " ms" << std::endl;
            std::cout << "   - 最快执行: " << min_time << " ms" << std::endl;
            std::cout << "   - 最慢执行: " << max_time << " ms" << std::endl;
            
            // 结果数量统计
            if (result_counts_by_type.find(type) != result_counts_by_type.end()) {
                const auto& counts = result_counts_by_type[type];
                if (!counts.empty()) {
                    int sum_results = 0, min_results = counts[0], max_results = counts[0];
                    for (int c : counts) {
                        sum_results += c;
                        min_results = std::min(min_results, c);
                        max_results = std::max(max_results, c);
                    }
                    double avg_results = (double)sum_results / counts.size();
                    
                    std::cout << "   - 平均结果数量: " << std::fixed << std::setprecision(1) << avg_results << " 条" << std::endl;
                    std::cout << "   - 总记录数: " << sum_results << " 条" << std::endl;
                }
            }
        }
        
        std::cout << "\n⏱️ 总体性能:" << std::endl;
        std::cout << "   - 总查询数: " << query_times.size() << std::endl;
        std::cout << "   - 平均吞吐量: " << std::fixed << std::setprecision(1) 
                 << (query_times.size() > 0 ? query_times.size() * 1000.0 / total_duration_ms : 0) << " 查询/秒" << std::endl;
    }
    
    void generateReport() {
        std::cout << "\n📄 ===== 完整性能测试报告 =====" << std::endl;
        analyzePerformanceStats(0);  // 传入0因为我们分别统计了各个阶段
    }
    
    // 友元函数，用于回调
    friend void async_query_callback(void* param, TAOS_RES* res, int code);
    friend void async_fetch_callback(void* param, TAOS_RES* res, int numOfRows);
    
    // 添加统计数据的方法
    void addPerformanceStats(const std::string& query_type, double query_time, double fetch_time, int result_count) {
        std::lock_guard<std::mutex> lock(stats_mutex);
        query_times.push_back(query_time);
        fetch_times.push_back(fetch_time);
        result_counts.push_back(result_count);
        
        // 按类型分类统计
        query_times_by_type[query_type].push_back(query_time);
        result_counts_by_type[query_type].push_back(result_count);
    }
};

// 全局实例指针，用于回调函数访问
static AsyncTDengineQueryTester* g_tester = nullptr;

// 异步查询回调函数 🔥 增强时间记录
void async_query_callback(void* param, TAOS_RES* res, int code) {
    AsyncQueryContext* context = static_cast<AsyncQueryContext*>(param);
    context->markQueryCallback();  // 🔥 记录查询回调时间
    
    if (code != 0) {
        // 查询失败
        std::lock_guard<std::mutex> lock(context->result_mutex);
        context->query_success = false;
        context->error_message = taos_errstr(res);
        context->query_completed = true;
        context->markFetchEnd();
        
        if (g_tester) {
            g_tester->active_queries--;
            g_tester->completed_queries++;
        }
        
        context->result_cv.notify_one();
        return;
    }
    
    // 查询成功，开始获取结果
    context->markFetchStart();  // 🔥 记录获取开始时间
    
    if (res == nullptr) {
        // 没有结果集
        std::lock_guard<std::mutex> lock(context->result_mutex);
        context->query_success = true;
        context->result_count = 0;
        context->query_completed = true;
        context->markFetchEnd();
        
        if (g_tester) {
            g_tester->active_queries--;
            g_tester->completed_queries++;
            g_tester->addPerformanceStats(context->query_type, context->query_execution_ms, context->result_fetch_ms, 0);
        }
        
        context->result_cv.notify_one();
        return;
    }
    
    // 异步获取结果
    taos_fetch_rows_a(res, async_fetch_callback, param);
}

// 异步获取结果回调函数 🔥 增强时间记录
void async_fetch_callback(void* param, TAOS_RES* res, int numOfRows) {
    AsyncQueryContext* context = static_cast<AsyncQueryContext*>(param);
    
    if (numOfRows < 0) {
        // 获取结果出错
        std::lock_guard<std::mutex> lock(context->result_mutex);
        context->query_success = false;
        context->error_message = taos_errstr(res);
        context->query_completed = true;
        context->markFetchEnd();
        
        if (g_tester) {
            g_tester->active_queries--;
            g_tester->completed_queries++;
        }
        
        context->result_cv.notify_one();
        return;
    }
    
    if (numOfRows == 0) {
        // 所有结果获取完毕 🔥 记录完成时间和统计
        std::lock_guard<std::mutex> lock(context->result_mutex);
        
        context->query_success = true;
        context->query_completed = true;
        context->markFetchEnd();
        
        if (g_tester) {
            g_tester->active_queries--;
            g_tester->completed_queries++;
            g_tester->addPerformanceStats(
                context->query_type,
                context->query_execution_ms, 
                context->result_fetch_ms, 
                context->result_count
            );
        }
        
        context->result_cv.notify_one();
        return;
    }
    
    // 处理当前批次的结果
    context->result_count += numOfRows;  // 累加结果数量
    
    // 继续获取下一批结果
    taos_fetch_rows_a(res, async_fetch_callback, param);
}

int main() {
    try {
        std::cout << "🌟 TDengine HealPix Async 容器化异步查询测试器" << std::endl;
        std::cout << "=================================================" << std::endl;
        
        AsyncTDengineQueryTester tester;
        g_tester = &tester;  // 设置全局指针供回调函数使用
        
        // 🔥 加载大数据文件
        if (!tester.loadTestData("/app/data/test_data_100M.csv")) {
            std::cerr << "❌ 请确认一亿数据文件存在: /app/data/test_data_100M.csv" << std::endl;
            return 1;
        }
        
        // 🔥 运行完整的异步性能测试套件
        tester.runAsyncNearestNeighborTest();
        tester.runAsyncConeSearchTest();
        tester.runAsyncTimeIntervalTest();
        
        // 生成完整测试报告
        tester.generateReport();
        
        std::cout << "\n🎉 ==== 一亿数据完整异步性能测试完成 ====" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
}