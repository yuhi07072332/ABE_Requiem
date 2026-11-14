#include <ostream>
#include <fstream>
#include <print>
#include <cstdlib>
#include <filesystem>

#include "genk.hpp"


std::ostream& operator<<(std::ostream& os, Coord c) {
    os << '(' << c.i << ", " << c.j << ')';
    return os;
}

std::ostream& operator<<(std::ostream& os, Point p) {
    os << '(' << p.x << ", " << p.y << ", " << p.z << ')';
    return os;
}

/// @brief MapDataのtype行列を図示し保存する(./tools/visualizer.pyを実行する)
void show_mapdata(const MapData& data) {
    std::ofstream ofs{"mapdata_temp.dat"};
    std::println(ofs, "{} {}", data.rows, data.cols);

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            std::print(ofs, "{} ", static_cast<int>(data.type_at(i, j)));
        }
        std::print(ofs, "\n");
    }
    ofs.close();

    std::system("python ./tools/visualizer.py ./mapdata_temp.dat");
    std::filesystem::remove("./mapdata_temp.dat");
}
