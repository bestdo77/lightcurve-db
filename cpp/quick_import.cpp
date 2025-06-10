#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <filesystem>
#include <map>
#include <algorithm>
#include <memory>
#include <iomanip>
#include <cstring>

// TDengine 头文件
#include <taos.h>

// HealPix C++ 库头文件
#include <healpix_cxx/healpix_base.h>
#include <healpix_cxx/pointing.h>

const double PI = 3.14159265358979323846;

// 度数转弧度函数
inline double deg2rad(double deg) {
    return deg * PI / 180.0;
}

struct AstronomicalRecord {
    std::string timestamp;
    int source_id;
    double ra;
    double dec;
    double mag;
    double jd_tcb;
    long healpix_id;
};

class TDengineHealpixImporter {
private:
    TAOS* conn;
    std::string db_name;
    std::string table_name;
    int nside_base;
    int nside_fine;
    int count_threshold;
    int batch_size;
    std::unique_ptr<Healpix_Base> healpix_base;
    std::unique_ptr<Healpix_Base> healpix_fine;

public:
    TDengineHealpixImporter(const std::string& database,
                           const std::string& host = "localhost",
                           const std::string& user = "root",
                           const std::string& password = "taosdata",
                           int port = 6030,
                           int nside_base_param = 64,
                           int nside_fine_param = 256,
                           int count_threshold_param = 10000,
                           int batch_size_param = 500)
        : conn(nullptr), db_name(database), table_name("sensor_data"),
          nside_base(nside_base_param), nside_fine(nside_fine_param),
          count_threshold(count_threshold_param), batch_size(batch_size_param) {
        
        // 初始化 HealPix
        healpix_base = std::make_unique<Healpix_Base>(nside_base, NEST, SET_NSIDE);
        healpix_fine = std::make_unique<Healpix_Base>(nside_fine, NEST, SET_NSIDE);
        std::cout << "✅ HealPix 初始化成功，基础NSIDE=" << nside_base 
                 << "，细分NSIDE=" << nside_fine << std::endl;
        
        // 初始化 TDengine
        taos_init();
        
        // 连接数据库
        conn = taos_connect(host.c_str(), user.c_str(), password.c_str(), nullptr, port);
        if (conn == nullptr) {
            throw std::runtime_error("无法连接到 TDengine: " + std::string(taos_errstr(conn)));
        }
        std::cout << "✅ TDengine 连接成功" << std::endl;
    }
    
    ~TDengineHealpixImporter() {
        if (conn) {
            taos_close(conn);
        }
        taos_cleanup();
    }
    
    bool dropDatabase() {
        std::cout << "⚠️ 正在删除数据库: " << db_name << std::endl;
        
        std::string sql = "DROP DATABASE IF EXISTS " + db_name;
        TAOS_RES* result = taos_query(conn, sql.c_str());
        
        if (taos_errno(result) != 0) {
            std::cerr << "❌ 删除数据库失败: " << taos_errstr(result) << std::endl;
            taos_free_result(result);
            return false;
        }
        
        taos_free_result(result);
        std::cout << "✅ 数据库 " << db_name << " 已删除" << std::endl;
        return true;
    }
    
