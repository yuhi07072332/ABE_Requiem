#pragma once

#include <string>
#include <iosfwd>
#include <functional>
#include <filesystem>
#include <string_view>

struct Option {
    std::string input_file;                         // 入力ファイル
    std::filesystem::path output_file;              // 出力ファイル
    bool append = false;                            // 出力する際にファイルを上書きするのではなく追加する
    
    bool colorful_buildings = false;

    /// `output_file`をオープンし、`ofstream`を返す
    std::ofstream open_output_file() const;
    std::ofstream open_output_file(std::string_view filename) const;
};

std::filesystem::path add_prefix(const std::filesystem::path& path,
                                 std::string_view prefix);

Option parse_args(int argc, const char** argv);

void add_command(std::string name,
                 std::string desc,
                 std::function<void(const Option&)> action);

void add_command(std::string name,
                 std::string desc,
                 std::function<void()> action);

void run_command(const Option& opt);
