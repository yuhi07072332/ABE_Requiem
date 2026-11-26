#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <vector>

#include "genk.hpp"

namespace views = std::views;

// clang-format off

/// 4近傍
[[maybe_unused]] constexpr std::array<Coord, 4> n4_dirs = {{
    {0, -1},
    {-1, 0},
    {0, 1},
    {1, 0}
}};

/// 8近傍
constexpr std::array<Coord, 8> n8_dirs = {{
    {-1,  0},
    {-1,  1},
    { 0,  1},
    { 1,  1},
    { 1,  0},
    { 1, -1},
    { 0, -1},
    {-1, -1},
}};

void Building::append(Coord c) {
    coords.insert(c);
    top_left.i = std::min(top_left.i, c.i);
    top_left.j = std::min(top_left.j, c.j);
    bottom_right.i = std::max(bottom_right.i, c.i);
    bottom_right.j = std::max(bottom_right.j, c.j);
}

// clang-format on

// bool is_n4(Coord dir) { return dir.i == 0 || dir.j == 0; }

auto indices(auto a, auto b) { return views::iota(decltype(b)(a), b); }

struct TraceState {
    Coord current;
    Coord backtrack;

    auto operator<=>(const TraceState&) const = default;
};

static std::optional<TraceState> trace_next(TraceState state,
                                            const MapData& data,
                                            const Building& building,
                                            const std::set<Coord>& visited) {
    auto [current, backtrack] = state;

    int b_dir = -1;
    for (auto [i, dc] : n8_dirs | views::enumerate) {
        if (dc + current == backtrack) {
            b_dir = i;
            break;
        }
    }

    if (b_dir == -1) return std::nullopt;

    for (int i : indices(0, n8_dirs.size())) {
        int dir = (b_dir + i + 1) % 8;
        Coord nc = current + n8_dirs.at(dir);

        if (!data.contains(nc) || data.type_at(nc) != Type::BuildingOutline)
            continue;
        if (!building.coords.contains(nc)) continue;
        if (visited.contains(nc)) continue;

        int prev_dir = (dir + 7) % 8;
        return TraceState {.current = nc,
                           .backtrack = current + n8_dirs.at(prev_dir)};
    }

    return std::nullopt;
}

/// @brief Moore neighborhood tracingを使ってbeginから建物の輪郭を
/// 時計回りに探索する
///
/// @details
/// * 輪郭が閉路でなかった場合はnulloptを返す
/// * 閉路の末尾の点は最初の点と一致する
/// 参考:
/// https://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/moore.html
static std::optional<std::vector<Coord>> mnt(const MapData& data,
                                             Coord begin,
                                             const Building& building,
                                             const std::set<Coord>& visited) {
    assert(data.type_at(begin) == Type::BuildingOutline);

    // 輪郭が閉曲線じゃない場合の無限ループを防ぐため、stateが重複しているかどうかも判定する
    std::set<TraceState> visited_state;

    TraceState init_state = {.current = begin,
                             .backtrack = begin - Coord {0, 1}};

    std::vector<Coord> outline_path = {begin};

    std::optional<TraceState> curr_state = init_state;

    while (true) {
        if (auto [_, inserted] = visited_state.insert(*curr_state); !inserted)
            break;
        curr_state = trace_next(*curr_state, data, building, visited);
        if (!curr_state.has_value()) break;
        if (*curr_state == init_state) {
            outline_path.push_back(outline_path.front());
            return outline_path;
        }
        assert(building.coords.contains(curr_state->current));

        outline_path.push_back(curr_state->current);
    }

    return std::nullopt;
}

/// @brief Buildingの全ての輪郭を時計回りの閉路としてoutline_pathsに格納
static void scan_outlines(const MapData& data, Building& building) {
    std::set<Coord> visited;

    for (Coord c : building.coords) {
        if (visited.contains(c)) continue;
        if (data.type_at(c) == Type::BuildingOutline) {
            auto outline = mnt(data, c, building, visited);
            if (!outline) continue;
            for (Coord c : *outline)
                visited.insert(c);
            building.outline_paths.emplace_back(std::move(*outline));
        }
    }
}

