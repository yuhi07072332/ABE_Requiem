#include <exception>
#include <format>
#include <fstream>
#include <print>
#include <string>

#include "third_party/decoterm.hpp"

#include "cli.hpp"
#include "genk.hpp"

// ┌                                                         ┐
// │                        コマンド                         │
// └                                                         ┘
// NOTE: 
// * コマンド関数は`void()`または`void(const Option& opt)`の形であること
// * 基本的な流れとして、`read_map(opt.input_file)`でMapDataを読み込み、
//   `open_output_file(opt)`で出力ファイルへの`std::ofstream`を受け取る

void report_heights(const Option& opt) {
    MapData data = read_map(opt.input_file);
    auto freq = calc_frequency(data.grid);
    std::println("ファイル: {}", opt.input_file);
    for (auto [lower, frequency] : freq)
        std::println("\"[{:>3}, {:>3}]\", {}", lower, lower + 5, frequency);
}

void gen_points(const Option& opt) {
    MapData data = read_map(opt.input_file);
    std::ofstream ofs = open_output_file(opt);
    write_elevation_grid(ofs, data);
    std::println("{} に出力しました",
                 opt.output_file | deco::fg(deco::bright_yellow));
}

void test_building() {
    test_building_algor();
}

void test(const Option& opt) {
    MapData data = read_map(opt.input_file);

    auto count_valid = [](const MapData& d) {
        return std::count_if(d.grid.begin(), d.grid.end(), [](double y) {
            return !std::isnan(y);
        });
    };

    size_t before = count_valid(data);
    downsample_flat_points(data, 3, 10.0);
    size_t after = count_valid(data);

// ▼ ここで有効な点のリスト（coords）を作る
    std::vector<Coord> coords;
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            if (!data.is_missing(i, j)) {
                coords.push_back(Coord{i, j});
            }
        }
    }

    std::ofstream ofs = open_output_file(opt);
    write_delaunay_triangulation(ofs, coords, data);

    std::println("ファイル: {}", opt.input_file);
    std::println("処理前: {} 点", before);
    std::println("処理後: {} 点", after);
    std::println("削減数: {} 点", before - after);
}

// ┌                                                         ┐
// │                          main                           │
// └                                                         ┘

int main(int argc, const char** argv) {
    try {
        // コマンドを登録
        add_command(
            "report-heights", "点群の高さの度数分布表を表示", report_heights);
        add_command("gen-points", "地図データから点群を直接出力", gen_points);
        add_command("test-building", "建物に関する処理を仮想データでテスト", test_building);

        add_command("test", "テスト用コマンド", test);

        // コマンドライン引数(`argv`)を解析し、結果を`opt`に保存
        Option opt = parse_args(argc, argv);

        // 指定されたコマンド実行
        run_command(opt);
    } catch (const std::exception& e) {
        return 1;
    }
    return 0;
}
