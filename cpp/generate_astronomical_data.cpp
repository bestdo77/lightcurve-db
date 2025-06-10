#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <ctime>
#include <sstream>
#include <cstring>
#include <algorithm>

struct AstronomicalRecord {
    std::string timestamp;
    int source_id;
    double ra;      // 赤经
    double dec;     // 赤纬  
    double mag;     // 星等
    double jd_tcb;  // 儒略日
};

class AstronomicalDataGenerator {
private:
    std::mt19937 rng;
    std::uniform_real_distribution<double> ra_dist;
    std::uniform_real_distribution<double> dec_dist;
    std::uniform_real_distribution<double> mag_dist;
    std::uniform_real_distribution<double> jd_dist;
    std::uniform_int_distribution<int> day_dist;
    std::uniform_int_distribution<int> hour_dist;
    std::uniform_int_distribution<int> minute_dist;
    std::uniform_int_distribution<int> second_dist;

public:
    AstronomicalDataGenerator() 
        : rng(std::chrono::steady_clock::now().time_since_epoch().count()),
          ra_dist(0.0, 360.0),
          dec_dist(-90.0, 90.0), 
          mag_dist(8.0, 18.0),
          jd_dist(2460311.0, 2460311.0 + 365.0),
          day_dist(0, 364),
          hour_dist(0, 23),
          minute_dist(0, 59),
          second_dist(0, 59) {}

