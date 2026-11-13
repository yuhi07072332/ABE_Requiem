#pragma once

#include <string>
#include <iosfwd>
#include <functional>

struct Option {
    std::string input_file;               // 入力ファイル
    std::string output_file;              // 出力ファイル
    bool append;                          // 出力する際にファイルを上書きするのではなく追加する
};


Option parse_args(int argc, const char** argv);

void add_command(std::string name,
                 std::string desc,
                 std::function<void(const Option&)> action);

void add_command(std::string name,
                 std::string desc,
                 std::function<void()> action);

void run_command(const Option& opt);

/// `output_file`をオープンし、`ofstream`を返す
std::ofstream open_output_file(const Option& opt);