/// @brief outline_pathに対して直線の簡略化を行う
[[maybe_unused]] static void simplify_outline_old(
    Building& building, std::vector<Coord>& outline_path) {
    if (outline_path.size() < 2) return;

    std::set<Coord> vertices {outline_path.front()};

    Coord prev_dir = outline_path.at(1) - outline_path.at(0);

    for (std::size_t i = 0; i < outline_path.size(); ++i) {
        const Coord dir =
            outline_path.at((i + 1) % outline_path.size()) - outline_path.at(i);
        if (dir != prev_dir) vertices.insert(outline_path.at(i));
        prev_dir = dir;
    }

    std::vector<Coord> simplified_path;

    for (Coord c : outline_path) {
        if (!vertices.contains(c)) building.coords.erase(c);
        else simplified_path.push_back(c);
    }

    outline_path = std::move(simplified_path);
}

/// @brief RDPアルゴリズムを再帰的に適用して中間座標を削除する
static void rdp(Building& building,
                const std::vector<Coord>& outline_path,
                std::size_t first,
                std::size_t last,
                long double epsilon_squared) {
    const Coord begin = outline_path[first];
    const Coord end = outline_path[last];
    const long double di = static_cast<long double>(end.i) - begin.i;
    const long double dj = static_cast<long double>(end.j) - begin.j;
    const long double length_squared = di * di + dj * dj;

    long double max_distance_squared = -1;
    std::size_t farthest = first;
    for (std::size_t i = first + 1; i < last; ++i) {
        const long double pi =
            static_cast<long double>(outline_path[i].i) - begin.i;
        const long double pj =
            static_cast<long double>(outline_path[i].j) - begin.j;
        long double distance_squared;

        if (length_squared == 0) {
            distance_squared = pi * pi + pj * pj;
        } else {
            const long double cross = pi * dj - pj * di;
            distance_squared = cross * cross / length_squared;
        }

        if (distance_squared > max_distance_squared) {
            max_distance_squared = distance_squared;
            farthest = i;
        }
    }

    if (max_distance_squared > epsilon_squared) {
        rdp(building, outline_path, first, farthest, epsilon_squared);
        rdp(building, outline_path, farthest, last, epsilon_squared);
    } else {
        for (std::size_t i = first + 1; i < last; ++i) {
            building.coords.erase(outline_path[i]);
        }
    }
}

/// @brief 輪郭のパスに対してRDPアルゴリズムで削減を行う
///
/// @details
/// 参考:
/// https://en.wikipedia.org/wiki/Ramer%E2%80%93Douglas%E2%80%93Peucker_algorithm
static void simplify_outlines(Building& building, std::uint32_t epsilon) {
    for (auto& outline_path : building.outline_paths) {
        if (outline_path.size() < 3) continue;

        const long double epsilon_squared =
            static_cast<long double>(epsilon) * epsilon;
        rdp(building,
            outline_path,
            0,
            outline_path.size() - 1,
            epsilon_squared);

        std::vector<Coord> simplified_path;
        simplified_path.reserve(outline_path.size());
        for (Coord c : outline_path) {
            if (building.coords.contains(c)) simplified_path.push_back(c);
        }
        outline_path = std::move(simplified_path);
    }
}

/// @brief 建物に対して輪郭追跡と輪郭削減を行う
void process_buildings(MapData& data) {
    std::println("輪郭追跡...");
    for (auto& building : data.buildings) scan_outlines(data, building);
    std::println("輪郭平滑化...");
    for (auto& building : data.buildings) simplify_outlines(building, 1);
}

// ──────────────────────────────── test ─────────────────────────────
#if 0

static constexpr int TEST_DATA_SIZE = 32;

