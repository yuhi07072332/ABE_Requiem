#pragma once

#include <string>
#include <iosfwd>

struct Option {
    std::string input_file;               // 入力ファイル
    std::string output_file;              // 出力ファイル
    bool append;                          // 出力する際にファイルを上書きするのではなく追加する
};

using CommandAction = void(*)(const Option&);

Option parse_args(int argc, const char** argv);
void add_command(std::string name, std::string desc, CommandAction action);
void run_command(const Option& opt);

/// `output_file`をオープンし、`ofstream`を返す
std::ofstream open_output_file(const Option& opt);
