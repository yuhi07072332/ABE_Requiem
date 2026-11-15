#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <print>
#include <queue>
#include <vector>
#include <array>

#include "genk.hpp"

// clang-format off

/// 8近傍の方向ベクトル
static constexpr std::array<Coord, 8> n8_dirs = {{
    {-1,  0},
    {-1,  1},
    { 0,  1},
    { 1,  1},
    { 1,  0},
    { 1, -1},
    { 0, -1},
    {-1, -1},
}};

/// 4近傍の方向ベクトル
static constexpr std::array<Coord, 4> n4_dirs = {{
    {0, -1},
    {-1, 0},
    {0, 1},
    {1, 0}
}};

// clang-format on

static const double INITIAL_GROUND_RATIO = 0.05;
static const double GROUND_HEIGHT_DIFF_MAX = 1.0;
static const double BUILDING_HEIGHT_DIFF_MIN = 2.0;

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

    const double ground_idx = valid_heights.size() * INITIAL_GROUND_RATIO;
    // ground_idx番目にしきい値が来るように処理する
    std::ranges::nth_element(valid_heights, valid_heights.begin() + ground_idx);
    return valid_heights[ground_idx];
}

// NOTE: アルゴリズム修正案
// * 1段階目では地面、建物輪郭(仮)、その他の分類し、地面のみ拡張
// * 2段階目ではBuildingOutlineから探索し、建物を拡張
//      * 輪郭再判定
//      * 建物内部判定をし、まとまった1つ建物をBuildingに格納
//      * 建物の幅または横が1しかない場合、すべてOtherに再判定する
// * 3段階目ではOtherから同様に探索・判定

// NOTE: 

// BFSの状態を表す
enum State : std::uint8_t{
    NotVisited = 0,
    Visited = 1,
    Decided = 2,
};

/// @brief UnknownからBFSで地面・建物輪郭(仮)・その他に分類
static void bfs_from_unknown(MapData& data,
                            std::vector<std::vector<State>>& state) {
    std::queue<Coord> queue;

    // 8近傍の地面と判定された点の内、高さが最小なものを基準としてcのTypeを判定する
    // 8近傍内に地面がない場合はエラーとなる
    auto judge = [&](Coord c) -> Type {
        double min_ground = std::numeric_limits<double>::max();
        for (Coord dc : n8_dirs) {
            Coord nc = c + dc;
            if (!data.contains(nc) 
                || data.is_missing(nc) 
                || data.type_at(nc) != Type::Ground) 
                continue;
            min_ground = std::min(data.at(nc), min_ground);
        }
        
        assert(min_ground != std::numeric_limits<double>::max());

        const double diff = data.at(c) - min_ground;

        if (diff >= BUILDING_HEIGHT_DIFF_MIN) return Type::BuildingOutline;
        if (diff <= GROUND_HEIGHT_DIFF_MAX) return Type::Ground;
        return Type::Other;
    };

    // 1. 初期Groundをqueueに格納
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            const Coord c{i, j};
            if (data.type_at(c) == Type::Ground) {
                queue.push(c);
                state[i][j] = State::Decided;
            }
        }
    }

    // 2. GroundからBFSしていく
    while (!queue.empty()) {
        const Coord ground = queue.front();
        queue.pop();

        for (Coord dc: n8_dirs) {
            const Coord nc = ground + dc;
            if (!data.contains(nc) 
                || state[nc.i][nc.j] == State::Visited
                || state[nc.i][nc.j] == State::Decided
                || data.is_missing(nc)) continue;
            if (data.type_at(nc) != Type::Unknown) continue;

            const Type nc_type = judge(nc);

            if (nc_type == Type::Ground) {
                queue.push(nc);
                state[nc.i][nc.j] = State::Decided;
            } else {
                // 地面以外の点は仮決定
                state[nc.i][nc.j] = State::Visited;
            }

            data.type_at(nc) = nc_type;
        }
    }
}

