#include <ostream>
#include <fstream>
#include <print>
#include <cstdlib>
#include <ranges>
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

    std::system("python ./visualizer/visualizer.py types ./mapdata_temp.dat");
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

    std::system("python ./visualizer/visualizer.py heights ./mapdata_temp.dat");
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

    std::system("python ./visualizer/visualizer.py buildings ./mapdata_temp.dat");
    std::filesystem::remove("./mapdata_temp.dat");
}

RGB hsv_to_rgb(double h, double s, double v) {
    h = std::fmod(h, 360.0);
    if (h < 0.0) h += 360.0;

    const double c = v * s;
    const double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    const double m = v - c;

    RGB rgb;

    if      (h < 60)  rgb = {c, x, 0};
    else if (h < 120) rgb = {x, c, 0};
    else if (h < 180) rgb = {0, c, x};
    else if (h < 240) rgb = {0, x, c};
    else if (h < 300) rgb = {x, 0, c};
    else              rgb = {c, 0, x};

    return {rgb.r + m, rgb.g + m, rgb.b + m};
}
