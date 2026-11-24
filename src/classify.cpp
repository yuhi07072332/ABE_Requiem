#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <print>
#include <queue>
#include <set>
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

enum State : std::uint8_t{
    UnVisited = 0,
    Visited = 1,
    Decided = 2,
};

/// @brief UnknownからBFSでGroundを拡張し, BuildingとOtherを仮判定
static void bfs_ground(MapData& data,
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

        if (diff >= BUILDING_HEIGHT_DIFF_MIN) return Type::Building;
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

/// @brief Buildingに囲まれたUnknown成分のうち、周辺地面とほぼ同じ高さのものをGroundにする
///
/// Unknown成分に接するBuilding連結成分の外側にあるGroundの中央値を
/// 基準にする。これにより、建物に囲まれて通常のbfs_ground()では
/// 到達できない中庭などをGroundとして扱える。
static void classify_enclosed_ground(
    MapData& data,
    std::vector<std::vector<State>>& state) {
    std::vector<std::vector<bool>> visited(
        data.rows,
        std::vector<bool>(data.cols, false)
    );

    auto median = [](std::vector<double> values) {
        const auto middle = values.begin() + values.size() / 2;
        std::ranges::nth_element(values, middle);
        return *middle;
    };

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            const Coord start{i, j};
            if (visited[i][j] || data.type_at(start) != Type::Unknown) continue;

            // 1. Unknownを探索し格納
            std::vector<Coord> unknowns;
            std::set<Coord> adjacent_buildings; // Unknownと隣接しているBuilding
            std::queue<Coord> unknown_queue;
            unknown_queue.push(start);
            visited[i][j] = true;

            while (!unknown_queue.empty()) {
                const Coord c = unknown_queue.front();
                unknown_queue.pop();
                unknowns.push_back(c);

                for (Coord dc : n4_dirs) {
                    const Coord nc = c + dc;
                    if (!data.contains(nc)) continue;

                    if (data.type_at(nc) == Type::Building) {
                        adjacent_buildings.insert(nc);
                    }

                    if (visited[nc.i][nc.j]
                        || data.type_at(nc) != Type::Unknown) {
                        continue;
                    }
                    visited[nc.i][nc.j] = true;
                    unknown_queue.push(nc);
                }
            }

            // Buildingに囲まれていないUnknownはbfs_building()で処理する
            if (adjacent_buildings.empty()) continue;

            // 2. Unknownと隣接しているBuildingを探索し、8近傍のGroundの高さを記録
            std::set<Coord> buildings;
            std::queue<Coord> building_queue;
            for (Coord c : adjacent_buildings) {
                if (buildings.insert(c).second) building_queue.push(c);
            }

            std::vector<double> surrounding_ground_heights; // Buildingの8近傍のGroundの高さ
            while (!building_queue.empty()) {
                const Coord c = building_queue.front();
                building_queue.pop();

                for (Coord dc : n8_dirs) {
                    const Coord nc = c + dc;
                    if (!data.contains(nc)) continue;

                    if (data.type_at(nc) == Type::Ground) {
                        surrounding_ground_heights.push_back(data.at(nc));
                    }
                }

                for (Coord dc : n4_dirs) {
                    const Coord nc = c + dc;
                    if (!data.contains(nc)) continue;
                    if (data.type_at(nc) == Type::Building
                        && buildings.insert(nc).second) {
                        building_queue.push(nc);
                    }
                }
            }

            if (surrounding_ground_heights.empty()) continue;

            // 3. 格納されたUnknownとGroundの高さの差を比較し、GROUND_HEIGHT_DIFF_MAX
            // 未満の場合はGroundに判定
            std::vector<double> unknown_heights;
            unknown_heights.reserve(unknowns.size());
            for (Coord c : unknowns) {
                unknown_heights.push_back(data.at(c));
            }

            const double ground_height = median(surrounding_ground_heights);
            const double unknown_height = median(unknown_heights);
            if (std::abs(unknown_height - ground_height)
                >= GROUND_HEIGHT_DIFF_MAX) {
                continue;
            }

            for (Coord c : unknowns) {
                data.type_at(c) = Type::Ground;
                state[c.i][c.j] = State::Decided;
            }
        }
    }
}

