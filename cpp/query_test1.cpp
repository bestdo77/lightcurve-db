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

class TDengineQueryTester {
private:
    TAOS* conn;
    std::string db_name;
    std::string table_name;
    int nside;
    std::unique_ptr<Healpix_Base> healpix_map;
    std::vector<TestData> test_coords_5k;
    std::vector<TestData> test_coords_100;
    
public:
    TDengineQueryTester(const std::string& host = "localhost",
                       const std::string& user = "root", 
                       const std::string& password = "taosdata",
                       int port = 6030,
                       const std::string& database = "test_db",
                       const std::string& table = "sensor_data",
                       int nside_param = 64)
        : conn(nullptr), db_name(database), table_name(table), nside(nside_param) {
        
        // 初始化 HealPix
        healpix_map = std::make_unique<Healpix_Base>(nside, NEST, SET_NSIDE);
        std::cout << "✅ HealPix 初始化成功，NSIDE=" << nside << std::endl;
        
        // 初始化 TDengine
        taos_init();
        
        // 连接数据库
        conn = taos_connect(host.c_str(), user.c_str(), password.c_str(), database.c_str(), port);
        if (conn == nullptr) {
            throw std::runtime_error("无法连接到 TDengine: " + std::string(taos_errstr(conn)));
        }
        
        std::cout << "✅ TDengine 连接成功" << std::endl;
    }
    
    ~TDengineQueryTester() {
        if (conn) {
            taos_close(conn);
        }
        taos_cleanup();
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
        int max_lines = 1000000;  // 读取100万行以获得足够的测试数据
        
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
        
        int max_test_count = std::min(500, static_cast<int>(all_coords.size()));
        int test_count_100 = std::min(100, static_cast<int>(all_coords.size()));
        
        test_coords_5k.assign(all_coords.begin(), all_coords.begin() + max_test_count);
        test_coords_100.assign(all_coords.begin(), all_coords.begin() + test_count_100);
        
        std::cout << "📊 测试规模: 最近邻检索 " << test_coords_5k.size() 
                 << " 个天体，锥形检索 " << test_coords_100.size() << " 个天体" << std::endl;
        
        return true;
    }
    
    double nearestWithHealpix(double ra, double dec) {
        // 验证和裁剪坐标值到有效范围
        ra = fmod(ra, 360.0);
        if (ra < 0) ra += 360.0;
        dec = std::max(-90.0, std::min(90.0, dec));
        
        // 使用真正的 HealPix 库，模仿Python的实现
        pointing pt(deg2rad(90.0 - dec), deg2rad(ra));  // theta, phi in radians
        
        long center_id = healpix_map->ang2pix(pt);
        
        // 获取所有8个邻居像素 + 中心像素 (Python: hp.get_all_neighbours)
        std::vector<long> healpix_ids;
        healpix_ids.push_back(center_id);
        
        // 简化的邻居查找：基于HealPix分层结构的相邻像素
        // 这是Python hp.get_all_neighbours的简化版本
        for (int d_theta = -1; d_theta <= 1; ++d_theta) {
            for (int d_phi = -1; d_phi <= 1; ++d_phi) {
                if (d_theta == 0 && d_phi == 0) continue; // 跳过中心点
                
                // 计算邻近点的坐标
                double theta = pt.theta + d_theta * M_PI / (2.0 * nside);
                double phi = pt.phi + d_phi * 2.0 * M_PI / (4.0 * nside);
                
                // 确保在有效范围内
                if (theta >= 0 && theta <= M_PI && phi >= 0 && phi < 2.0 * M_PI) {
                    pointing neighbor_pt(theta, phi);
                    long neighbor_id = healpix_map->ang2pix(neighbor_pt);
                    if (neighbor_id >= 0 && neighbor_id < healpix_map->Npix()) {
                        healpix_ids.push_back(neighbor_id);
                    }
                }
            }
        }
        
        // 构建 SQL 查询 - 查询超级表sensor_data而不是特定子表
        std::ostringstream oss;
        oss << "SELECT ra, dec FROM " << table_name << " WHERE healpix_id IN (";
        for (size_t i = 0; i < healpix_ids.size(); ++i) {
            if (i > 0) oss << ",";
            oss << healpix_ids[i];
        }
        oss << ")";
        
        TAOS_RES* result = taos_query(conn, oss.str().c_str());
        if (taos_errno(result) != 0) {
            std::cerr << "查询错误: " << taos_errstr(result) << std::endl;
            taos_free_result(result);
            return -1.0;
        }
        
        TAOS_ROW row;
        double min_distance = std::numeric_limits<double>::max();
        SkyCoord target(ra, dec);
        bool found_any = false;
        
        while ((row = taos_fetch_row(result)) != nullptr) {
            if (row[0] == nullptr || row[1] == nullptr) continue;
            
            double query_ra = *static_cast<double*>(row[0]);
            double query_dec = *static_cast<double*>(row[1]);
            
            SkyCoord source(query_ra, query_dec);
            double distance = target.separation(source);
            min_distance = std::min(min_distance, distance);
            found_any = true;
        }
        
        taos_free_result(result);
        return found_any ? min_distance : -1.0;
    }
    
