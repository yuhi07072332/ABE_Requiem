#include <exception>
#include <fstream>
#include <print>
#include <string>
#include <vector>
#include <format>
#include <ios>

#include "third_party/decoterm/format.hpp" // IWYU pragma: keep

#include "cli.hpp"
#include "genk.hpp"
#include "third_party/decoterm/style.hpp"

/// Optionに基づいて、`opt.output_file`をオープンする
std::ofstream open_file(const Option& opt) {
    auto mode = std::ios_base::out;
    if (opt.append) mode |= std::ios_base::app;
    return std::ofstream(opt.output_file, mode);
}

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

void test_ground(const Option& opt) {
    MapData data = read_map(opt.input_file);

    std::println("{}", deco::bold | deco::fg(deco::yellow) | "地面判定中...");
    auto ground_points = find_ground(data);
    std::println("マップ内点群数: {}", data.grid.size());
    std::println("地面判定した点群数: {} (~{}%)",
                 ground_points.size(),
                 ground_points.size() * 100 / data.grid.size());

    std::ofstream ofs = open_file(opt);
    std::println(ofs, VRML_HEADER);
    int triangles = write_delaunay_triangulation(ofs, ground_points);

    std::println("面数(三角形数): {}", triangles);
    std::println("総データ量(点 + 面の数): {} (~{}%)",
                 deco::bold | deco::fg(deco::bright_yellow)
                 | (ground_points.size() + triangles),
                 (ground_points.size() + triangles) * 100 / data.grid.size());
    std::println("{} に出力しました", deco::fg(deco::bright_yellow) | opt.output_file);
}

void gen_points(const Option& opt) {
    MapData data = read_map(opt.input_file);
    std::ofstream ofs = open_file(opt);
    write_elevation_grid(ofs, data);
    std::println("{} に出力しました", deco::fg(deco::bright_yellow) | opt.output_file);
}

void test(const Option& opt) {
    std::println("{}", opt.output_file);
}

// ┌                                                         ┐
// │                          main                           │
// └                                                         ┘

int main(int argc, const char** argv) {
    // コマンドを定義
    add_command("report-heights", "点群の高さの度数分布表を表示", report_heights);
    add_command("test-ground", "地面判定アルゴリズムをテスト", test_ground);
    add_command("gen-points", "地図データから点群を直接出力", gen_points);

    add_command("test", "テスト用コマンド", test);

    // argvを解析し、結果を`opt`に保存(失敗した場合はプログラムを終了)
    Option opt;
    if (auto res = parse_args(argc, argv)) opt = *res;
    else return 1;

    // 指定されたコマンド実行
    try {
        run_command(opt);
    } catch (const std::exception& e) {
        // エラーが発生した場合、エラーメッセージを出力
        std::println(stderr,
                     "{} {}\n",
                     deco::bold | deco::fg(deco::bright_red) | "エラー: ",
                     e.what());
        return 1;
    }

    return 0;
}
