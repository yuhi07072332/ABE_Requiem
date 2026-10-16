#pragma once

#include <vector>
#include <map>
#include <string_view>
#include <cmath>
#include <iosfwd>

struct Point {
    int x, z;
    double y;
};

/// @brief 地図から読み込んだデータ
///
/// @details
/// * x座標を列、z座標を行とした行列でマップ内の点群を表す
/// * 各要素にはy座標値を格納している
/// * 行と列は0から始まる
/// * 欠損点のy座標値はNaNとし、is_missing()で判定できる
struct MapData {
    std::vector<double> grid;

    int x, z; // 地図の左下の座標

    int rows; // 行数(z軸方向の長さ)
    int cols; // 列数(x軸方向の長さ)

    /// i行j列のz座標を取得
    double operator[](int i, int j) const { return grid.at(i * cols + j); }
    double& operator[](int i, int j) { return grid.at(i * cols + j); }
    double at(int i, int j) const { return grid.at(i * cols + j); }
    double& at(int i, int j) { return grid.at(i * cols + j); }

    /// i行j列に対応するPointを取得
    Point point_at(int i, int j) const {
        return Point {x + j, z + i, at(i, j)};
    }

    /// i行j列の点が欠損点かどうかを判定
    bool is_missing(int i, int j) const { return std::isnan(at(i, j)); }
};

/* ----- algorithm ----- */

std::map<int, int> calc_frequency(const std::vector<double>& heights,
                                  int width = 5);
std::vector<Point> find_ground(const MapData& data);


/* ----- vrml ----- */

inline constexpr std::string_view VRML_HEADER = "#VRML V2.0 utf8";

void generate_points(const MapData& data, std::string_view out_filename);
int write_delaunay_triangulation(const std::vector<Point>& pts,
                                 std::ostream& os);

/* ----- input ----- */

MapData read_map(std::string_view data_path);