    bool createSuperTable() {
        std::cout << "🏗️ 创建数据库和超级表..." << std::endl;
        
        // 创建数据库
        std::string create_db_sql = "CREATE DATABASE IF NOT EXISTS " + db_name;
        TAOS_RES* result = taos_query(conn, create_db_sql.c_str());
        if (taos_errno(result) != 0) {
            std::cerr << "❌ 创建数据库失败: " << taos_errstr(result) << std::endl;
            taos_free_result(result);
            return false;
        }
        taos_free_result(result);
        
        // 使用数据库
        std::string use_db_sql = "USE " + db_name;
        result = taos_query(conn, use_db_sql.c_str());
        if (taos_errno(result) != 0) {
            std::cerr << "❌ 使用数据库失败: " << taos_errstr(result) << std::endl;
            taos_free_result(result);
            return false;
        }
        taos_free_result(result);
        
        // 创建超级表
        std::string create_table_sql = 
            "CREATE STABLE IF NOT EXISTS " + table_name + " ("
            "ts TIMESTAMP, "
            "ra DOUBLE, "
            "dec DOUBLE, "
            "mag DOUBLE, "
            "jd_tcb DOUBLE"
            ") TAGS (healpix_id BIGINT, source_id BIGINT)";
        
        result = taos_query(conn, create_table_sql.c_str());
        if (taos_errno(result) != 0) {
            std::cerr << "❌ 创建超级表失败: " << taos_errstr(result) << std::endl;
            taos_free_result(result);
            return false;
        }
        taos_free_result(result);
        
        std::cout << "✅ 超级表 " << table_name << " 已创建" << std::endl;
        return true;
    }
    
    long calculateAdaptiveHealpixId(double ra, double dec, int source_id, 
                                   const std::map<long, int>& base_counts) {
        // 验证和裁剪坐标值到有效范围
        ra = fmod(ra, 360.0);
        if (ra < 0) ra += 360.0;  // 确保 RA 在 [0, 360) 范围内
        
        dec = std::max(-90.0, std::min(90.0, dec));  // 裁剪 DEC 到 [-90, 90] 范围
        
        // 计算基础分辨率的 healpix ID
        pointing pt(deg2rad(90.0 - dec), deg2rad(ra));
        long base_id = healpix_base->ang2pix(pt);
        
        // 检查是否需要细分
        auto it = base_counts.find(base_id);
        int count = (it != base_counts.end()) ? it->second : 0;
        
        if (count > count_threshold) {
            // 需要细分，使用细分分辨率
            long fine_id = healpix_fine->ang2pix(pt);
            return (base_id << 32) + fine_id;  // 组合ID
        } else {
            return base_id;
        }
    }
    
    std::vector<AstronomicalRecord> loadAndProcessData(const std::string& csv_file) {
        std::cout << "📖 读取和处理数据文件: " << csv_file << std::endl;
        
        std::ifstream file(csv_file);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开数据文件: " + csv_file);
        }
        
        std::vector<AstronomicalRecord> records;
        std::string line;
        
        // 跳过头部
        std::getline(file, line);
        
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string item;
            std::vector<std::string> fields;
            
            while (std::getline(ss, item, ',')) {
                fields.push_back(item);
            }
            