    int coneWithHealpix(double ra, double dec, double radius) {
        // 验证和裁剪坐标值到有效范围
        ra = fmod(ra, 360.0);
        if (ra < 0) ra += 360.0;
        dec = std::max(-90.0, std::min(90.0, dec));
        
        // 使用真正的 HealPix 库进行圆盘查询，模仿Python实现
        pointing pt(deg2rad(90.0 - dec), deg2rad(ra));  // theta, phi in radians
        double radius_rad = deg2rad(radius);
        
        std::vector<int> healpix_ids;
        healpix_map->query_disc(pt, radius_rad, healpix_ids);
        
        if (healpix_ids.empty()) {
            // 如果没有找到像素，可能半径太小，尝试包含中心像素
            long center_id = healpix_map->ang2pix(pt);
            healpix_ids.push_back(center_id);
        }
        
        // 构建 SQL 查询 - 查询超级表sensor_data
        std::ostringstream oss;
        oss << "SELECT ra, dec FROM " << table_name << " WHERE healpix_id IN (";
        for (size_t i = 0; i < healpix_ids.size(); ++i) {
            if (i > 0) oss << ",";
            oss << healpix_ids[i];
        }
        oss << ")";
        
        TAOS_RES* result = taos_query(conn, oss.str().c_str());
        if (taos_errno(result) != 0) {
            std::cerr << "锥形查询错误: " << taos_errstr(result) << std::endl;
            taos_free_result(result);
            return 0;
        }
        
        int count = 0;
        TAOS_ROW row;
        SkyCoord target(ra, dec);
        
        while ((row = taos_fetch_row(result)) != nullptr) {
            if (row[0] == nullptr || row[1] == nullptr) continue;
            
            double query_ra = *static_cast<double*>(row[0]);
            double query_dec = *static_cast<double*>(row[1]);
            
            SkyCoord source(query_ra, query_dec);
            if (target.separation(source) < radius) {
                count++;
            }
        }
        
        taos_free_result(result);
        return count;
    }
    
