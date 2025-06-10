#include <iostream>
#include <cmath>
#include <healpix_base.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline double deg2rad(double deg) {
    return deg * M_PI / 180.0;
}

int main() {
    int nside = 64;
    Healpix_Base healpix_map(nside, NEST, SET_NSIDE);
    
    // 测试数据库中的真实坐标
    double ra = 45.2579355828128;
    double dec = 0.458364992350643;
    
    std::cout << "=== HealPix ID 计算调试 ===" << std::endl;
    std::cout << "NSIDE: " << nside << std::endl;
    std::cout << "RA: " << ra << " 度" << std::endl;
    std::cout << "DEC: " << dec << " 度" << std::endl;
    
    // 计算 theta 和 phi
    double theta = deg2rad(90.0 - dec);  // 余纬度
    double phi = deg2rad(ra);             // 经度
    
    std::cout << "\n转换为球坐标：" << std::endl;
    std::cout << "theta (余纬度): " << theta << " 弧度 = " << (theta * 180.0 / M_PI) << " 度" << std::endl;
    std::cout << "phi (经度): " << phi << " 弧度 = " << (phi * 180.0 / M_PI) << " 度" << std::endl;
    
    // 计算 HealPix ID
    pointing pt(theta, phi);
    long healpix_id = healpix_map.ang2pix(pt);
    
    std::cout << "\n计算结果：" << std::endl;
    std::cout << "HealPix ID: " << healpix_id << std::endl;
    
    // 验证逆变换
    pointing pt_back = healpix_map.pix2ang(healpix_id);
    double ra_back = pt_back.phi * 180.0 / M_PI;
    double dec_back = 90.0 - pt_back.theta * 180.0 / M_PI;
    
    std::cout << "\n逆变换验证：" << std::endl;
    std::cout << "反算 RA: " << ra_back << " 度" << std::endl;
    std::cout << "反算 DEC: " << dec_back << " 度" << std::endl;
    std::cout << "RA 误差: " << std::abs(ra - ra_back) << " 度" << std::endl;
    std::cout << "DEC 误差: " << std::abs(dec - dec_back) << " 度" << std::endl;
    
    // 测试几个不同的坐标
    std::cout << "\n=== 批量测试 ===" << std::endl;
    double test_coords[][2] = {
        {0.0, 0.0},      // 赤道，0度经线
        {90.0, 0.0},     // 赤道，90度经线  
        {180.0, 0.0},    // 赤道，180度经线
        {270.0, 0.0},    // 赤道，270度经线
        {0.0, 90.0},     // 北极
        {0.0, -90.0},    // 南极
        {45.26, 0.46},   // 测试坐标
    };
    
    for (int i = 0; i < 7; ++i) {
        double test_ra = test_coords[i][0];
        double test_dec = test_coords[i][1];
        
        double test_theta = deg2rad(90.0 - test_dec);
        double test_phi = deg2rad(test_ra);
        pointing test_pt(test_theta, test_phi);
        long test_id = healpix_map.ang2pix(test_pt);
        
        std::cout << "RA=" << test_ra << "°, DEC=" << test_dec << "° => HealPix ID=" << test_id << std::endl;
    }
    
    return 0;
} 