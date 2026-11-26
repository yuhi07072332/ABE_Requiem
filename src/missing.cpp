#include <array>
#include <queue>
#include <vector>
#include <algorithm>

#include "genk.hpp"


/// @brief 欠損点補完
/// @details
/// * 線形探索で有効なデータに隣接している欠損点をキューに入れる
/// * 以降は幅優先探索で欠損点領域の外側から
///   8近傍の有効なデータの中央値を取って欠損点を補完する
/// * 補完した欠損点の8近傍を調べ、欠損点でキューに入っていないものを入れる
void fill_missing(MapData& data) {

    constexpr std::array<Coord, 8> dirs = {
        Coord {-1, -1}, Coord {-1, 0}, Coord {-1, 1},
        Coord {0, -1},                 Coord {0, 1},
        Coord {1, -1}, Coord {1, 0}, Coord {1, 1},
    };

    std::queue<Coord> missing_queue;

    std::vector<std::vector<bool>> queued(
        data.rows,
        std::vector<bool>(data.cols, false)
    );

    std::vector<int> building_ids(data.rows * data.cols, -1);
    for (std::size_t id = 0; id < data.buildings.size(); ++id) {
        for (Coord c : data.buildings[id].coords) {
            building_ids[c.i * data.cols + c.j] = static_cast<int>(id);
        }
    }

    constexpr std::array<Type, 4> tie_priority = {
        Type::Building,
        Type::BuildingOutline,
        Type::Ground,
        Type::Other,
    };


    // 欠損点の座標をキューに入れる
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            Coord current {i, j};

            if (!data.is_missing(current)) continue;

            for (Coord dir : dirs) {
                Coord neighbor = current + dir;
                
                if (neighbor.i < 0 || neighbor.i >= data.rows) continue;
                if (neighbor.j < 0 || neighbor.j >= data.cols) continue;

                if (!data.is_missing(neighbor)) {
                    if (!queued.at(current.i).at(current.j)) {
                        missing_queue.push(current);
                        queued.at(current.i).at(current.j) = true;
                    }
                    break;
                }
            }
        }
    }


    // 幅優先探索で欠損点を補完
    while (!missing_queue.empty()) {
        Coord current = missing_queue.front();
        missing_queue.pop();

        std::array<std::vector<double>, 5> heights_by_type;
        std::array<unsigned int, 5> type_counts{};
        std::vector<double> all_heights;
        std::vector<unsigned int> building_votes(data.buildings.size(), 0);

        for (Coord dir : dirs) {
            Coord neighbor = current + dir;

            if (!data.contains(neighbor)) continue;
            if (data.is_missing(neighbor)) continue;

            const double height = data.at(neighbor);
            const Type type = data.type_at(neighbor);
            all_heights.push_back(height);

            if (type == Type::Unknown) continue;
            const auto type_index = static_cast<std::size_t>(type);
            heights_by_type[type_index].push_back(height);
            ++type_counts[type_index];

            const int building_id =
                building_ids[neighbor.i * data.cols + neighbor.j];
            if (building_id >= 0) {
                ++building_votes[static_cast<std::size_t>(building_id)];
            }
        }

        if (all_heights.empty()) continue;

        Type selected_type = Type::Unknown;
        unsigned int max_count = 0;
        for (Type type : tie_priority) {
            const auto type_index = static_cast<std::size_t>(type);
            if (type_counts[type_index] > max_count) {
                selected_type = type;
                max_count = type_counts[type_index];
            }
        }

        std::vector<double>& heights =
            selected_type == Type::Unknown
                ? all_heights
                : heights_by_type[static_cast<std::size_t>(selected_type)];
        std::sort(heights.begin(), heights.end());
        data.at(current) = heights.at(heights.size() / 2);
        data.type_at(current) = selected_type;

        switch (selected_type) {
            case Type::Ground:
                ++data.ground_coords;
                break;
            case Type::Building:
                ++data.building_coords;
                break;
            case Type::BuildingOutline:
                ++data.building_outline_coords;
                break;
            case Type::Other:
                ++data.other_coords;
                break;
            case Type::Unknown:
                break;
        }

        if (selected_type == Type::Building
            || selected_type == Type::BuildingOutline) {
            int selected_building = -1;
            unsigned int max_building_votes = 0;
            for (std::size_t id = 0; id < building_votes.size(); ++id) {
                if (building_votes[id] > max_building_votes) {
                    selected_building = static_cast<int>(id);
                    max_building_votes = building_votes[id];
                }
            }
            if (selected_building >= 0) {
                data.buildings[static_cast<std::size_t>(selected_building)]
                    .append(current);
                building_ids[current.i * data.cols + current.j] =
                    selected_building;
            }
        }

        
        // 補完した欠損点の8近傍を調べ、欠損点でキューに入っていないものを入れる
        for (Coord dir : dirs) {
            Coord neighbor = current + dir;

            if (neighbor.i < 0 || neighbor.i >= data.rows) continue;
            if (neighbor.j < 0 || neighbor.j >= data.cols) continue;

            if (!data.is_missing(neighbor)) continue;
            if (queued.at(neighbor.i).at(neighbor.j)) continue;

            missing_queue.push(neighbor);
            queued.at(neighbor.i).at(neighbor.j) = true;
        }
    }
}
