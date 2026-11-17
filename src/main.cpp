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
//   `opt.open_output_file()`で出力ファイルへの`std::ofstream`を受け取る

void report_heights(const Option& opt) {
    MapData data = read_map(opt.input_file);
    auto freq = calc_frequency(data.grid);
    std::println("ファイル: {}", opt.input_file);
    for (auto [lower, frequency] : freq)
        std::println("\"[{:>3}, {:>3}]\", {}", lower, lower + 5, frequency);
}

void gen_points(const Option& opt) {
    MapData data = read_map(opt.input_file);
    std::ofstream ofs = opt.open_output_file();
    write_elevation_grid(ofs, data);
    std::println("{} に出力しました",
                 opt.output_file.filename().string() | deco::fg(deco::bright_yellow));
}

void test_building() {
    test_building_algor();
}

void show_classify(const Option& opt) {
    MapData data = read_map(opt.input_file);
    classify_map(data);
    show_mapdata(data);
}

void all(const Option& opt) {
    const deco::Style hl = deco::bold | deco::fg(deco::bright_yellow);

    MapData data = read_map(opt.input_file);

    std::println("{}", "分類" | hl);
    classify_map(data);

    std::println("{}", "欠損点補間" | hl);
    fill_missing(data);

    std::println("{}", "建物処理" | hl);
    process_building(data);

    std::vector<Coord> ground_coords;

    for (int i = 0; i < data.rows; ++i) { 
        for (int j = 0; j < data.cols; ++j) {
            switch (data.type_at(i, j)) {
                case Type::Ground: 
                case Type::Unknown: ground_coords.emplace_back(i, j);
                    break;
                default: break;
            }
        }
    }

    std::ofstream out = opt.open_output_file();

    std::filesystem::path out_ground_path = add_prefix(opt.output_file, "_ground");
    std::ofstream out_ground = 
        opt.open_output_file(out_ground_path.string());

    std::filesystem::path out_building_path = add_prefix(opt.output_file, "_building");
    std::ofstream out_building = 
        opt.open_output_file(out_building_path.string());

    constexpr std::string_view MAIN_VRML = R"({}
Inline {{
    url "{}"
}}

Inline {{
    url "{}"
}}
)";

    std::print(out, MAIN_VRML, VRML_HEADER, out_ground_path.string(), out_building_path.string());

    std::println("VRML: 地面");
    
    std::print(out_ground, VRML_HEADER);
    write_delaunay_triangulation(out_ground, ground_coords, data, "0.25 0.5 1");

    std::println("VRML: 建物");

    std::print(out_building, VRML_HEADER);

    for (const auto& building : data.buildings) {
        write_building(out_building, data, building, "1 0.5 0.25");
    }

    std::println("{}, {}, {}に出力しました",
                 opt.output_file.filename().string(),
                 out_ground_path.filename().string(),
                 out_building_path.filename().string());
}

void all_show(const Option& opt) {
    const deco::Style hl = deco::bold | deco::fg(deco::bright_yellow);

    MapData data = read_map(opt.input_file);

    std::println("{}", "分類" | hl);
    classify_map(data);

    std::println("{}", "欠損点補間" | hl);
    fill_missing(data);

    std::println("{}", "建物処理" | hl);
    process_building(data);

    show_mapdata(data);
}

void test() {
}

// ┌                                                         ┐
// │                          main                           │
// └                                                         ┘

int main(int argc, const char** argv) {
    try {
        // コマンドを登録
        add_command("all", "すべてのアルゴリズムとVRML化を実行", all);
        add_command("all-show", "すべてのアルゴリズム実行し、VRML化をせずvisualizerを起動", all_show);

        add_command(
            "report-heights", "点群の高さの度数分布表を表示", report_heights);
        add_command("gen-points", "地図データから点群を直接出力", gen_points);
        add_command("test-building", "建物に関する処理を仮想データでテスト", test_building);
        add_command("show-classify", "classifyの結果を図示", show_classify);


        add_command("test", "テスト用コマンド", test);

        Option opt = parse_args(argc, argv);
        run_command(opt);
    } catch (const std::exception& e) {
        return 1;
    }
    return 0;
}
