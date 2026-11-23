#include <exception>
#include <format>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <string>

#include <third_party/decoterm.hpp>

#include "cli.hpp"
#include "genk.hpp"

constexpr deco::Style hl = deco::bold | deco::fg(deco::bright_yellow);

/// 全てのアルゴリズムを実行
void exec_all(MapData& data) {
    std::println("{}", "分類" | hl);
    classify_map(data);

    std::println("{}", "欠損点補間" | hl);
    fill_missing(data);

    std::println("{}", "建物処理" | hl);
    process_building(data);

    std::println("{}", "地面平滑化" | hl);
    smooth_ground(data);

    std::println("{}", "建物平滑化" | hl);
    smooth_buildings(data);
}

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

void all(const Option& opt) {
    MapData data = read_map(opt.input_file);
    exec_all(data);

    std::println("{}", "VRML化" | hl);
    auto out = opt.open_output_file();

    auto [out_ground, out_ground_filename] =
        opt.open_output_file_suffix("_ground");
    auto [out_building, out_building_filename] =
        opt.open_output_file_suffix("_building");

    export_vrml(data,
                out,
                out_ground,
                out_ground_filename,
                out_building,
                out_building_filename,
                opt.colorful_buildings);

    std::println("{}, {}, {}に出力しました",
                 opt.output_file.string(),
                 out_ground_filename,
                 out_building_filename);
}

void show_classify(const Option& opt) {
    MapData data = read_map(opt.input_file);
    classify_map(data);
    show_mapdata_types(data);
}

void show_buildings(const Option& opt) {
    MapData data = read_map(opt.input_file);
    classify_map(data);
    show_mapdata_buildings(data);
}

void show_heights(const Option& opt) {
    MapData data = read_map(opt.input_file);
    show_mapdata_heights(data);
}


void inspect(const Option& opt) {
    // 入力ファイルが指定されていない場合はエラー
    if (opt.input_file.empty())
        throw std::runtime_error("inspect: input file is required");

    MapData data = read_map(opt.input_file);
    
    // 元々欠損点だったかどうかを記録する配列
    std::vector<bool> originally_missing(data.rows * data.cols);
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            originally_missing[i * data.cols + j] = data.is_missing(i, j);
        }
    }

    // アルゴリズムを実行１
    std::println("{}", "分類" | hl);
    classify_map(data);

    std::println("{}", "欠損点補間" | hl);
    fill_missing(data);

    std::println("{}", "建物処理" | hl);
    process_building(data);

    // 一時ファイルにデータを保存
    constexpr const char* temp_file = "inspector_temp.dat";
    {
        std::ofstream ofs{temp_file};
        std::println(ofs, "MAPDATA 1");
        std::println(ofs, "{} {} {} {}", data.rows, data.cols, data.x, data.z);
        for (int i = 0; i < data.rows; ++i) {
            for (int j = 0; j < data.cols; ++j) {
                std::println(ofs, "{} {} {}",
                             data.at(i, j),
                             static_cast<int>(data.type_at(i, j)),
                             originally_missing[i * data.cols + j] ? 1 : 0);
            }
        }
    }
    
    // アルゴリズムを実行２
    std::println("{}", "地面平滑化" | hl);
    const unsigned int ground_before = data.ground_coords;
    smooth_ground(data);
    const unsigned int ground_erased = ground_before - data.ground_coords;

    std::println("{}", "建物平滑化" | hl);
    const unsigned int building_before = data.building_coords;
    smooth_buildings(data);
    const unsigned int building_erased = building_before - data.building_coords;

    std::ofstream ofs{temp_file, std::ios::app};
    std::println(ofs,
                 "SMOOTHING {} {} {} {}",
                 ground_before,
                 ground_erased,
                 building_before,
                 building_erased);
    ofs.close();

    // Pythonのビューアを起動
    const int result = std::system("python ./tools/inspector.py inspector_temp.dat");
    std::filesystem::remove(temp_file);
    if (result != 0)
        throw std::runtime_error("inspect: integrated viewer failed to start");
}

void test() {}

// ┌                                                         ┐
// │                          main                           │
// └                                                         ┘

int main(int argc, const char** argv) {
    try {
        // コマンドを登録
        add_command("all", "すべてのアルゴリズムとVRML化を実行", all);

        add_command("show-heights", "点群の高さを図示", show_heights);
        add_command("show-classify", "classifyの結果を図示", show_classify);
        add_command("show-buildings", "建物を色分けで図示", show_buildings);

        add_command(
            "report-heights", "点群の高さの度数分布表を表示", report_heights);
        add_command("gen-points", "地図データから点群を直接出力", gen_points);


        add_command("inspect", "すべてのアルゴリズムを実行し、総合ビューアを起動", inspect);

        add_command("test", "テスト用コマンド", test);

        Option opt = parse_args(argc, argv);
        run_command(opt);
    } catch (const std::exception& e) {
        return 1;
    }
    return 0;
}