            if (fields.size() >= 6) {
                AstronomicalRecord record;
                record.timestamp = fields[0];
                record.source_id = std::stoi(fields[1]);
                record.ra = std::stod(fields[2]);
                record.dec = std::stod(fields[3]);
                record.mag = std::stod(fields[4]);
                record.jd_tcb = std::stod(fields[5]);
                
                records.push_back(record);
            }
        }
        
        std::cout << "✅ 成功读取 " << records.size() << " 条记录" << std::endl;
        
        // 统计每个基础healpix区块的天体数量
        std::cout << "🔧 开始自适应 healpix 分区计算..." << std::endl;
        std::map<long, int> base_counts;
        
        for (const auto& record : records) {
            // 验证和裁剪坐标值
            double clean_ra = fmod(record.ra, 360.0);
            if (clean_ra < 0) clean_ra += 360.0;
            double clean_dec = std::max(-90.0, std::min(90.0, record.dec));
            
            pointing pt(deg2rad(90.0 - clean_dec), deg2rad(clean_ra));
            long base_id = healpix_base->ang2pix(pt);
            base_counts[base_id]++;
        }
        
        std::cout << "📊 基础分区统计:" << std::endl;
        std::cout << "   - 总区块数: " << base_counts.size() << std::endl;
        
        // 计算平均值和最大值
        long total_count = 0;
        int max_count = 0;
        for (const auto& pair : base_counts) {
            total_count += pair.second;
            max_count = std::max(max_count, pair.second);
        }
        double avg_count = static_cast<double>(total_count) / base_counts.size();
        std::cout << "   - 平均天体/区块: " << std::fixed << std::setprecision(1) << avg_count << std::endl;
        std::cout << "   - 最大天体/区块: " << max_count << std::endl;
        
        // 统计需要细分的区块
        int large_blocks = 0;
        for (const auto& pair : base_counts) {
            if (pair.second > count_threshold) {
                large_blocks++;
            }
        }
        std::cout << "⚡ 需要细分的区块: " << large_blocks << " 个" << std::endl;
        
        // 为每条记录分配healpix_id
        for (auto& record : records) {
            record.healpix_id = calculateAdaptiveHealpixId(record.ra, record.dec, record.source_id, base_counts);
        }
        
        // 生成映射表
        std::map<int, long> source_healpix_map;
        for (const auto& record : records) {
            if (source_healpix_map.find(record.source_id) == source_healpix_map.end()) {
                source_healpix_map[record.source_id] = record.healpix_id;
            }
        }
        
        // 保存映射表
        std::filesystem::create_directories("output/query_results");
        std::ofstream map_file("output/query_results/sourceid_healpix_map.csv");
        std::ofstream map_file_root("sourceid_healpix_map.csv");
        
        if (map_file.is_open() && map_file_root.is_open()) {
            map_file << "source_id,healpix_id\n";
            map_file_root << "source_id,healpix_id\n";
            
            for (const auto& pair : source_healpix_map) {
                map_file << pair.first << "," << pair.second << "\n";
                map_file_root << pair.first << "," << pair.second << "\n";
            }
            
            map_file.close();
            map_file_root.close();
            std::cout << "💾 已保存映射表，共 " << source_healpix_map.size() << " 条记录" << std::endl;
        }
        
        return records;
    }
    
    bool importData(const std::vector<AstronomicalRecord>& records) {
        std::cout << "\n🚀 开始导入数据到超级表..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 按 (healpix_id, source_id) 分组
        std::map<std::pair<long, int>, std::vector<const AstronomicalRecord*>> groups;
        
        for (const auto& record : records) {
            groups[{record.healpix_id, record.source_id}].push_back(&record);
        }
        
        std::cout << "📊 导入统计预览:" << std::endl;
        std::cout << "   - 总记录数: " << records.size() << std::endl;
        std::cout << "   - 子表数量: " << groups.size() << std::endl;
        std::cout << "   - 批处理大小: " << batch_size << std::endl;
        
        int total_success = 0;
        int total_error = 0;
        int processed_groups = 0;
        
        for (const auto& group : groups) {
            processed_groups++;
            long healpix_id = group.first.first;
            int source_id = group.first.second;
            const auto& group_records = group.second;
            
            // 创建子表
            std::string table_name_full = table_name + "_" + std::to_string(healpix_id) + "_" + std::to_string(source_id);
            std::string create_sql = "CREATE TABLE IF NOT EXISTS " + table_name_full + 
                                   " USING " + table_name + " TAGS (" + std::to_string(healpix_id) + 
                                   ", " + std::to_string(source_id) + ")";
            
            TAOS_RES* result = taos_query(conn, create_sql.c_str());
            if (taos_errno(result) != 0) {
                std::cerr << "❌ 创建子表失败: " << taos_errstr(result) << std::endl;
                taos_free_result(result);
                continue;
            }
            taos_free_result(result);
            
            // 批量插入数据
            for (size_t i = 0; i < group_records.size(); i += batch_size) {
                size_t end_idx = std::min(i + batch_size, group_records.size());
                
                std::ostringstream insert_sql;
                insert_sql << "INSERT INTO " << table_name_full << " VALUES ";
                
                for (size_t j = i; j < end_idx; ++j) {
                    if (j > i) insert_sql << ",";
                    const auto& record = *group_records[j];
                    insert_sql << "('" << record.timestamp << "'," 
                              << std::fixed << std::setprecision(6) << record.ra << ","
                              << std::fixed << std::setprecision(6) << record.dec << ","
                              << std::fixed << std::setprecision(2) << record.mag << ","
                              << std::fixed << std::setprecision(6) << record.jd_tcb << ")";
                }
                
                result = taos_query(conn, insert_sql.str().c_str());
                if (taos_errno(result) == 0) {
                    total_success += (end_idx - i);
                } else {
                    total_error += (end_idx - i);
                    // 可以选择记录错误详情
                }
                taos_free_result(result);
            }
            
            // 显示进度
            if (processed_groups % 100 == 0 || processed_groups == groups.size()) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);
                double rate = total_success / (elapsed.count() > 0 ? elapsed.count() : 1);
                
                std::cout << "📈 进度: " << processed_groups << "/" << groups.size() << " 分组" << std::endl;
                std::cout << "   - 成功: " << total_success << std::endl;
                std::cout << "   - 失败: " << total_error << std::endl;
                std::cout << "   - 速度: " << static_cast<int>(rate) << " 行/秒" << std::endl;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        // 生成导入报告
        generateImportReport(records.size(), total_success, total_error, duration.count(), groups.size());
        
        std::cout << "\n🎉 导入完成！" << std::endl;
        std::cout << "✅ 成功导入: " << total_success << " 条" << std::endl;
        std::cout << "❌ 失败: " << total_error << " 条" << std::endl;
        std::cout << "📊 成功率: " << std::fixed << std::setprecision(2) 
                 << (total_success * 100.0 / (total_success + total_error)) << "%" << std::endl;
        std::cout << "⏱️ 总耗时: " << duration.count() << " 秒" << std::endl;
        std::cout << "📁 子表数量: " << groups.size() << std::endl;
        
        return total_success > 0;
    }
    
