#include <algorithm>
#include <map>
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
#include "third_party/delaunator/delaunator.hpp"

// デバック用マクロ
#define dbg() std::println(stderr, "\x1b[1;33m[DEBUG: {} line]\x1b[m", __LINE__)

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

    Point point_at(int i, int j) const {
        return Point { x + j, z + i, at(i, j) };
    }

    /// i行j列の点が欠損点かどうかを判定
    bool is_missing(int i, int j) const { return std::isnan(at(i, j)); }
};

// ┌                                                         ┐
// │                       データ処理                        │
// └                                                         ┘

/// @brief 高さの度数分布を計算する
/// @returns keyを階級の下限値, valueを度数とするmap
std::map<int, int> calc_frequency(const std::vector<double>& heights, int width = 5) {
    std::map<int, int> freq;

    for (double height : heights) {
        if (std::isnan(height)) continue;
        int lower = static_cast<int>(height / width) * width;
        if (auto it = freq.find(lower); it != freq.end()) it->second++;
        else freq.try_emplace(lower, 1);
    }

    return freq;
}

/// @brief 度数が2番目まで最大なkeyを返す
std::pair<int, int> max2(const std::map<int, int>& data) {
    int first_key = 0, second_key = 0;

    for (auto [key, value] : data) {
        if (value > data.at(first_key)) {
            second_key = first_key;
            first_key = key;
        } else if (value > data.at(second_key)) {
            second_key = key;
        }
    }

    return {first_key, second_key};
}

/// @brief 地面と判定された点群を返す
/// @details
/// 暫定のアルゴリズム:
/// * 高さの度数が最も高い2つの階級(の下限値)を地面高さの候補とし、高さがそれより低い階級も地面の候補とする
/// * マップを16 * 16のチャンクに分けて、チャンクごとに：
///     - チャンク内の最も低い高さが地面の候補にあれば、チャンク内でそれを地面の高さの基準とする
///     - 基準よりNメートル高ければ地面以外と判定する
std::vector<Point> find_ground(const MapData& data) {
    std::println("地面判定中...");
    constexpr int N = 2;
    constexpr int CHUNK_WIDTH = 16;
    constexpr int CHUNK_HEIGHT = 16;

    auto freq = calc_frequency(data.grid);
    auto freq_max2 = max2(freq);

    // これ以下の高さを地面の候補とする
    double ground_candidate_height = std::max(freq_max2.first, freq_max2.second);
    std::println("地面基準: {}", ground_candidate_height);

    std::vector<Point> ground_points;

    auto process_chunk = [&](int i_begin, int j_begin) -> void {
        // チャンク内高さが一番低い点を探す
        double min_height = std::numeric_limits<double>::max();
        for (int i = i_begin; i < i_begin + CHUNK_HEIGHT; ++i) {
            for (int j = j_begin; j < j_begin + CHUNK_HEIGHT; ++j) {
                if (data.is_missing(i, j)) continue;
                if (data[i, j] < min_height) min_height = data[i, j];
            }
        }

        if (min_height > ground_candidate_height) return;

        for (int i = i_begin; i < i_begin + CHUNK_HEIGHT; ++i) {
            for (int j = j_begin; j < j_begin + CHUNK_HEIGHT; ++j) {
                Point p = data.point_at(i, j);
                if (p.y < min_height + N) ground_points.push_back(p);
            }
        }
    };

    int chunks_rows = data.rows / CHUNK_HEIGHT;
    int chunks_cols = data.cols / CHUNK_WIDTH;

    for (int r = 0; r < chunks_rows; ++r) {
        for (int c = 0; c < chunks_cols; ++c) {
            int i_begin = r * CHUNK_HEIGHT;
            int j_begin = c * CHUNK_WIDTH;

            process_chunk(i_begin, j_begin);
        }
    }

    return ground_points;
}


// ┌                                                         ┐
// │                       データ入力                        │
// └                                                         ┘

struct BoundingBox {
    std::vector<Point> pts;
    int min_x, min_z, max_x, max_z;
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
    int min_z = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int max_z = std::numeric_limits<int>::min();

    std::string buf;
    while (std::getline(ifs, buf)) {
        double x, z, y;
        if (std::sscanf(buf.data(), "%10lf%10lf%10lf", &x, &z, &y) != 3) {
            throw std::runtime_error("read_points(): 読み込み失敗しました");
        }
        pts.push_back({(int)x, (int)z, y});
        min_x = std::min(min_x, (int)x);
        min_z = std::min(min_z, (int)z);
        max_x = std::max(max_x, (int)x);
        max_z = std::max(max_z, (int)z);
    }

    return {std::move(pts), min_x, min_z, max_x, max_z};
}

/// @brief 地図データから点群を読み込み、`MapData`を作成
///
/// @details
/// この関数は`read_points()`から点群を読み込み、さらに2次元配列になる
/// ように点群の欠損点を補う(補間は行わない, すなわち欠損点としてdataに格納)
MapData read_map(const char* data_path) {
    auto [pts, min_x, min_z, max_x, max_z] = read_points(data_path);
    const int rows = max_z - min_z + 1;
    const int cols = max_x - min_x + 1;

    std::vector<double> grid(rows * cols,
                             std::numeric_limits<double>::quiet_NaN());

    for (Point p : pts) {
        const int i = p.z - min_z;
        const int j = p.x - min_x;
        if (p.y == -9999.99) {
            grid[i * cols + j] = std::numeric_limits<double>::quiet_NaN();
        } else grid[i * cols + j] = p.y;
    }

    return MapData {
        .grid = std::move(grid),
        .x = min_x,
        .z = min_z,
        .rows = rows,
        .cols = cols,
    };
}

