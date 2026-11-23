#include <filesystem>
#include <string_view>
#include <variant>
#include <vector>
#include <print>
#include <exception>
#include <utility>
#include <string>
#include <fstream>
#include <ios>

#include <third_party/decoterm.hpp>
#include <third_party/argparse.hpp>

#include "cli.hpp"

struct CommandAction {
    using OptFn = std::function<void(const Option&)>;
    using VoidFn = std::function<void(void)>;

    std::variant<OptFn, VoidFn> action;

    void operator()(const Option& opt) const {
        if (auto* fn = std::get_if<OptFn>(&action))
            (*fn)(opt);
    }

    void operator()() const {
        if (auto* fn = std::get_if<VoidFn>(&action))
            (*fn)();
    }
};

struct Command {
    std::string name;
    std::string desc;
    CommandAction action;
};

static std::vector<Command> commands;
static std::string selected_command;

static void print_help() {
    const deco::Style placeholder = deco::fg(deco::bright_yellow);
    const deco::Style title = deco::bold | deco::underline;
    const deco::Style highlight = deco::bold | deco::fg(deco::bright_blue);

    std::println("{}", "使い方\n" | title);
    std::println("./genk {} {} {}",
                 "[オプション...]" | deco::dim,
                 "<コマンド>" | placeholder,
                 "<地図データファイル>" | placeholder);
    std::println("");

    std::println("{}", "コマンド\n" | title);
    for (const auto& [name, desc, _] : commands)
        std::println("{:8}\t\t{}", name | highlight, desc);
    std::println("");

    std::println("{}", "オプション\n" | title);
    std::println("{}\t\t出力ファイル名を指定(デフォルト: out.wrl)",
                 "-o <ファイル名>" | highlight);
    std::println("{}\t\t出力する際にファイルを上書きするのではなく追加する",
                 "-a, --append" | highlight);
    std::println("{}\t建物をカラフルにして出力",
                 "--colorful-buildings" | highlight);
    std::println("{}\t\tヘルプメッセージを出力",
                 "-h, --help" | highlight);
}

static void print_error(std::string_view msg) {
    std::println(stderr,
                 "{} {}\n",
                 "エラー: " | deco::bold | deco::fg(deco::bright_red),
                 msg);
}

Option parse_args(int argc, const char** argv) {
    Option opt;
    argparse::ArgumentParser parser("genk", "0.1", argparse::default_arguments::none);
    std::string output_filename;

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
        .store_into(output_filename)
        .action([&](const std::string& filename){
            if (!filename.ends_with(".wrl")) {
                output_filename = filename + ".wrl";
            }
        });

    parser.add_argument("-a", "--append")
        .flag()
        .store_into(opt.append);

    parser.add_argument("--colorful-buildings")
        .flag()
        .store_into(opt.colorful_buildings);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        print_error(e.what());
        print_help();
        throw std::exception();
    }

    opt.output_file = std::move(output_filename);

    return opt;
}


void add_command(std::string name,
                 std::string desc,
                 std::function<void(const Option&)> action) {
    commands.emplace_back(std::move(name), std::move(desc), CommandAction(std::move(action)));
}

void add_command(std::string name,
                 std::string desc,
                 std::function<void()> action) {
    commands.emplace_back(std::move(name), std::move(desc), CommandAction(std::move(action)));
}


void run_command(const Option& opt) {
    for (const auto& command : commands) {
        if (command.name == selected_command) {
            try {
                command.action(opt);
                command.action();
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

static std::filesystem::path add_suffix(const std::filesystem::path& path,
                                 std::string_view prefix) {
    std::filesystem::path new_path = path;
    new_path.replace_filename(path.filename()
                                .stem()
                                .string()
                                .append(prefix)
                                .append(path.extension().string()));
    return new_path;
}

std::ofstream open_file(const std::filesystem::path& path, const Option& opt) {
    auto mode = std::ios_base::out;
    if (opt.append) mode |= std::ios_base::app;
    return std::ofstream(path, mode);
}

std::ofstream Option::open_output_file() const {
    return open_file(output_file, *this);
}

std::pair<std::ofstream, std::string> Option::open_output_file_suffix(std::string_view suffix) const {
    std::filesystem::path out_path = add_suffix(output_file, suffix);
    return std::make_pair(open_file(out_path, *this), out_path.string());
}
