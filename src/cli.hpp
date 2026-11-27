#pragma once

#include <cstdint>
#include <string>
#include <iosfwd>
#include <functional>
#include <filesystem>
#include <utility>
#include <string_view>
#include <optional>

struct Option {
    std::vector<std::string> input_files;           // 入力ファイル
    std::optional<std::string> input_dir;           // 入力ディレクトリ
    std::filesystem::path output_file;              // 出力ファイル
    bool append = false;                            // 出力する際にファイルを上書きするのではなく追加する
    
    bool minecraft = false;
    
    double smooth_ground_threshold = 0.1;
    int smooth_ground_block = 64;
    double smooth_building_threshold = 0.1;
    int smooth_building_block = 16;

    std::uint32_t smooth_outline = 1;
    bool colorful_buildings = false;

    std::ofstream open_output_file() const;
    std::pair<std::ofstream, std::string> open_output_file_suffix(std::string_view suffix) const;
};

Option parse_args(int argc, const char** argv);

void add_command(std::string name,
                 std::string desc,
                 std::function<void(const Option&)> action);

void add_command(std::string name,
                 std::string desc,
                 std::function<void()> action);

void run_command(const Option& opt);
