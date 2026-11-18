#include <algorithm>
#include <limits>

#include "genk.hpp"

// 1ブロック内の間引き処理（このファイル内でのみ使用するため static）
static void downsample_block(MapData& data, int start_i, int start_j, int block_size, double height_threshold) {
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
        return;
    }

    // 数値が近い集まりとみなし、代表点以外を NaN にして消す
    const double nan_val = std::numeric_limits<double>::quiet_NaN();
    for (int i = start_i; i < end_i; ++i) {
        for (int j = start_j; j < end_j; ++j) {
            if (i == rep_i && j == rep_j) continue;
            data.grid[i * data.cols + j] = nan_val;
            // 必要に応じて type もリセットする場合は以下を有効化
            // data.type_at(i, j) = Type::Unknown;
        }
    }
}

void downsample_flat_points(MapData& data, int block_size, double height_threshold) {
    if (block_size <= 1) return;

    for (int i = 0; i < data.rows; i += block_size) {
        for (int j = 0; j < data.cols; j += block_size) {
            downsample_block(data, i, j, block_size, height_threshold);
        }
    }
}
