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

void test_building([[maybe_unused]] const Option& opt) {
    test_building_algor();
}

void test([[maybe_unused]] const Option& opt) { }

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
