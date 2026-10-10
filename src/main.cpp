#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <limits>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "third_party/argparse.hpp"

// デバック用マクロ
#define dbg() std::println(stderr, "\x1b[1;33m[DEBUG: {} line]\x1b[m", __LINE__)

struct Point {
    int x, y;
    double z;
};

/// @brief 地図から読み込んだデータ
///
/// @details
/// * x座標を列、y座標を行とした行列でマップ内の点群を表す
/// * 各要素にはz座標値を格納している
/// * 行と列は0から始まる
/// * 欠損点のz座標値はNaNとし、is_missing()で判定できる
struct MapData {
    std::vector<double> grid;

    int x, y; // 地図の左下の座標

    int rows; // 行数(y軸方向の長さ)
    int cols; // 列数(x軸方向の長さ)

    /// i行j列のz座標を取得
    double operator[](int i, int j) const { return grid.at(i * cols + j); }
    double& operator[](int i, int j) { return grid.at(i * cols + j); }
    double at(int i, int j) const { return grid.at(i * cols + j); }
    double& at(int i, int j) { return grid.at(i * cols + j); }

    /// i行j列の点が欠損点かどうかを判定
    bool is_missing(int i, int j) const { return std::isnan(at(i, j)); }
};

/// 建物
struct Building {
    // TODO: 
};

// ┌                                                         ┐
// │                      アルゴリズム                       │
// └                                                         ┘



// ┌                                                         ┐
// │                       データ入力                        │
// └                                                         ┘

struct BoundingBox {
    std::vector<Point> pts;
    int min_x, min_y, max_x, max_y;
};

/// @brief データファイルの点座標を全て読み込む
///
/// @returns 点群配列、x座標, y座標の最大値及び最小値
/// @throws
/// ファイルが開けない場合や読み込み失敗した場合はruntime_errorを投げる
BoundingBox read_points(const char* data_path) {
    std::ifstream ifs(data_path);
    if (!ifs.is_open())
        throw std::runtime_error("read_points(): ファイルを開けませんでした");

    std::vector<Point> pts;
    pts.reserve(1045486);

    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int max_y = std::numeric_limits<int>::min();

    std::string buf;
    while (std::getline(ifs, buf)) {
        double x, y, z;
        if (sscanf(buf.data(), "%10lf%10lf%10lf", &x, &y, &z) != 3) {
            throw std::runtime_error("read_points(): 読み込み失敗しました");
        }
        pts.push_back({(int)x, (int)y, z});
        min_x = std::min(min_x, (int)x);
        min_y = std::min(min_y, (int)y);
        max_x = std::max(max_x, (int)x);
        max_y = std::max(max_y, (int)y);
    }

    return {pts, min_x, min_y, max_x, max_y};
}

/// @brief 地図データから点群を読み込み、`MapData`を作成
///
/// @details
/// この関数は`read_points()`から点群を読み込み、さらに2次元配列になる
/// ように点群の欠損点を補う(補間は行わない, すなわち欠損点としてdataに格納)
MapData read_map(const char* data_path) {
    auto [pts, min_x, min_y, max_x, max_y] = read_points(data_path);
    const int rows = max_y - min_y + 1;
    const int cols = max_x - min_x + 1;

    std::vector<double> grid(rows * cols,
                             std::numeric_limits<double>::quiet_NaN());

    for (Point p : pts) {
        const int i = p.y - min_y;
        const int j = p.x - min_x;
        if (p.z == -9999.99) {
            grid[i * cols + j] = std::numeric_limits<double>::quiet_NaN();
        } else grid[i * cols + j] = p.z;
    }

    return MapData {
        .grid = std::move(grid),
        .x = min_x,
        .y = min_y,
        .rows = rows,
        .cols = cols,
    };
}

// ┌                                                         ┐
// │                        VRML出力                         │
// └                                                         ┘