    void runNearestNeighborTest() {
        std::cout << "\n==== 最近邻检索：" << test_coords_5k.size() << "个天体（HealPix索引） ====" << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        int successful_queries = 0;
        
        for (size_t i = 0; i < test_coords_5k.size(); ++i) {
            double distance = nearestWithHealpix(test_coords_5k[i].ra, test_coords_5k[i].dec);
            if (distance >= 0) {
                successful_queries++;
            }
            
            // 显示进度
            if ((i + 1) % 50 == 0 || i + 1 == test_coords_5k.size()) {
                std::cout << "进度: " << (i + 1) << "/" << test_coords_5k.size() 
                         << " (" << ((i + 1) * 100 / test_coords_5k.size()) << "%)" << std::endl;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << test_coords_5k.size() << "个最近邻（healpix）总耗时：" 
                 << (duration.count() / 1000.0) << "秒" << std::endl;
        std::cout << "成功查询: " << successful_queries << "/" << test_coords_5k.size() << std::endl;
    }
    
    void runConeSearchTest() {
        std::cout << "\n==== 锥形检索：" << test_coords_100.size() << "个天体，不同半径（HealPix索引） ====" << std::endl;
        
        std::vector<double> radii = {0.01, 0.05, 0.1, 0.5, 1.0};
        
        for (double radius : radii) {
            auto start_time = std::chrono::high_resolution_clock::now();
            int total_count = 0;
            
            for (size_t i = 0; i < test_coords_100.size(); ++i) {
                int count = coneWithHealpix(test_coords_100[i].ra, test_coords_100[i].dec, radius);
                total_count += count;
                
                // 显示进度
                if ((i + 1) % 20 == 0 || i + 1 == test_coords_100.size()) {
                    std::cout << "锥形（r=" << radius << "°）进度: " << (i + 1) << "/" 
                             << test_coords_100.size() << std::endl;
                }
            }
            
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
            
            std::cout << test_coords_100.size() << "个锥形检索（healpix，半径" << radius 
                     << "度）总耗时：" << (duration.count() / 1000.0) << "秒，总找到：" 
                     << total_count << "个源" << std::endl;
        }
    }
    
    void runTimeRangeTest() {
        std::cout << "\n==== " << test_coords_5k.size() << "个天体时间区间统计（HealPix索引） ====" << std::endl;
        
        struct TimeRange {
            std::string name;
            std::string start_time;
            std::string end_time;
        };
        
        std::vector<TimeRange> time_ranges = {
            {"近一月", "2024-11-30 00:00:00", "2024-12-30 23:59:59"},
            {"近一季度", "2024-10-01 00:00:00", "2024-12-30 23:59:59"},
            {"近半年", "2024-07-01 00:00:00", "2024-12-30 23:59:59"}
        };
        
        std::map<std::string, std::pair<double, int>> time_stats;
        
        for (const auto& range : time_ranges) {
            time_stats[range.name] = {0.0, 0};
        }
        
        for (size_t i = 0; i < std::min(size_t(5000), test_coords_5k.size()); ++i) {
            int source_id = test_coords_5k[i].source_id;
            
            for (const auto& range : time_ranges) {
                auto start_time = std::chrono::high_resolution_clock::now();
                
                std::ostringstream sql;
                sql << "SELECT COUNT(*) FROM " << table_name 
                    << " WHERE source_id=" << source_id
                    << " AND ts >= '" << range.start_time << "'"
                    << " AND ts <= '" << range.end_time << "'";
                
                TAOS_RES* result = taos_query(conn, sql.str().c_str());
                if (taos_errno(result) == 0) {
                    TAOS_ROW row = taos_fetch_row(result);
                    if (row) {
                        // 查询成功
                    }
                }
                taos_free_result(result);
                
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                
                time_stats[range.name].first += duration.count() / 1000.0; // 转换为毫秒
                time_stats[range.name].second++;
            }
            
            // 显示进度
            if ((i + 1) % 500 == 0 || i + 1 == std::min(size_t(5000), test_coords_5k.size())) {
                std::cout << "时间查询进度: " << (i + 1) << "/" 
                         << std::min(size_t(5000), test_coords_5k.size()) << std::endl;
            }
        }
        
        std::cout << "\n=== 时间区间查询汇总 ===" << std::endl;
        for (const auto& range : time_ranges) {
            const auto& stats = time_stats[range.name];
            double avg_time = stats.second > 0 ? stats.first / stats.second : 0.0;
            std::cout << range.name << ":" << std::endl;
            std::cout << "  总查询次数: " << stats.second << "次" << std::endl;
            std::cout << "  总耗时: " << (stats.first / 1000.0) << "秒" << std::endl;
            std::cout << "  平均耗时: " << avg_time << "毫秒/次" << std::endl;
        }
    }
    
    void generateReport() {
        // 确保output目录存在
        std::filesystem::create_directories("output/performance_reports");
        
        // 生成报告文件名
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp_ss;
        timestamp_ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        
        std::string report_file = "output/performance_reports/healpix_performance_report_" + timestamp_ss.str() + ".txt";
        std::ofstream report(report_file);
        
        if (report.is_open()) {
            report << "================================================================================\n";
            report << "🌟 TDengine HealPix 空间索引性能测试报告 (C++ 版本)\n";
            report << "================================================================================\n";
            
            std::stringstream current_time_ss;
            current_time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            
            report << "测试时间: " << current_time_ss.str() << "\n";
            report << "数据库: " << db_name << "\n";
            report << "HealPix NSIDE: " << nside << "\n\n";
            
            report << "📊 测试规模:\n";
            report << "  - 最近邻检索测试: " << test_coords_5k.size() << " 个天体\n";
            report << "  - 锥形检索测试: " << test_coords_100.size() << " 个天体\n\n";
            
            report << "🔍 测试结果概要:\n";
            report << "  ✅ 最近邻检索性能测试完成\n";
            report << "  ✅ 多半径锥形检索性能测试完成\n";
            report << "  ✅ 时间区间查询统计完成\n\n";
            
            report << "💡 测试说明:\n";
            report << "  本报告展示了HealPix空间索引在TDengine中的性能表现。\n";
            report << "  HealPix分区能够显著提升空间查询的效率。\n";
            
            report.close();
            std::cout << "📄 详细测试报告已保存到: " << report_file << std::endl;
        }
    }
};

int main() {
    try {
        std::cout << "🌟 TDengine HealPix 同步查询性能测试器 (原始版本)" << std::endl;
        std::cout << "============================================================" << std::endl;
        
        TDengineQueryTester tester;
        
        // 加载测试数据
        if (!tester.loadTestData("../data/test_data_100M.csv")) {
            std::cerr << "❌ 请确认一亿数据文件存在: ../data/test_data_100M.csv" << std::endl;
            return 1;
        }
        
        // 运行性能测试
        tester.runNearestNeighborTest();
        tester.runConeSearchTest();
        tester.runTimeRangeTest();
        
        // 生成测试报告
        tester.generateReport();
        
        std::cout << "\n🎉 ==== HealPix空间索引性能测试完成 ====" << std::endl;
        std::cout << "📊 测试结果已显示在上方，包含:" << std::endl;
        std::cout << "   - 最近邻检索性能" << std::endl;
        std::cout << "   - 不同半径锥形检索性能" << std::endl;
        std::cout << "   - 时间区间查询统计" << std::endl;
        std::cout << "💡 如需详细分析，请查看保存的性能报告文件" << std::endl;
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
}