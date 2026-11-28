#include <ostream>
#include <fstream>
#include <print>
#include <cstdlib>
#include <ranges>
#include <filesystem>
#include <utility>
#include <map>

#include "genk.hpp"

std::ostream& operator<<(std::ostream& os, Coord c) {
    os << '(' << c.i << ", " << c.j << ')';
    return os;
}

std::ostream& operator<<(std::ostream& os, Point p) {
    os << '(' << p.x << ", " << p.y << ", " << p.z << ')';
    return os;
}

/// @brief 高さの度数分布を計算する
/// @returns keyを階級の下限値, valueを度数とするmap
std::map<int, int> calc_frequency(const std::vector<double>& heights,
                                  int width) {
    std::map<int, int> freq;

    for (double height : heights) {
        if (std::isnan(height)) continue;
        int lower = static_cast<int>(height / width) * width;
        if (auto it = freq.find(lower); it != freq.end()) it->second++;
        else freq.try_emplace(lower, 1);
    }

    return freq;
}


/// @brief visualizer.pyを実行してMapDataのtype行列を図示する
void show_mapdata_types(const MapData& data) {
    std::ofstream ofs{"mapdata_temp.dat"};
    std::println(ofs, "{} {}", data.rows, data.cols);

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            std::print(ofs, "{} ", static_cast<int>(data.type_at(i, j)));
        }
        std::print(ofs, "\n");
    }
    ofs.close();

    std::system("python ./tools/visualizer.py types ./mapdata_temp.dat");
    std::filesystem::remove("./mapdata_temp.dat");
}

/// @brief visualizer.pyを実行してMapDataの高さ行列を図示する
void show_mapdata_heights(const MapData& data) {
    std::ofstream ofs{"mapdata_temp.dat"};
    std::println(ofs, "{} {}", data.rows, data.cols);

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            std::print(ofs, "{} ", data.at(i, j));
        }
        std::print(ofs, "\n");
    }
    ofs.close();

    std::system("python ./tools/visualizer.py heights ./mapdata_temp.dat");
    std::filesystem::remove("./mapdata_temp.dat");
}

/// @brief visualizer.pyを実行して建物区分結果を図示する
void show_mapdata_buildings(const MapData& data) {
    std::vector<int> building_ids(data.rows * data.cols, 0);

    for (const auto& [id, building] : data.buildings | std::views::enumerate) {
        for (Coord c : building.coords) {
            building_ids.at(c.i * data.cols + c.j) = id + 1;
        }
    }

    std::ofstream ofs{"mapdata_temp.dat"};
    std::println(ofs, "{} {}", data.rows, data.cols);

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            std::print(ofs, "{} ", building_ids.at(i * data.cols + j));
        }
        std::print(ofs, "\n");
    }
    ofs.close();

    std::system("python ./tools/visualizer.py buildings ./mapdata_temp.dat");
    std::filesystem::remove("./mapdata_temp.dat");
}

