#include <cassert>
#include <exception>
#include <fstream>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <format>

#include "third_party/argparse.hpp"
#include "third_party/decoterm/format.hpp"

#include "genk.hpp"

enum class Command {
    ReportHeights,
    GeneratePoints,
    TestGround,
};

struct Option : argparse::Args {
    std::string& command = arg("コマンド");
    std::string& file_path = arg("入力ファイル");
    std::string& output_filename = kwarg("o", "").set_default("out.wrl");

    void help() override {
        const deco::Style placeholder_s = deco::fg(deco::bright_yellow);
        const deco::Style title_s = deco::bold | deco::underline;
        const deco::Style highlight_s = deco::bold | deco::fg(deco::bright_blue);

        deco::println(title_s, "使い方\n");
        std::println("./genk {} {} {}",
                     deco::dim | "[オプション...]",
                     placeholder_s | "<コマンド>",
                     placeholder_s | "<地図データファイル>");
        std::println();

        deco::println(title_s, "コマンド\n");

        std::println("{}\t\t点群の高さの度数分布表を表示",
                     highlight_s | "report-heights");
        std::println("{}\t\t地面判定アルゴリズムをテスト",
                     highlight_s | "test-ground");
        std::println("{}\t\t地図データから点群を直接出力",
                     highlight_s | "gen-points");
        std::println();

        deco::println(title_s, "オプション\n");
        std::println("{}\t\t出力ファイル名を指定",
                     highlight_s | "-o <ファイル名>");
    }

    Command parse_command() const {
        if (command == "report-heights") return Command::ReportHeights;
        if (command == "gen-points") return Command::GeneratePoints;
        if (command == "test-ground") return Command::TestGround;
        throw std::runtime_error(std::format("未知なコマンド名: {}", command));
    }

    void normalize_output_filename() const {
        if (!output_filename.ends_with(".wrl")) output_filename.append(".wrl");
    }
};

void report_heights(const std::vector<double>& heights) {
    auto freq = calc_frequency(heights);
    for (auto [lower, frequency] : freq)
        std::println("\"[{:>3}, {:>3}]\", {}", lower, lower + 5, frequency);
}

void test_ground(const MapData& data, std::string_view out_filename) {
    deco::println(deco::bold | deco::fg(deco::yellow), "地面判定中...");
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


int main(int argc, const char** argv) {
    Option opt;
    Command command;
    try {
        opt.parse(argc, argv, true);
        opt.normalize_output_filename();
        command = opt.parse_command();
    } catch (const std::runtime_error& e) {
        std::println(stderr,
                     "{} {}\n",
                     deco::bold | deco::fg(deco::bright_red) | "エラー: ",
                     e.what());
        opt.help();
        return 1;
    }

    try {
        auto data = read_map(opt.file_path.c_str());
        switch (command) {
            case Command::ReportHeights:
                report_heights(data.grid);
                break;
            case Command::GeneratePoints:
                generate_points(data, opt.output_filename);
                break;
            case Command::TestGround:
                test_ground(data, opt.output_filename);
                break;
        }
    } catch (const std::exception& e) {
        std::println(stderr,
                     "{} {}\n",
                     deco::bold | deco::fg(deco::bright_red) | "エラー: ",
                     e.what());
        return 1;
    }

    return 0;
}