/// @brief OutlineからBFSで建物を拡張
static void bfs_from_outline(MapData& data,
                            std::vector<std::vector<State>>& state) {
    auto judge_type = [&](Coord c) -> Type {
        for (Coord dc : n4_dirs) {
            Coord nc = c + dc;
            if (!data.contains(nc)) continue;
            if (data.type_at(nc) == Type::Ground 
                || data.type_at(nc) == Type::Other) {
                return Type::BuildingOutline;
            }
        }
        return Type::Building;
    };

    // 輪郭からBFSで穴埋めを行う
    // 輪郭周辺にvisitしたことない点が存在しないときはnullを返す
    auto bfs_from = [&](Coord start) -> std::optional<Building> {
        assert(data.type_at(start) == Type::BuildingOutline);

        std::optional<Building> building = Building{};
        std::queue<Coord> queue;
        queue.push(start);
        building->append(start);

        while (!queue.empty()) {
            Coord c = queue.front();
            queue.pop();

            for (Coord dc : n4_dirs) {
                Coord nc = c + dc;
                if (!data.contains(nc) 
                    || state[nc.i][nc.j] == State::Decided) continue;
                if (data.type_at(nc) != Type::Unknown) continue;

                Type nc_type = judge_type(nc);
                data.type_at(nc) = nc_type;
                if (nc_type == Type::Building) building->has_inner = true;
                state[nc.i][nc.j] = State::Decided;
                building->append(c);
                queue.push(nc);
            }
        }

        if (building->coords.size() < 2) return std::nullopt;
        return building;
    };

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if (state[i][j] == State::Decided) continue;
            if (data.type_at(i, j) != Type::BuildingOutline) continue;

            auto building = bfs_from({i, j});
            if (!building) continue;

            const int rows = building->bottom_right.i - building->top_left.i + 1;
            const int cols = building->bottom_right.j - building->top_left.j + 1;
            const int area = rows * cols;

            if (building->has_inner && rows != 1 && cols != 1 && area >= 10) {
                data.buildings.emplace_back(std::move(*building));
                continue;
            }

            // 建物がvalidではない
            for (Coord c : building->coords) {
                data.type_at(c) = Type::Other;
            }
        }
    }
}

/// @brief 地面と接している未確定領域をBFSで探索し、地面・建物・その他に分類する
[[maybe_unused]] static void propagate_from_ground(MapData& data,
                                  std::vector<std::vector<bool>>& decided) {
    const int rows = data.rows;
    const int cols = data.cols;

    std::queue<Coord> q;
    std::vector<std::vector<bool>> in_queue(rows, std::vector<bool>(cols, false));

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
[[maybe_unused]] static void fill_building_interiors(MapData& data,
                                    std::vector<std::vector<bool>>& decided) {
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
    std::size_t building_outline_cnt = 0;
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
                case Type::BuildingOutline:
                    ++building_outline_cnt;
                    break;
                case Type::Other:
                    ++other_cnt;
                    break;
                default:
                    break;
            }
        }
    }

    std::println("分類完了 (初期地面しきい値: {:.2f}m)", threshold);
    std::println("  建物数: {}\n", data.buildings.size());

    std::println("  地面(Ground):   {} 点", ground_cnt);
    std::println("  建物(Building): {} 点", building_cnt);
    std::println("  輪郭(BuildingOutline): {} 点", building_outline_cnt);
    std::println("  その他(Other):  {} 点", other_cnt);
}

/// @brief 標高データをもとに地図の各点を地面・建物・その他に分類する
void classify_map(MapData& data) {
    const double threshold = calc_initial_ground_threshold(data);
    if (std::isnan(threshold))
        throw std::runtime_error("classify_map(): 有効な高さデータがありません。");

    std::vector<std::vector<State>> state(data.rows,
                                           std::vector<State>(data.cols, State::NotVisited));

    // 1. 下位5%の点を初期「地面」に設定
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if (data.is_missing(i, j)) {
                state[i][j] = State::Decided;
            } else if (data.at(i, j) <= threshold) {
                data.type_at(i, j) = Type::Ground;
                state[i][j] = State::Decided;
            }
        }
    }

    bfs_from_unknown(data, state);
    bfs_from_outline(data, state);

    print_classification_summary(data, threshold);
    /*
    const int rows = data.rows;
    const int cols = data.cols;

    data.types.assign(rows * cols, Type::Unknown);


    std::vector<std::vector<bool>> decided(rows, std::vector<bool>(cols, false));

    // 2. 地面の8近傍から幅優先探索で地面・建物・その他を判定
    propagate_from_ground(data, decided);

    // 3. 建物の外周から屋根の内側へ建物判定を広げる
    fill_building_interiors(data, decided);

    // 4. 結果の集計を表示
    print_classification_summary(data, threshold);
    */
}
