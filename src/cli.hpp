#pragma once

#include <optional>
#include <string>

struct Option {
    std::string input_file;               // 入力ファイル
    std::string output_file;              // 出力ファイル
    bool append;                          // 出力する際にファイルを上書きするのではなく追加する
};

using CommandAction = void(*)(const Option&);

std::optional<Option> parse_args(int argc, const char** argv);
void add_command(std::string name, std::string desc, CommandAction action);
void run_command(const Option& opt);