/// 点群をElevationGridでout.wrlに出力
void generate_points(const MapData& data, std::string_view out_filename) {
    std::ofstream ofs(out_filename.data());
    std::println(ofs, "#VRML V2.0 utf8");

    constexpr std::string_view FMT1 =
        "Shape {{\n"
        "   appearance Appearance {{\n"
        "       material Material {{\n"
        "           diffuseColor 0.812 0.664 0.988\n"
        "       }}\n"
        "   }}\n"
        "   geometry ElevationGrid {{\n"
        "       xDimension {0}\n"
        "       zDimension {1}\n"
        "       xSpacing 1\n"
        "       zSpacing 1\n"
        "       solid TRUE\n"
        "       ccw TRUE\n"
        "       height [\n";

    constexpr std::string_view FMT2 = 
        "       ]\n"
        "   }}\n"
        "}}\n";

    std::print(ofs, FMT1, data.cols, data.rows);

    int count = 0;
    for (int y = data.rows - 1; y >= 0; --y) {
        for (int x = 0; x < data.cols; ++x) {
            if (data.is_missing(y, x)) std::print(ofs, "100, ");
            else
                std::print(ofs, "{}, ", data[y, x]);
            if (count++ == 50) {
                count = 0;
                std::println(ofs);
            }
        }
    }

    std::print(ofs, FMT2);
}

// ┌                                                         ┐
// │                     メインエントリ                      │
// └                                                         ┘

enum class Command {
    ReportHeights,
    GeneratePoints,
};

struct Option : argparse::Args {
    std::string& command = arg("コマンド");
    std::string& file_path = arg("入力ファイル");
    std::string& output_filename = kwarg("o", "")
        .set_default("out.wrl");

    void help() override {
        std::println("\x1b[1;4m使い方\x1b[m\n\n\x1b[1m"
                     "./genk \x1b[2m[オプション...]\x1b[m <コマンド> <地図データ>\x1b[m\n");
        std::println("\x1b[1;4mコマンド\x1b[m\n");
        std::println("\x1b[1;34mreport-heights\x1b[m\t\t点群の高さの度数分布表を表示");
        std::println("\x1b[1;34mgen-points\x1b[m\t\t地図データから点群を直接出力");

        std::println();
        std::println("\x1b[1;4mオプション\x1b[m\n");
        std::println("\x1b[1;34m-o <ファイル名>\x1b[m\t\t出力ファイル名を指定");
    }

    Command parse_command() const {
        if (command == "report-heights") return Command::ReportHeights;
        if (command == "gen-points") return Command::GeneratePoints;
        throw std::runtime_error(std::format("未知なコマンド名: {}", command));
    }

    void normalize_output_filename() const {
        if (!output_filename.ends_with(".wrl")) output_filename.append(".wrl");
    }
};

void report_heights(std::vector<double>& heights) {
    std::ranges::sort(heights);
    const auto [min_height, max_height] = std::ranges::minmax(heights);

    auto it = heights.cbegin();

    for (int i = -30; i <= max_height; i += 5) {
        auto end = std::ranges::lower_bound(it, heights.cend(), i + 5);
        std::size_t len = end - it;

        std::println("{} <= x < {}: {}", i, i + 5, len);
        it = end;
    }
}

int main(int argc, const char** argv) {
    Option opt;
    Command command;
    try {
        opt.parse(argc, argv, true);
        opt.normalize_output_filename();
        command = opt.parse_command();
    } catch (const std::runtime_error& e) {
        std::println("\x1b[1;31mエラー:\x1b[m {}\n", e.what());
        opt.help();
        return 1;
    }

    try {
        auto data = read_map(opt.file_path.c_str());
        switch (command) {
            case Command::ReportHeights: report_heights(data.grid);
            case Command::GeneratePoints: generate_points(data, opt.output_filename);
        }
    } catch (const std::exception& e) {
        std::println("\x1b[1;31mエラー:\x1b[m {}", e.what());
        return 1;
    }

    return 0;
}
