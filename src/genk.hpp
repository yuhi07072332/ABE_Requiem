#pragma once

#include <vector>
#include <map>
#include <string_view>
#include <cmath>
#include <iosfwd>
#include <cstdint>

struct Point {
    int x, z;
    double y;
};

/// 行列内の位置を表す
struct Coord {
    int i, j;
};

// 点の種類
enum class Type : uint8_t {
    Unknown,                // 未知
    Ground,                 // 地面
    Building,               // 建物
    BuildingOutline,        // 建物(輪郭)
};

/// @brief 地図から読み込んだデータ
///
/// @details
/// * x座標を列、z座標を行とした行列でマップ内の点群を表す
/// * 各要素にはy座標値と点の種類(`Type`)を格納している
/// * 行と列は0から始まる
struct MapData {
    std::vector<double> grid;
    std::vector<Type> types;

    int x, z; // 地図の左下の座標

    int rows; // 行数(z軸方向の長さ)
    int cols; // 列数(x軸方向の長さ)

    /// i行j列のz座標を取得
    double at(int i, int j) const { return grid.at(i * cols + j); }
    double at(Coord c) const { return grid.at(c.i * cols + c.j); }

    /// i行j列に対応するPointを取得
    Point point_at(int i, int j) const {
        return Point {x + j, z + i, at(i, j)};
    }
    Point point_at(Coord c) const { return point_at(c.i, c.j); }

    /// i行j列のTypeを取得
    Type type_at(int i, int j) const { return types.at(i * cols + j); }
    Type type_at(Coord c) const { return types.at(c.i * cols + c.j); }
    Type& type_at(int i, int j) { return types.at(i * cols + j); }
    Type& type_at(Coord c) { return types.at(c.i * cols + c.j); }

    /// i行j列の点が欠損点かどうかを判定
    bool is_missing(int i, int j) const { return std::isnan(at(i, j)); }
    bool is_missing(Coord c) const { return is_missing(c.i, c.j); }
};

/* ----- algorithm ----- */

std::map<int, int> calc_frequency(const std::vector<double>& heights,
                                  int width = 5);
std::vector<Coord> find_ground(const MapData& data);


/* ----- vrml ----- */

inline constexpr std::string_view VRML_HEADER = "#VRML V2.0 utf8";

void write_elevation_grid(std::ostream& os, const MapData& data);
int write_delaunay_triangulation(std::ostream& os,
                                 const std::vector<Coord>& coords,
                                 const MapData& data,
                                 std::string_view diffuse_color = "0.3 0.5 0.7");

/* ----- input ----- */

MapData read_map(std::string_view data_path);
