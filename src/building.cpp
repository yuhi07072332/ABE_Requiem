#include <print>
#include <cstdint>
#include <array>
#include <optional>
#include <queue>

#include "third_party/decoterm.hpp"
#include "genk.hpp"

using Building = std::vector<Coord>;

/// @brief 建物の輪郭検出を行う
static void find_building_outline(MapData& data) {
    for (int i = 0 ; i < data.rows; ++i) {
        for (int j = 0 ; j < data.cols; ++j) {
            if (data.type_at(i, j) != Type::Building) continue;
            std::array<std::optional<Coord>, 4> checks = {
                i > 0 ? std::make_optional<Coord>(i - 1, j) : std::nullopt,
                j < data.cols - 1 ? std::make_optional<Coord>(i, j + 1) : std::nullopt,
                i < data.rows - 1 ? std::make_optional<Coord>(i + 1, j) : std::nullopt,
                j > 0 ? std::make_optional<Coord>(i, j - 1) : std::nullopt,
            };

            for (auto coord : checks) {
                if (!coord) {
                    data.type_at(i, j) = Type::BuildingOutline;
                    break;
                }
                if (data.type_at(*coord) != Type::Building 
                    && data.type_at(*coord) != Type::BuildingOutline) {
                    data.type_at(i, j) = Type::BuildingOutline;
                    break;
                }
            }
        }
    }
}

static Building bfs_buildings(const MapData& data, std::vector<bool>& visited, Coord start) {
    assert(data.type_at(start) == Type::BuildingOutline);
    constexpr std::array<Coord, 4> dirs = {
        Coord {0, -1},
        Coord {-1, 0},
        Coord {0, 1},
        Coord {1, 0}
    };

    Building building;
    std::queue<Coord> queue;
    queue.push(start);

    while (!queue.empty()) {
        Coord c = queue.front();
        queue.pop();

        building.push_back(c);

        for (Coord dc: dirs) {
            Coord nc = { c.i + dc.i, c.j + dc.j };
            if (nc.i < 0 || nc.i >= data.rows || nc.j < 0 || nc.j >= data.cols)
                continue;

            if (data.type_at(nc) == Type::Building 
                || data.type_at(nc) == Type::BuildingOutline) {
                visited.at(nc.i * data.rows + nc.j) = true;
                queue.push(nc);
            }
        }
    }
}

/// @brief 建物(輪郭と内部)と判定された点群の領域を建物ごとに分別する
///
/// @details BFSで探索を行う
static std::vector<Building> ccl_buildings(const MapData& data) {
    std::vector<Building> buildings;
    std::vector<bool> visited = std::vector<bool>(data.rows * data.cols, false);

    std::queue<Coord> queue;
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {

        }
    }
}

// ──────────────────────────────── test ─────────────────────────────

static constexpr int TEST_DATA_SIZE = 16;

static consteval auto test_type_grid() {
    constexpr std::array<std::uint16_t, TEST_DATA_SIZE> type_grid = {
        0b0001110000000000,
        0b0001110000011100,
        0b0001110000011100,
        0b0001110000011100,
        0b0100000000011100,
        0b0110000011111100,
        0b0111000011111100,
        0b0111100011111100,
        0b0000000000000000,
        0b0011111110000000,
        0b010000011110000,
        0b011011001110000,
        0b010111101110000,
        0b010111001110000,
        0b010000001110000,
        0b011111111110000,
    };

    std::array<Type, TEST_DATA_SIZE * TEST_DATA_SIZE> converted;

    for (int i = 0; i < TEST_DATA_SIZE; ++i) {
        for (int j = 0; j < TEST_DATA_SIZE; ++j) {
            int type_bit = type_grid[i] >> (TEST_DATA_SIZE - 1 - j);
            type_bit &= 0b1;
            converted[i * TEST_DATA_SIZE + j] = 
                type_bit ? Type::Building : Type::Ground;
        }
    }

    return converted;
}

static void print_type_grid(std::vector<Type>& type_grid) {
    using deco::fg;

    for (int i = 0; i < TEST_DATA_SIZE; ++i) {
        for (int j = 0; j < TEST_DATA_SIZE; ++j) {
            switch (type_grid[i * TEST_DATA_SIZE + j]) {
                case Type::Building: std::print("{}", 'B' | fg(deco::bright_blue)); break;
                case Type::BuildingOutline: std::print("O"); break;
                default: std::print("{}", '.' | deco::dim);
            }
        }
        std::print("\n");
    }
}

/// 輪郭検出アルゴリズムをテスト
void test_find_building_outline() {
    constexpr int rows = TEST_DATA_SIZE;
    constexpr int cols = TEST_DATA_SIZE;

    constexpr auto type_grid = test_type_grid();

    MapData data = MapData{
        .grid = std::vector<double>(rows * cols, 0),
        .types = std::vector<Type>(type_grid.begin(), type_grid.end()),
        .x = 0,
        .z = 0,
        .rows = rows,
        .cols = cols,
    };

    std::println("初期: ");
    print_type_grid(data.types);

    find_building_outline(data);
    std::println("\n輪郭検出: ");
    print_type_grid(data.types);
}
