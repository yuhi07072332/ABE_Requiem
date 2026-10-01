#include <algorithm>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>
#include <filesystem>

using path = std::filesystem::path;

// ┌                                                         ┐
// │                     データ読み込み                      │
// └                                                         ┘

struct Point {
    double x, y, z;
};


void read_data(path data_path) {
    FILE* f = std::fopen(data_path.c_str(), "r");

    std::vector<Point> pts;
    pts.reserve(1045486);
    while (std::fscanf(f,  "%10lf %10lf %10lf", )) {
        Point p;
        std::sscanf(line.c_str(), "%10lf %10lf %10lf", &p.x, &p.y, &p.z);
        pts.push_back(p);

    }
    std::cout << "read " << pts.size() << "points\n";
}

// ┌                                                         ┐
// │                     メインエントリ                      │
// └                                                         ┘

int main(int argc, const char** argv) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <input_data>";
        return 1;
    }

    read_data(argv[1]);
}
