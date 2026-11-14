#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <print>
#include <queue>
#include <vector>

#include "genk.hpp"

// std::vector<bool> を避けるためのエイリアス
using Bool = std::int8_t;

// clang-format off

/// 8近傍の方向ベクトル
static const std::vector<Coord> n8_dirs = {
    {-1,  0},
    {-1,  1},
    { 0,  1},
    { 1,  1},
    { 1,  0},
    { 1, -1},
    { 0, -1},
    {-1, -1},
};

// clang-format on

static const double INITIAL_GROUND_RATIO = 0.05;
static const double GROUND_HEIGHT_DIFF_MAX = 1.0;
static const double BUILDING_HEIGHT_DIFF_MIN = 4.0;

/// @brief 8近傍内にある地面(Type::Ground)の最小高さを取得する
/// @return 地面が見つかった場合はその最小高さ、見つからない場合は NaN
static double find_neighbor_ground_height(const MapData& data, Coord c) {
    double min_ground_h = std::numeric_limits<double>::infinity();
    bool found = false;

    for (Coord dc : n8_dirs) {
        Coord nc = c + dc;
        if (!data.contains(nc) || data.is_missing(nc)) continue;

        if (data.type_at(nc) == Type::Ground) {
            min_ground_h = std::min(min_ground_h, data.at(nc));
            found = true;
        }
    }

    return found ? min_ground_h : std::numeric_limits<double>::quiet_NaN();
}

/// @brief 有効な標高データの下位5%のしきい値を算出する
static double calc_initial_ground_threshold(const MapData& data) {
    std::vector<double> valid_heights;
    valid_heights.reserve(data.rows * data.cols);

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if (!data.is_missing(i, j)) {
                valid_heights.push_back(data.at(i, j));
            }
        }
    }

    if (valid_heights.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const std::ptrdiff_t ground_idx =
        valid_heights.size() * INITIAL_GROUND_RATIO;
    std::ranges::nth_element(valid_heights, valid_heights.begin() + ground_idx);
    return valid_heights[ground_idx];
}

/// @brief 地面と接している未確定領域をBFSで探索し、地面・建物・その他に分類する
static void propagate_from_ground(MapData& data,
                                  std::vector<std::vector<Bool>>& decided) {
    const int rows = data.rows;
    const int cols = data.cols;

    std::queue<Coord> q;
    std::vector<std::vector<Bool>> in_queue(rows, std::vector<Bool>(cols, false));

    bool updated = true;
    while (updated) {
        updated = false;

        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                if (decided[i][j] || in_queue[i][j]) continue;

                Coord start{i, j};
                if (std::isnan(find_neighbor_ground_height(data, start))) {
                    continue;
                }

                q.push(start);
                in_queue[i][j] = true;

                while (!q.empty()) {
                    Coord c = q.front();
                    q.pop();
                    in_queue[c.i][c.j] = false;

                    if (decided[c.i][c.j]) continue;

                    const double ground_h = find_neighbor_ground_height(data, c);
                    if (std::isnan(ground_h)) continue;

                    const double diff = data.at(c) - ground_h;
                    if (diff <= GROUND_HEIGHT_DIFF_MAX) {
                        data.type_at(c) = Type::Ground;
                    } else if (diff >= BUILDING_HEIGHT_DIFF_MIN) {
                        data.type_at(c) = Type::Building;
                    } else {
                        data.type_at(c) = Type::Unknown;
                    }

                    decided[c.i][c.j] = true;
                    updated = true;

                    for (Coord dc : n8_dirs) {
                        Coord nc = c + dc;
                        if (!data.contains(nc) || decided[nc.i][nc.j]
                            || in_queue[nc.i][nc.j]) {
                            continue;
                        }
                        if (!std::isnan(find_neighbor_ground_height(data, nc))) {
                            q.push(nc);
                            in_queue[nc.i][nc.j] = true;
                        }
                    }
                }
            }
        }
    }
}

/// @brief 建物の外周から屋根の内側へBFSで建物判定を広げる（中抜き防止）
static void fill_building_interiors(MapData& data,
                                    std::vector<std::vector<Bool>>& decided) {
    const int rows = data.rows;
    const int cols = data.cols;

    std::vector<std::vector<double>> base_ground(
        rows,
        std::vector<double>(cols, std::numeric_limits<double>::quiet_NaN()));

    std::queue<Coord> q;
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (data.type_at(i, j) == Type::Building) {
                base_ground[i][j] = find_neighbor_ground_height(data, {i, j});
                q.push({i, j});
            }
        }
    }

    while (!q.empty()) {
        Coord c = q.front();
        q.pop();

        const double ground_h = base_ground[c.i][c.j];
        if (std::isnan(ground_h)) continue;

        for (Coord dc : n8_dirs) {
            Coord nc = c + dc;
            if (!data.contains(nc) || decided[nc.i][nc.j]) continue;

            const double diff = data.at(nc) - ground_h;
            if (diff >= BUILDING_HEIGHT_DIFF_MIN) {
                data.type_at(nc) = Type::Building;
                decided[nc.i][nc.j] = true;
                base_ground[nc.i][nc.j] = ground_h;
                q.push(nc);
            } else if (diff > GROUND_HEIGHT_DIFF_MAX) {
                data.type_at(nc) = Type::Unknown;
                decided[nc.i][nc.j] = true;
            }
        }
    }
}

/// @brief 分類結果の点数を集計して標準出力に表示する
static void print_classification_summary(const MapData& data, double threshold) {
    std::size_t ground_cnt = 0;
    std::size_t building_cnt = 0;
    std::size_t other_cnt = 0;

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if (data.is_missing(i, j)) continue;

            switch (data.type_at(i, j)) {
                case Type::Ground:
                    ++ground_cnt;
                    break;
                case Type::Building:
                    ++building_cnt;
                    break;
                default:
                    ++other_cnt;
                    break;
            }
        }
    }

    std::println("分類完了 (初期地面しきい値: {:.2f}m)", threshold);
    std::println("  地面(Ground):   {} 点", ground_cnt);
    std::println("  建物(Building): {} 点", building_cnt);
    std::println("  その他(Other):  {} 点", other_cnt);
}

/// @brief 標高データをもとに地図の各点を地面・建物・その他に分類する
void classify_map(MapData& data) {
    const int rows = data.rows;
    const int cols = data.cols;

    data.types.assign(rows * cols, Type::Unknown);

    const double threshold = calc_initial_ground_threshold(data);
    if (std::isnan(threshold)) {
        std::println("有効な高さデータがありません。");
        return;
    }

    std::vector<std::vector<Bool>> decided(rows, std::vector<Bool>(cols, false));

    // 1. 下位5%の点を初期「地面」に設定
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            if (data.is_missing(i, j)) {
                decided[i][j] = true;
            } else if (data.at(i, j) <= threshold) {
                data.type_at(i, j) = Type::Ground;
                decided[i][j] = true;
            }
        }
    }

    // 2. 地面の8近傍から幅優先探索で地面・建物・その他を判定
    propagate_from_ground(data, decided);

    // 3. 建物の外周から屋根の内側へ建物判定を広げる
    fill_building_interiors(data, decided);

    // 4. 結果の集計を表示
    print_classification_summary(data, threshold);
}
