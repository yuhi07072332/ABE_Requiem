#include <algorithm>
#include <cassert>
#include <limits>

#include "genk.hpp"
#include <third_party/decoterm.hpp>

/// @brief 1ブロック内の間引き処理（このファイル内でのみ使用するため static）
///
/// @param filter Coordを受け取ってboolを返す関数。trueを返された場合のみ処理の対象となる
/// @returns 削減数
template <typename F>
static unsigned int downsample_block(MapData& data,
                             int start_i,
                             int start_j,
                             int block_size,
                             double height_threshold,
                             const F& filter) {
    int end_i = std::min(start_i + block_size, data.rows);
    int end_j = std::min(start_j + block_size, data.cols);

    double min_y = std::numeric_limits<double>::infinity();
    double max_y = -std::numeric_limits<double>::infinity();
    int rep_i = -1;
    int rep_j = -1;

    // ブロック内の有効な点の最小・最大高さと、代表点を探す
    for (int i = start_i; i < end_i; ++i) {
        for (int j = start_j; j < end_j; ++j) {
            if (data.is_missing(i, j)) continue;
            if (!filter(Coord{i, j})) continue;

            double y = data.at(i, j);
            min_y = std::min(min_y, y);
            max_y = std::max(max_y, y);

            if (rep_i == -1) {
                rep_i = i;
                rep_j = j;
            }
        }
    }

    // 有効点がない、または高さの差が閾値より大きい（変化が大きい）場合は消さない
    if (rep_i == -1 || (max_y - min_y) > height_threshold) {
        return 0;
    }

    // 数値が近い集まりとみなし、代表点以外を NaN にして消す
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    unsigned int count = 0;
    for (int i = start_i; i < end_i; ++i) {
        for (int j = start_j; j < end_j; ++j) {
            if (i == rep_i && j == rep_j) continue;
            if (data.is_missing(i, j)) continue;
            if (!filter(Coord{i, j})) continue;
            data.at(i, j) = nan_val;
            count ++;
        }
    }

    return count;
}

/// @returns 削減した点の数
template <typename F>
unsigned int downsample_flat_points(MapData& data,
                            int block_size,
                            double height_threshold,
                            const F& filter,
                            Coord top_left,
                            Coord bottom_right) {
    if (block_size <= 1) return 0;

    unsigned int count = 0;
    for (int i = top_left.i; i <= bottom_right.i; i += block_size) {
        for (int j = top_left.j; j <= bottom_right.j; j += block_size) {
            count += downsample_block(
                data,
                i,
                j,
                block_size,
                height_threshold,
                filter
            );
        }
    }
    return count;
}

/// 地面の平滑化を行う
void smooth_ground(MapData& data, int max_block, double init_threshold) {
    int before = data.ground_coords;
    int erased = 0;

    for (int i = 1; i <= max_block; i *= 2) {
        const int block_size = 2 * i;
        const double threshold = init_threshold * i;
        erased += downsample_flat_points(
            data,
            block_size,
            threshold,
            [&](Coord c){ return data.type_at(c) == Type::Ground; },
            {0, 0},
            {data.rows - 1, data.cols - 1}
        );
    }

    int after = before - erased;

    data.ground_coords = after;

    // std::println("地面平滑化: {} -> {} ({}%)",
    //              before,
    //              after,
    //              -(erased * 100 / before) | deco::fg(deco::bright_yellow));
}

/// 建物の平滑化を行う
void smooth_buildings(MapData& data, int max_block, double init_threshold) {
    int before = data.building_coords;
    int erased = 0;

    for (const auto& building : data.buildings) {
        for (int i = 1; i <= max_block; i *= 2) {
            const int block_size = 2 * i;
            const double threshold = init_threshold * i;
            erased += downsample_flat_points(
                data,
                block_size,
                threshold,
                [&](Coord c){ 
                    return building.coords.contains(c) && data.type_at(c) == Type::Building;
                },
                building.top_left,
                building.bottom_right
            );
        }
    }

    int after = before - erased;

    data.building_coords = after;

    // std::println("建物平滑化: {} -> {} ({}%)",
    //              before,
    //              after,
    //              -(erased * 100 / before) | deco::fg(deco::bright_yellow));
}
