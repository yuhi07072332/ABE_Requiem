#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <print>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
    double x1, y1; // 左下の座標
    double x2, y2; // 右上の座標
    double height; // 高さ
};

/// 地面
struct Ground {
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

void write_elevation_grid(std::ostream& os, const MapData& data) {
    constexpr std::string_view FMT =
        "Shape {{\n"
        "   appearance Appearance {{\n"
        "       material Material {{\n"
        "           diffuseColor 0.812 0.664 0.988\n"
        "       }}\n"
        "   }}\n"
        "   geometry ElevationGrid {{\n"
        "       xDimension {0}\n"
        "       zDimension {1}\n"
        "       xSpacing 0.01\n"
        "       zSpacing 0.01\n"
        "       solid TRUE\n"
        "       ccw TRUE\n"
        "       height [\n"
        "{2}"
        "       ]\n"
        "   }}\n"
        "}}\n";

    std::string heights_str;
    int count = 0;
    for (int y = data.rows - 1; y >= 0; --y) {
        for (int x = 0; x < data.cols; ++x) {
            if (data.is_missing(y, x)) heights_str.append("2.5, ");
            else
                std::format_to(
                    std::back_inserter(heights_str), "{}, ", data[y, x] / 100);
            if (count++ == 50) {
                count = 0;
                heights_str.push_back('\n');
            }
        }
    }
    std::print(os, FMT, data.cols, data.rows, heights_str);
}

void write_vrml(const char* file_path, const MapData& data) {
    std::ofstream ofs(file_path);
    std::println(ofs, "#VRML V2.0 utf8");
    write_elevation_grid(ofs, data);
}

// ┌                                                         ┐
// │                     メインエントリ                      │
// └                                                         ┘

int main(int argc, const char** argv) {
    if (argc != 2) {
        std::println(stderr, "使い方: {} <入力ファイル>", argv[0]);
        return 1;
    }

    MapData data = read_map(argv[1]);
    write_vrml("out.wrl", data);
}
