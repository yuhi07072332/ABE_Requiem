#include <algorithm>
#include <limits>
#include <print>
#include <cstdint>
#include <array>
#include <queue>
#include <mdspan>

#include "third_party/decoterm.hpp"
#include "genk.hpp"

template <typename T>
using Matrix = std::mdspan<T, std::dextents<std::size_t, 2>>;

// std::vector<bool>避ける用
using Bool = std::int8_t;

struct Building {
    std::vector<Coord> coords;
    Coord top_left = {
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max()
    };
    Coord bottom_right = {0, 0};

    void push_back(Coord c) {
        coords.push_back(c);
        top_left.i = std::min(top_left.i, c.i);
        top_left.j = std::min(top_left.j, c.j);
        bottom_right.i = std::max(bottom_right.i, c.i);
        bottom_right.j = std::max(bottom_right.j, c.j);
    }
};

/// @brief 建物の輪郭検出を行う
static void find_building_outline(MapData& data) {
    constexpr std::array<Coord, 4> dirs = {
        Coord {0, -1},
        Coord {-1, 0},
        Coord {0, 1},
        Coord {1, 0}
    };

    for (int i = 0 ; i < data.rows; ++i) {
        for (int j = 0 ; j < data.cols; ++j) {
            if (data.type_at(i, j) != Type::Building) continue;
            for (Coord dc : dirs) {
                Coord nc = {i + dc.i, j + dc.j};

                if (nc.i < 0 || nc.i >= data.rows || nc.j < 0 || nc.j >= data.cols) {
                    data.type_at(i, j) = Type::BuildingOutline;
                    break;
                }

                if (data.type_at(nc) != Type::Building 
                    && data.type_at(nc) != Type::BuildingOutline) {
                    data.type_at(i, j) = Type::BuildingOutline;
                    break;
                }
            }
        }
    }
}

/// @brief startから建物をBFS探索していく
static Building bfs_building(const MapData& data, Matrix<Bool> visited, Coord start) {
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

            if (visited[nc.i, nc.j]) continue;

            if (data.type_at(nc) == Type::Building 
                || data.type_at(nc) == Type::BuildingOutline) {
                visited[nc.i, nc.j] = true;
                queue.push(nc);
            }
        }
    }

    return building;
}

/// @brief 建物(輪郭と内部)と判定された点群の領域を建物ごとに分別する
///
/// @details BFSで探索を行う
static std::vector<Building> ccl_buildings(const MapData& data) {
    std::vector<Building> buildings;
    std::vector<Bool> visit = std::vector<Bool>(data.rows * data.cols, false);
    Matrix<Bool> visited{visit.data(), data.rows, data.cols};

    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if (visited[i, j] || data.type_at(i, j) != Type::BuildingOutline)
                continue;
            buildings.emplace_back(bfs_building(data, visited, {i, j}));
        }
    }

    return buildings;
}

// ──────────────────────────────── test ─────────────────────────────

static constexpr int TEST_DATA_SIZE = 32;

/// テスト用仮データ
static consteval auto test_type_grid() {
    constexpr std::array<std::uint32_t, TEST_DATA_SIZE> type_grid = {
    0b00000000000000000000000000000000,
    0b00000000000000000000000000000000,
    0b00111111100000000000000010000000,
    0b00111111100000000000000010000000,
    0b00111111100000000000000111000000,
    0b00111111100000000000000111000000,
    0b00111111100000000000001111100000,
    0b00111111100000000000001111100000,
    0b00111111100000000000011111110000,
    0b00000000000000000000011111110000,
    0b00000000000000000000111111111000,
    0b00000000000000000000000000000000,
    0b00000000000000000000000000000000,
    0b00111100000000000000001111000000,
    0b00111100000000000000111111110000,
    0b00111100000000000001111111000000,
    0b00111100000000000001111111000000,
    0b00111100000000000000111111111100,
    0b00111111111110000001111111111100,
    0b00111111111110000000011111111000,
    0b00111111111110000000000111110000,
    0b00111111111110000000000000000000,
    0b00000000000000000001111111111100,
    0b00000000000000000111111111111100,
    0b00000000000000001111000000011110,
    0b00000000000000001110001110001110,
    0b00000000000000001110011111001110,
    0b00000000000000001110001111001110,
    0b00000000000000001111000000011110,
    0b00000000000000000111111111111100,
    0b00000000000000000001111111110000,
    0b00000000000000000000000000000000,
    };

    std::array<Type, TEST_DATA_SIZE * TEST_DATA_SIZE> converted_arr;
    Matrix<Type> converted{converted_arr.data(), TEST_DATA_SIZE, TEST_DATA_SIZE};

    for (int i = 0; i < TEST_DATA_SIZE; ++i) {
        for (int j = 0; j < TEST_DATA_SIZE; ++j) {
            int type_bit = type_grid[i] >> (TEST_DATA_SIZE - 1 - j);
            type_bit &= 0b1;
            converted[i, j] = 
                type_bit ? Type::Building : Type::Ground;
        }
    }

    return converted_arr;
}

static void print_type_grid(Matrix<Type> type_grid, Coord top_left, Coord bottom_right) {
    using deco::fg;

    for (int i = top_left.i; i <= bottom_right.i; ++i) {
        for (int j = top_left.j; j <= bottom_right.j; ++j) {
            switch (type_grid[i, j]) {
                case Type::Building: std::print("{}", 'B' | fg(deco::bright_blue)); break;
                case Type::BuildingOutline: std::print("O"); break;
                default: std::print("{}", '.' | deco::dim);
            }
        }
        std::print("\n");
    }
}

static void print_type_grid(Matrix<Type> type_grid) {
    const Coord bottom_right = {
        static_cast<int>(type_grid.extent(0) - 1),
        static_cast<int>(type_grid.extent(1) - 1),
    };
    print_type_grid(type_grid, {0, 0}, bottom_right);
}

/// 輪郭検出アルゴリズムをテスト
void test_find_building_outline() {
    constexpr int rows = TEST_DATA_SIZE;
    constexpr int cols = TEST_DATA_SIZE;

    constexpr auto type_grid_arr = test_type_grid();

    MapData data = MapData{
        .grid = std::vector<double>(rows * cols, 0),
        .types = std::vector<Type>(type_grid_arr.begin(), type_grid_arr.end()),
        .x = 0,
        .z = 0,
        .rows = rows,
        .cols = cols,
    };
    Matrix<Type> type_grid{data.types.data(), TEST_DATA_SIZE, TEST_DATA_SIZE};

    std::println("{}", "初期: " | deco::bold);
    print_type_grid(type_grid);

    std::println("{}", "\n輪郭検出: " | deco::bold);
    find_building_outline(data);
    print_type_grid(type_grid);

    auto buildings = ccl_buildings(data);
    std::println("{}", "\n建物検出:" | deco::bold);
    std::println("建物数: {}", buildings.size() | deco::fg(deco::bright_yellow));

    int count = 1;
    for (auto& building : buildings) {
        std::println("建物{}: ({}, {}) ~ ({}, {}):\n",
                     count++,
                     building.top_left.i,
                     building.top_left.j,
                     building.bottom_right.i,
                     building.bottom_right.j);

        print_type_grid(type_grid, building.top_left, building.bottom_right);
        std::println("");
    }

}