/// テスト用仮データ
static consteval auto test_type_grid() {
    constexpr std::array<std::uint32_t, TEST_DATA_SIZE> type_grid = {
        // clang-format off
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
    0b00001000000000001110011111001110,
    0b00000000000000001110001111001110,
    0b00000000000000001111000000011110,
    0b01111111111110000111111111111100,
    0b00000000000000000001111111110000,
    0b00000000000000000000000000000000,
        // clang-format on
    };

    std::array<Type, TEST_DATA_SIZE * TEST_DATA_SIZE> converted_arr;
    Matrix<Type> converted {
        converted_arr.data(), TEST_DATA_SIZE, TEST_DATA_SIZE};

    for (int i = 0; i < TEST_DATA_SIZE; ++i) {
        for (int j = 0; j < TEST_DATA_SIZE; ++j) {
            int type_bit = type_grid[i] >> (TEST_DATA_SIZE - 1 - j);
            type_bit &= 0b1;
            converted[i, j] = type_bit ? Type::Building : Type::Ground;
        }
    }

    return converted_arr;
}


template <typename F>
static void print_type_grid(Matrix<const Type> type_grid,
                            Coord top_left,
                            Coord bottom_right,
                            const F& filter) {
    using deco::fg;

    for (int i = top_left.i; i <= bottom_right.i; ++i) {
        for (int j = top_left.j; j <= bottom_right.j; ++j) {
            if (!filter(Coord {i, j})) {
                std::print("{}", '.' | deco::dim);
                continue;
            }
            switch (type_grid[i, j]) {
                case Type::Building:
                    std::print("{}", 'B' | fg(deco::bright_blue));
                    break;
                case Type::BuildingOutline:
                    std::print("O");
                    break;
                default:
                    std::print("{}", '.' | deco::dim);
            }
        }
        std::print("\n");
    }
}

static void print_type_grid(Matrix<const Type> type_grid) {
    const Coord bottom_right = {
        static_cast<int>(type_grid.extent(0) - 1),
        static_cast<int>(type_grid.extent(1) - 1),
    };
    print_type_grid(type_grid,
                    {0, 0},
                    bottom_right,
                    []([[maybe_unused]] Coord c) { return true; });
}

static void print_building(Matrix<const Type> type_grid,
                           const Building& building,
                           const auto& filter) {
    print_type_grid(
        type_grid,
        building.top_left,
        building.bottom_right,
        [&](Coord c) { return building.coords.contains(c) && filter(c); });
}

static void print_building(Matrix<const Type> type_grid,
                           const Building& building) {
    print_building(type_grid, building, []([[maybe_unused]]Coord c) { 
        return true;
    });
}

/// 輪郭検出アルゴリズムをテスト
void test_building_algor() {
    constexpr int rows = TEST_DATA_SIZE;
    constexpr int cols = TEST_DATA_SIZE;

    constexpr auto type_grid_arr = test_type_grid();

    MapData data = MapData {
        .grid = std::vector<double>(rows * cols, 0),
        .types = std::vector<Type>(type_grid_arr.begin(), type_grid_arr.end()),
        .buildings = std::vector<Building>{},
        .x = 0,
        .z = 0,
        .rows = rows,
        .cols = cols,
    };
    Matrix<Type> type_grid {data.types.data(), TEST_DATA_SIZE, TEST_DATA_SIZE};

    std::println("{}", "入力: " | deco::bold);
    print_type_grid(type_grid);

    std::println("{}", "\n輪郭検出: " | deco::bold);
    find_building_outline(data);
    print_type_grid(type_grid);

    auto buildings = ccl_buildings(data);
    std::println("{}", "\n建物検出:" | deco::bold);
    std::println("建物数: {}",
                 buildings.size() | deco::fg(deco::bright_yellow));

    int count = 1;
    for (auto& building : buildings) {
        std::println("\n建物{}: ({}, {}) ~ ({}, {}):\n",
                     count++,
                     building.top_left.i,
                     building.top_left.j,
                     building.bottom_right.i,
                     building.bottom_right.j);

        print_building(type_grid, building);
        scan_outlines(data, building);
        std::println("輪郭数: {}", building.outline_paths.size());
        for (const auto [i, outline_path] :
             building.outline_paths | views::enumerate) {
            std::println("\n輪郭{}: \n", i + 1);
            print_building(type_grid, building, [&](Coord c){
                return std::ranges::contains(outline_path, c);
            });
            std::println("\n削減: \n");
            simplify_outline(building, outline_path);
            print_building(type_grid, building, [&](Coord c){
                return std::ranges::contains(outline_path, c);
            });
        }
    }
}
#endif