/// @brief BuildingからBFSで建物を拡張
///
/// @returns buildings
static std::vector<Building> bfs_building(MapData& data,
                         std::vector<std::vector<State>>& state) {
    // BuildingからBFSで穴埋めを行う
    auto bfs_from = [&](Coord start) -> Building {
        assert(data.type_at(start) == Type::Building);

        Building building;
        std::queue<Coord> queue;
        queue.push(start);
        state[start.i][start.j] = State::Decided;
        building.append(start);

        while (!queue.empty()) {
            Coord c = queue.front();
            queue.pop();

            for (Coord dc : n4_dirs) {
                Coord nc = c + dc;
                if (!data.contains(nc) 
                    || state[nc.i][nc.j] == State::Decided) continue;
                if (data.type_at(nc) != Type::Building
                    && data.type_at(nc) != Type::Unknown) continue;

                

                data.type_at(nc) = Type::Building;
                state[nc.i][nc.j] = State::Decided;
                building.append(nc);
                queue.push(nc);
            }
        }

        return building;
    };

    std::vector<Building> buildings;

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if (state[i][j] == State::Decided) continue;
            if (data.type_at(i, j) != Type::Building) continue;

            Building building = bfs_from(Coord {i, j});
            // buildingが1点のみの場合はOtherへ変換する
            if (building.coords.size() > 1) 
                buildings.push_back(std::move(building));
            else
                data.type_at(i, j) = Type::Other;
        }
    }

    return buildings;
}

/// @brief 全てのBuildingに対して輪郭検出を行い、妥当かどうか判定し、MapData::buildingsに格納
///
/// @details
/// * 妥当でなかった場合はdata.buildingsに格納されず、その建物に含まれる
/// すべての点をOtherに変更する
static void validate_buildings(std::vector<Building>& buildings, MapData& data) {
    //Coordが輪郭かを判定する
    auto is_outline = [&](Coord c, const Building& building) -> bool {
        for (Coord dc : n4_dirs) {
            Coord nc = c + dc;
            if (!data.contains(nc)
                || !building.coords.contains(nc)) return true;
        }
        return false;
    };

    /// 輪郭の8近傍内にBuildingがない場合、その点を削除し, typeをOtherに変更
    auto validate_outline = [&](Building& building) {
        std::vector<Coord> coords_to_erase;
        for (Coord c : building.coords) {
            if (data.type_at(c) != Type::BuildingOutline) continue;
            bool has_inner_neibor = false;

            for (Coord dc : n8_dirs) {
                Coord nc = c + dc;
                if (!data.contains(nc)) continue;
                if (data.type_at(nc) == Type::Building) {
                    has_inner_neibor = true;
                    building.has_inner = true;
                    break;
                };
            }

            if (!has_inner_neibor) {
                coords_to_erase.push_back(c);
                data.type_at(c) = Type::Other;
            }
        } 

        for (Coord c : coords_to_erase) {
            building.coords.erase(c);
        }
    };

    // 建物が妥当か否かを判定する
    auto is_valid = [&](Building& building) -> bool {
        const int rows = building.bottom_right.i - building.top_left.i + 1;
        const int cols = building.bottom_right.j - building.top_left.j + 1;
        const int area = building.coords.size();
        return building.has_inner && rows > 2 && cols > 2 && area >= 10;
    };

    for (Building& building : buildings) {
        for (Coord c : building.coords) {
            if (is_outline(c, building)) data.type_at(c) = Type::BuildingOutline;
        }

        validate_outline(building);

        if (!is_valid(building)) {
            for (Coord c : building.coords) {
                data.type_at(c) = Type::Other;
            }
            continue;
        }
        data.buildings.push_back(std::move(building));
    }
}

/// @brief 分類結果をMapDataに格納し、標準出力に表示する
static void print_classification_summary(MapData& data, double threshold) {
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

    data.ground_coords = ground_cnt;
    data.building_coords = building_cnt;
    data.building_outline_coords = building_outline_cnt;
    data.other_coords = other_cnt;

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
                                           std::vector<State>(data.cols, State::UnVisited));

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

    bfs_ground(data, state);
    classify_enclosed_ground(data, state);
    bfs_ground(data, state);
    auto buildings = bfs_building(data, state);

    validate_buildings(buildings, data);

    print_classification_summary(data, threshold);
}
