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

// ────────────────────────────── algorithm ──────────────────────────────

/// @brief 高さの度数分布を計算する
/// @returns keyを階級の下限値, valueを度数とするmap
std::map<int, int> calc_frequency(const std::vector<double>& heights,
                                  int width = 5);

/// @brief 地面と判定された点群を返す
/// @details
/// 暫定のアルゴリズム:
/// * 高さの度数が最も高い2つの階級(の下限値)を地面高さの候補とし、高さがそれ
///   より低い階級も地面の候補とする
/// * マップを16 * 16のチャンクに分けて、チャンクごとに：
///     - チャンク内の最も低い高さが地面の候補にあれば、チャンク内でそれを地面
///       の高さの基準とする
///     - 基準よりNメートル高ければ地面以外と判定する
///
/// 可能な問題点:
/// * 斜めの地面を建物に誤判定することがある
/// * 逆に建物が地面に誤判定することがある
/// * チャンク間のつなぎがあまり綺麗じゃないことがある
std::vector<Point> find_ground(const MapData& data);

// ──────────────────────────────── vrml ─────────────────────────────

inline constexpr std::string_view VRML_HEADER = "#VRML V2.0 utf8";

/// 点群をElevationGridで出力
void generate_points(const MapData& data, std::string_view out_filename);

/// @brief 点群に対しドロネーの三角形分割を実行し、IndexFacedSetとして出力
/// @returns 三角形の面数
int write_delaunay_triangulation(const std::vector<Point>& pts,
                                 std::ostream& os);

// ──────────────────────────────── input ────────────────────────────────

/// @brief 地図データから点群を読み込み、`MapData`を作成
///
/// @details
/// この関数は`read_points()`から点群を読み込み、さらに2次元配列になる
/// ように点群の欠損点を補う(補間は行わない, すなわち欠損点としてdataに格納)
MapData read_map(std::string_view data_path);
