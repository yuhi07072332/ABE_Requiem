#include <fstream>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <limits>
#include <utility>

#include "genk.hpp"

struct BoundingBox {
    std::vector<Point> pts;
    int min_x, min_z, max_x, max_z;
};

/// @brief データファイルの点座標を全て読み込む
///
/// @returns 点群配列、x座標, y座標の最大値及び最小値
/// @throws
/// ファイルが開けない場合や読み込み失敗した場合はruntime_errorを投げる
static BoundingBox read_points(const char* data_path) {
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
MapData read_map(std::string_view data_path) {
    auto [pts, min_x, min_z, max_x, max_z] = read_points(data_path.data());
    const int rows = max_z - min_z + 1;
    const int cols = max_x - min_x + 1;

    std::vector<double> grid(rows * cols,
                             std::numeric_limits<double>::quiet_NaN());

    for (Point p : pts) {
        // VRMLのz軸の方向が逆なので読み込み段階で逆にする
        const int i = (rows - 1) - (p.z - min_z);
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

