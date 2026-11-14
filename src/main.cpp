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

void show_classify(const Option& opt) {
    MapData data = read_map(opt.input_file);
    classify_map(data);
    show_mapdata_types(data);
}

void all(const Option& opt) {
    MapData data = read_map(opt.input_file);
    auto ofs = open_output_file(opt);

    std::println("{}", "分類" | deco::bold | deco::fg(deco::bright_yellow));
    classify_map(data);

    std::println("{}", "欠損点補間" | deco::bold | deco::fg(deco::bright_yellow));
    fill_missing(data);

    std::println("{}", "建物処理" | deco::bold | deco::fg(deco::bright_yellow));
    auto buildings = process_building(data);

    std::vector<Coord> ground;

    for (int i = 0; i < data.rows; ++i) { 
        for (int j = 0; j < data.cols; ++j) {
            switch (data.type_at(i, j)) {
                case Type::Ground: 
                case Type::Unknown: ground.emplace_back(i, j); break;
                default: break;
            }
        }
    }

    write_delaunay_triangulation(ofs, ground, data, "255 125 64");

    for (const auto& building : buildings) {
        std::vector<Coord> coords{building.coords.begin(), building.coords.end()};
        write_delaunay_triangulation(ofs, coords, data, "64 125 255");
    }
}

void test() {
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
        add_command("show-classify", "classifyの結果を図示", show_classify);

        add_command("all", "すべてのアルゴリズムを実行", all);

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