    std::string generateTimestamp() {
        // 生成2024年的随机时间戳
        std::tm base_time = {};
        base_time.tm_year = 2024 - 1900; // 年份从1900开始计算
        base_time.tm_mon = 0;            // 1月
        base_time.tm_mday = 1;           // 1日
        
        int days = day_dist(rng);
        int hours = hour_dist(rng);
        int minutes = minute_dist(rng);
        int seconds = second_dist(rng);
        
        base_time.tm_mday += days;
        base_time.tm_hour = hours;
        base_time.tm_min = minutes;
        base_time.tm_sec = seconds;
        
        std::mktime(&base_time); // 规范化时间
        
        std::ostringstream oss;
        oss << std::put_time(&base_time, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    AstronomicalRecord generateRecord(int source_id, double ra_center = -1, double dec_center = -91) {
        AstronomicalRecord record;
        record.timestamp = generateTimestamp();
        record.source_id = source_id;
        
        if (ra_center >= 0 && dec_center > -91) {
            // 在中心坐标附近微小扰动
            std::uniform_real_distribution<double> perturbation(-0.001, 0.001);
            record.ra = ra_center + perturbation(rng);
            record.dec = dec_center + perturbation(rng);
        } else {
            record.ra = ra_dist(rng);
            record.dec = dec_dist(rng);
        }
        
        record.mag = mag_dist(rng);
        record.jd_tcb = jd_dist(rng);
        
        return record;
    }

    void generateData(int num_sources, int records_per_source, const std::string& output_file) {
        std::cout << "🌟 生成 " << num_sources << " 个天体，每个 " << records_per_source << " 条记录的数据..." << std::endl;
        
        std::vector<AstronomicalRecord> data;
        data.reserve(num_sources * records_per_source);
        
        // 生成数据
        for (int source_id = 1; source_id <= num_sources; ++source_id) {
            // 为每个天体生成一个中心坐标，均匀分布在全天球
            double ra_center = ra_dist(rng);
            double dec_center = dec_dist(rng);
            
            for (int i = 0; i < records_per_source; ++i) {
                data.push_back(generateRecord(source_id, ra_center, dec_center));
            }
            
            // 显示进度
            if (source_id % (num_sources / 10) == 0 || source_id == num_sources) {
                std::cout << "📈 进度: " << source_id << "/" << num_sources 
                         << " (" << (source_id * 100 / num_sources) << "%)" << std::endl;
            }
        }
        
        // 按时间排序（简单排序，实际应该转换为时间戳排序）
        std::sort(data.begin(), data.end(), [](const AstronomicalRecord& a, const AstronomicalRecord& b) {
            return a.timestamp < b.timestamp;
        });
        
        // 确保输出目录存在
        std::filesystem::path file_path(output_file);
        if (file_path.has_parent_path()) {
            std::filesystem::create_directories(file_path.parent_path());
        }
        
        // 保存为CSV
        saveToCSV(data, output_file);
        
        // 生成统计报告
        generateReport(data, output_file, num_sources, records_per_source);
        
        std::cout << "✅ 数据已保存到 " << output_file << std::endl;
        std::cout << "📊 总记录数: " << data.size() << std::endl;
        std::cout << "🌟 源数量: " << num_sources << std::endl;
    }

private:
    void saveToCSV(const std::vector<AstronomicalRecord>& data, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("无法打开输出文件: " + filename);
        }
        
        // 写入CSV头部
        file << "ts,source_id,ra,dec,mag,jd_tcb\n";
        
        // 写入数据
        for (const auto& record : data) {
            file << record.timestamp << ","
                 << record.source_id << ","
                 << std::fixed << std::setprecision(6) << record.ra << ","
                 << std::fixed << std::setprecision(6) << record.dec << ","
                 << std::fixed << std::setprecision(2) << record.mag << ","
                 << std::fixed << std::setprecision(6) << record.jd_tcb << "\n";
        }
        
        file.close();
    }
    
    void generateReport(const std::vector<AstronomicalRecord>& data, 
                       const std::string& output_file,
                       int num_sources, int records_per_source) {
        // 确保logs目录存在
        std::filesystem::create_directories("output/logs");
        
        // 获取当前时间戳
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp_ss;
        timestamp_ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");
        
        std::string report_file = "output/logs/data_generation_report_" + timestamp_ss.str() + ".txt";
        std::ofstream report(report_file);
        
        if (report.is_open()) {
            report << "============================================================\n";
            report << "🌟 天文观测数据生成报告\n";
            report << "============================================================\n";
            
            std::stringstream current_time_ss;
            current_time_ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            
            report << "生成时间: " << current_time_ss.str() << "\n";
            report << "输出文件: " << output_file << "\n";
            report << "天体数量: " << num_sources << "\n";
            report << "每天体记录数: " << records_per_source << "\n";
            report << "总记录数: " << data.size() << "\n";
            
            if (!data.empty()) {
                double min_ra = data[0].ra, max_ra = data[0].ra;
                double min_dec = data[0].dec, max_dec = data[0].dec;
                double min_mag = data[0].mag, max_mag = data[0].mag;
                
                for (const auto& record : data) {
                    min_ra = std::min(min_ra, record.ra);
                    max_ra = std::max(max_ra, record.ra);
                    min_dec = std::min(min_dec, record.dec);
                    max_dec = std::max(max_dec, record.dec);
                    min_mag = std::min(min_mag, record.mag);
                    max_mag = std::max(max_mag, record.mag);
                }
                
                report << "时间范围: " << data.front().timestamp << " ~ " << data.back().timestamp << "\n";
                report << "赤经范围: " << std::fixed << std::setprecision(3) << min_ra << "° ~ " << max_ra << "°\n";
                report << "赤纬范围: " << std::fixed << std::setprecision(3) << min_dec << "° ~ " << max_dec << "°\n";
                report << "星等范围: " << std::fixed << std::setprecision(2) << min_mag << " ~ " << max_mag << "\n";
            }
            
            // 计算文件大小
            std::ifstream file(output_file, std::ifstream::ate | std::ifstream::binary);
            if (file.is_open()) {
                double file_size_mb = file.tellg() / (1024.0 * 1024.0);
                report << "文件大小: " << std::fixed << std::setprecision(1) << file_size_mb << " MB\n";
            }
            
            report.close();
            std::cout << "📄 生成报告已保存到: " << report_file << std::endl;
        }
    }
};

void printUsage(const char* program_name) {
    std::cout << "用法: " << program_name << " [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  --num_sources <数量>        天体数量 (默认: 100000)\n";
    std::cout << "  --records_per_source <数量> 每个天体的记录数 (默认: 100)\n";
    std::cout << "  --output <文件名>           输出文件名 (默认: data/generated_data_large.csv)\n";
    std::cout << "  --help                      显示此帮助信息\n\n";
    std::cout << "示例:\n";
    std::cout << "  " << program_name << " --num_sources 50000 --records_per_source 200\n";
    std::cout << "  " << program_name << " --output my_data.csv\n";
}

int main(int argc, char* argv[]) {
    int num_sources = 100000;
    int records_per_source = 100;
    std::string output_file = "data/generated_data_large.csv";
    
    // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--num_sources") == 0 && i + 1 < argc) {
            num_sources = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--records_per_source") == 0 && i + 1 < argc) {
            records_per_source = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (std::strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "未知参数: " << argv[i] << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    try {
        std::cout << "🌟 天文观测数据生成器 (C++版本)" << std::endl;
        std::cout << "============================================================" << std::endl;
        
        AstronomicalDataGenerator generator;
        generator.generateData(num_sources, records_per_source, output_file);
        
        std::cout << "\n🎊 数据生成完成！" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
}