private:
    void generateImportReport(int total_records, int success_count, int error_count, 
                            int duration_seconds, int table_count) {
        std::filesystem::create_directories("output/logs");
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp_ss;
        timestamp_ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        
        std::string report_file = "output/logs/import_report_" + timestamp_ss.str() + ".txt";
        std::ofstream report(report_file);
        
        if (report.is_open()) {
            report << "================================================================================\n";
            report << "🌟 TDengine HealPix 数据导入报告 (C++ 版本)\n";
            report << "================================================================================\n";
            
            std::stringstream current_time_ss;
            current_time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            
            report << "导入时间: " << current_time_ss.str() << "\n";
            report << "目标数据库: " << db_name << "\n";
            report << "基础NSIDE: " << nside_base << "\n";
            report << "细分NSIDE: " << nside_fine << "\n";
            report << "细分阈值: " << count_threshold << "\n";
            report << "批处理大小: " << batch_size << "\n\n";
            
            report << "📊 导入统计:\n";
            report << "  - 总记录数: " << total_records << "\n";
            report << "  - 成功导入: " << success_count << "\n";
            report << "  - 失败记录: " << error_count << "\n";
            double success_rate = (success_count * 100.0) / (success_count + error_count);
            report << "  - 成功率: " << std::fixed << std::setprecision(2) << success_rate << "%\n";
            report << "  - 总耗时: " << duration_seconds << " 秒\n";
            if (duration_seconds > 0) {
                report << "  - 导入速度: " << (success_count / duration_seconds) << " 行/秒\n";
            }
            
            report << "\n🏗️ 表结构统计:\n";
            report << "  - 子表数量: " << table_count << "\n";
            
            report.close();
            std::cout << "📄 导入报告已保存到: " << report_file << std::endl;
        }
    }
};

