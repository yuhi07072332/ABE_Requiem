#include <cstdlib>
#include <exception>
#include <fstream>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <format>

#include "third_party/argparse.hpp"
#include "third_party/decoterm/format.hpp" // IWYU pragma: keep

#include "genk.hpp"

struct Option {
    std::string command;
    std::string file_path;
    std::string output_filename;
};

void report_heights(const std::vector<double>& heights) {
    auto freq = calc_frequency(heights);
    for (auto [lower, frequency] : freq)
        std::println("\"[{:>3}, {:>3}]\", {}", lower, lower + 5, frequency);
}

void test_ground(const MapData& data, std::string_view out_filename) {
    std::println("{}", deco::bold | deco::fg(deco::yellow) | "地面判定中...");
    auto ground_points = find_ground(data);
    std::println("マップ内点群数: {}", data.grid.size());
    std::println("地面判定した点群数: {} (~{}%)",
                 ground_points.size(),
                 ground_points.size() * 100 / data.grid.size());

    std::ofstream ofs(out_filename.data());
    std::println(ofs, VRML_HEADER);

    int triangles = write_delaunay_triangulation(ground_points, ofs);
    std::println("面数(三角形数): {}", triangles);
    std::println("総データ量(点 + 面の数): {}",
                 deco::fg(deco::bright_yellow)
                     | (ground_points.size() + triangles));
}

/// ヘルプメッセージを出力
void print_help() {
    const deco::Style placeholder_s = deco::fg(deco::bright_yellow);
    const deco::Style title_s = deco::bold | deco::underline;
    const deco::Style highlight_s = deco::bold | deco::fg(deco::bright_blue);

    /// コマンド一覧
    const std::vector<std::pair<std::string_view, std::string_view>> COMMANDS = {
        {"report-heights", "点群の高さの度数分布表を表示"},
        {"test-ground", "地面判定アルゴリズムをテスト"},
        {"gen-points", "地図データから点群を直接出力"}
    };

    std::println("{}", title_s | "使い方\n");
    std::println("./genk {} {} {}",
                 deco::dim | "[オプション...]",
                 placeholder_s | "<コマンド>",
                 placeholder_s | "<地図データファイル>");
    std::println();

    std::println("{}", title_s | "コマンド\n");
    for (auto [name, desc] : COMMANDS)
        std::println("{}\t\t{}", highlight_s | name, desc);
    std::println();

    std::println("{}", title_s | "オプション\n");
    std::println("{}\t\t出力ファイル名を指定",
                 highlight_s | "-o <ファイル名>");
}

int main(int argc, const char** argv) {
    Option opt;
    argparse::ArgumentParser parser("genk", "0.1", argparse::default_arguments::none);

    parser.add_argument("command")
        .help("コマンド")
        .store_into(opt.command);

    parser.add_argument("file-path")
        .help("地図データファイル")
        .store_into(opt.file_path);

    parser.add_argument("-h", "--help")
        .flag()
        .help("ヘルプメッセージを出力")
        .action([](const auto&){
            print_help();
            std::exit(0);
        });

    parser.add_argument("-o")
        .help("出力ファイル名を指定")
        .default_value("out.wrl")
        .store_into(opt.output_filename)
        .action([&](const std::string& filename){
            if (!filename.ends_with(".wrl")) {
                opt.output_filename = filename + ".wrl";
            }
        });

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        std::println(stderr,
                     "{} {}\n",
                     deco::bold | deco::fg(deco::bright_red) | "エラー: ",
                     e.what());
        print_help();
        return 1;
    }

    try {
        auto data = read_map(opt.file_path.c_str());

        if (opt.command == "report-heights") report_heights(data.grid);
        else if (opt.command == "test-ground") test_ground(data, opt.output_filename);
        else if (opt.command == "gen-points") generate_points(data, opt.output_filename);
        else throw std::runtime_error(std::format("コマンドが見つかりません: {}", opt.command));

    } catch (const std::exception& e) {
        std::println(stderr,
                     "{} {}\n",
                     deco::bold | deco::fg(deco::bright_red) | "エラー: ",
                     e.what());
        return 1;
    }

    return 0;
}
