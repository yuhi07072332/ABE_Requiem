#include <string_view>
#include <vector>
#include <print>
#include <exception>
#include <utility>
#include <string>
#include <fstream>
#include <ios>

#include "third_party/decoterm/format.hpp" // IWYU pragma: keep
#include "third_party/argparse.hpp"

#include "cli.hpp"

struct Command {
    std::string name;
    std::string desc;
    CommandAction action;
};

static std::vector<Command> commands;
static std::string selected_command;

static void print_help() {
    const deco::Style placeholder_s = deco::fg(deco::bright_yellow);
    const deco::Style title_s = deco::bold | deco::underline;
    const deco::Style highlight_s = deco::bold | deco::fg(deco::bright_blue);

    std::println("{}", title_s | "使い方\n");
    std::println("./genk {} {} {}",
                 deco::dim | "[オプション...]",
                 placeholder_s | "<コマンド>",
                 placeholder_s | "<地図データファイル>");
    std::println("");

    std::println("{}", title_s | "コマンド\n");
    for (const auto& [name, desc, _] : commands)
        std::println("{:8}\t\t{}", highlight_s | name, desc);
    std::println("");

    std::println("{}", title_s | "オプション\n");
    std::println("{}\t\t出力ファイル名を指定(デフォルト: out.wrl)",
                 highlight_s | "-o <ファイル名>");
    std::println("{}\t\t出力する際にファイルを上書きするのではなく追加する",
                 highlight_s | "-a, --append");
    std::println("{}\t\tヘルプメッセージを出力",
                 highlight_s | "-h, --help");
}

static void print_error(std::string_view msg) {
    std::println(stderr,
                 "{} {}\n",
                 deco::bold | deco::fg(deco::bright_red) | "エラー: ",
                 msg);
}

Option parse_args(int argc, const char** argv) {
    Option opt;
    argparse::ArgumentParser parser("genk", "0.1", argparse::default_arguments::none);

    parser.add_argument("command")
        .store_into(selected_command);

    parser.add_argument("file-path")
        .default_value("")
        .store_into(opt.input_file);

    parser.add_argument("-h", "--help")
        .flag()
        .action([](const auto&){
            print_help();
            std::exit(0);
        });

    parser.add_argument("-o")
        .default_value("out.wrl")
        .store_into(opt.output_file)
        .action([&](const std::string& filename){
            if (!filename.ends_with(".wrl")) {
                opt.output_file = filename + ".wrl";
            }
        });

    parser.add_argument("-a", "--append")
        .flag()
        .store_into(opt.append);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        print_error(e.what());
        print_help();
        throw std::exception();
    }

    return opt;
}

void add_command(std::string name, std::string desc, CommandAction action) {
    commands.emplace_back(std::move(name), std::move(desc), action);
}

void run_command(const Option& opt) {
    for (const auto& command : commands) {
        if (command.name == selected_command) {
            try {
                command.action(opt);
            } catch (const std::exception& e) {
                print_error(e.what());
                throw std::exception();
            }
            return;
        }
    }
    print_error(std::string("コマンド '" + selected_command + "' が見つかりません"));
    print_help();
    throw std::exception();
}

std::ofstream open_output_file(const Option& opt) {
    auto mode = std::ios_base::out;
    if (opt.append) mode |= std::ios_base::app;
    return std::ofstream(opt.output_file, mode);
}