// ┌                                                         ┐
// │                        VRML出力                         │
// └                                                         ┘

constexpr std::string_view VRML_HEADER = "#VRML V2.0 utf8";

/// 点群をElevationGridで出力
void generate_points(const MapData& data, std::string_view out_filename) {
    std::ofstream ofs(out_filename.data());
    std::println(ofs, VRML_HEADER);

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
    for (int z = 0; z < data.rows; ++z) {
        for (int x = 0; x < data.cols; ++x) {
            if (data.is_missing(z, x)) std::print(ofs, "100, ");
            else
                std::print(ofs, "{}, ", data[z, x]);
            if (count++ == 50) {
                count = 0;
                std::println(ofs);
            }
        }
    }

    std::print(ofs, FMT2);
}

/// @brief 点群に対しドロネーの三角形分割を実行し、IndexFacedSetとして出力
/// @returns 分割された三角形の座標のインデックス
void delaunay_triangulation(const std::vector<Point>& pts, std::ostream& os) {
    constexpr std::string_view FMT1 = 
"Shape {{"
"    appearance Appearance {{"
"        material Material {{"
"            diffuseColor 0.3 0.5 0.7"
"        }}"
"    }}"
"    geometry IndexedFaceSet {{"
"        coord Coordinate {{"
"            point [";

    constexpr std::string_view FMT2 =
"            ]"
"        }}"
"        coordIndex [";

    constexpr std::string_view FMT3 =
"        ]"
"    }}"
"}}";

    std::vector<double> coords;
    coords.reserve(pts.size() * 2);
    for (const Point& p : pts) {
        coords.push_back(p.x);
        coords.push_back(p.z);
    }

    delaunator::Delaunator d(coords);

    std::println(os, FMT1);
    
    for (Point p : pts) {
        std::println(os, "{} {} {},", p.x, p.y, p.z);
    }

    std::println(os, FMT2);

    for (std::size_t i = 0; i < d.triangles.size(); i += 3) {
        std::println(os, "{}, {}, {}, -1",
                     d.triangles[i],
                     d.triangles[i + 1],
                     d.triangles[i + 2]);
    }

    std::println(os, FMT3);
}


// ┌                                                         ┐
// │                     メインエントリ                      │
// └                                                         ┘

void report_heights(const std::vector<double>& heights) {
    auto freq = calc_frequency(heights);
    for (auto [lower, frequency] : freq) 
        std::println("\"[{:>3}, {:>3}]\", {}", lower, lower + 5, frequency);
}

void test_ground(const MapData& data, std::string_view out_filename) {
    auto ground_points = find_ground(data);
    std::println("マップ内点群数: {}", data.grid.size());
    std::println("地面判定した点群数: {} (~{}%)", ground_points.size(), ground_points.size() * 100 / data.grid.size());

    std::ofstream ofs(out_filename.data());
    std::println(ofs, VRML_HEADER);
    delaunay_triangulation(ground_points, ofs);
}

enum class Command {
    ReportHeights,
    GeneratePoints,
    TestGround,
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
        std::println("\x1b[1;34mtest-ground\x1b[m\t\t地面判定アルゴリズムをテスト");
        std::println("\x1b[1;34mgen-points\x1b[m\t\t地図データから点群を直接出力");

        std::println();
        std::println("\x1b[1;4mオプション\x1b[m\n");
        std::println("\x1b[1;34m-o <ファイル名>\x1b[m\t\t出力ファイル名を指定");
    }

    Command parse_command() const {
        if (command == "report-heights") return Command::ReportHeights;
        if (command == "gen-points") return Command::GeneratePoints;
        if (command == "test-ground") return Command::TestGround;
        throw std::runtime_error(std::format("未知なコマンド名: {}", command));
    }

    void normalize_output_filename() const {
        if (!output_filename.ends_with(".wrl")) output_filename.append(".wrl");
    }
};


int main(int argc, const char** argv) {
    Option opt;
    Command command;
    try {
        opt.parse(argc, argv, true);
        opt.normalize_output_filename();
        command = opt.parse_command();
    } catch (const std::runtime_error& e) {
        std::println(stderr, "\x1b[1;31mエラー:\x1b[m {}\n", e.what());
        opt.help();
        return 1;
    }

    try {
        auto data = read_map(opt.file_path.c_str());
        switch (command) {
            case Command::ReportHeights: report_heights(data.grid); break;
            case Command::GeneratePoints: generate_points(data, opt.output_filename); break;
            case Command::TestGround: test_ground(data, opt.output_filename); break;
        }
    } catch (const std::exception& e) {
        std::println(stderr, "\x1b[1;31mエラー:\x1b[m {}", e.what());
        return 1;
    }

    return 0;
}
