#pragma once

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
    
    int smooth_ground = 16;
    int smooth_building = 16;
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
