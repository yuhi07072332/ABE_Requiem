#include <ostream>
#include <fstream>
#include <print>
#include <cstdlib>

#include "genk.hpp"


std::ostream& operator<<(std::ostream& os, Coord c) {
    os << '(' << c.i << ", " << c.j << ')';
    return os;
}

std::ostream& operator<<(std::ostream& os, Point p) {
    os << '(' << p.x << ", " << p.y << ", " << p.z << ')';
    return os;
}


/// @brief MapDataのtype行列をtypes.datに出力
void show_mapdata_types(const MapData& data) {
    std::ofstream ofs{"types.dat"};

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            std::print(ofs, "{} ", static_cast<int>(data.type_at(i, j)));
        }
        std::print(ofs, "\n");
    }

    std::system("python ./tools/visualizer.py ./types.dat");
}