void printUsage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  --input <文件>            输入CSV文件路径\n";
    std::cout << "  --db <数据库名>           TDengine数据库名\n";
    std::cout << "  --nside_base <值>         基础healpix分辨率 (默认: 64)\n";
    std::cout << "  --nside_fine <值>         细分healpix分辨率 (默认: 256)\n";
    std::cout << "  --count_threshold <值>    细分阈值 (默认: 10000)\n";
    std::cout << "  --batch_size <值>         批处理大小 (默认: 500)\n";
    std::cout << "  --host <主机>             TDengine主机 (默认: localhost)\n";
    std::cout << "  --user <用户>             用户名 (默认: root)\n";
    std::cout << "  --password <密码>         密码 (默认: taosdata)\n";
    std::cout << "  --port <端口>             端口 (默认: 6030)\n";
    std::cout << "  --drop_db                 导入前删除数据库\n";
    std::cout << "  --help                    显示此帮助信息\n\n";
    std::cout << "示例:\n";
    std::cout << "  " << program_name << " --input data.csv --db sensor_db_healpix\n";
    std::cout << "  " << program_name << " --input data.csv --db test_db --nside_base 128 --drop_db\n";
}

int main(int argc, char* argv[]) {
    std::string input_file;
    std::string db_name;
    std::string host = "localhost";
    std::string user = "root";
    std::string password = "taosdata";
    int port = 6030;
    int nside_base = 64;
    int nside_fine = 256;
    int count_threshold = 10000;
    int batch_size = 500;
    bool drop_db = false;
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input_file = argv[++i];
        } else if (std::strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_name = argv[++i];
        } else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (std::strcmp(argv[i], "--user") == 0 && i + 1 < argc) {
            user = argv[++i];
        } else if (std::strcmp(argv[i], "--password") == 0 && i + 1 < argc) {
            password = argv[++i];
        } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--nside_base") == 0 && i + 1 < argc) {
            nside_base = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--nside_fine") == 0 && i + 1 < argc) {
            nside_fine = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--count_threshold") == 0 && i + 1 < argc) {
            count_threshold = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--batch_size") == 0 && i + 1 < argc) {
            batch_size = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--drop_db") == 0) {
            drop_db = true;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "未知参数: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // 检查必需参数
    if (input_file.empty() || db_name.empty()) {
        std::cerr << "❌ 缺少必需参数 --input 和 --db" << std::endl;
        printUsage(argv[0]);
        return 1;
    }
    
    // 检查输入文件
    if (!std::filesystem::exists(input_file)) {
        std::cerr << "❌ 输入文件不存在: " << input_file << std::endl;
        return 1;
    }
    
    try {
        std::cout << "🌟 TDengine Healpix 空间分析数据导入器 (C++ 版本)" << std::endl;
        std::cout << "============================================================" << std::endl;
        
        double file_size_mb = std::filesystem::file_size(input_file) / (1024.0 * 1024.0);
        std::cout << "📁 输入文件: " << input_file << " (" << std::fixed 
                 << std::setprecision(1) << file_size_mb << " MB)" << std::endl;
        std::cout << "🎯 目标数据库: " << db_name << std::endl;
        std::cout << "🏠 TDengine主机: " << host << ":" << port << std::endl;
        
        TDengineHealpixImporter importer(db_name, host, user, password, port,
                                        nside_base, nside_fine, count_threshold, batch_size);
        
        // 删除数据库（如果指定）
        if (drop_db) {
            if (!importer.dropDatabase()) {
                std::cerr << "❌ 删除数据库失败，停止执行" << std::endl;
                return 1;
            }
        }
        
        // 创建超级表
        if (!importer.createSuperTable()) {
            std::cerr << "❌ 创建超级表失败，停止执行" << std::endl;
            return 1;
        }
        
        // 加载和处理数据
        auto records = importer.loadAndProcessData(input_file);
        
        // 导入数据
        bool success = importer.importData(records);
        
        if (success) {
            std::cout << "\n🎊 数据导入成功完成！" << std::endl;
            std::cout << "💡 下一步：运行查询测试来验证性能" << std::endl;
            return 0;
        } else {
            std::cout << "\n💥 数据导入失败！" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
} 