#include <exception>
#include <filesystem>
#include <fstream>
#include <ios>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <third_party/argparse.hpp>
#include <third_party/decoterm.hpp>

#include "cli.hpp"

struct CommandAction {
    using OptFn = std::function<void(const Option&)>;
    using VoidFn = std::function<void(void)>;

    std::variant<OptFn, VoidFn> action;

    void operator()(const Option& opt) const {
        if (auto* fn = std::get_if<OptFn>(&action)) (*fn)(opt);
    }

    void operator()() const {
        if (auto* fn = std::get_if<VoidFn>(&action)) (*fn)();
    }
};

struct Command {
    std::string name;
    std::string desc;
    CommandAction action;
};

static std::vector<Command> commands;
static std::string selected_command;
static std::vector<argparse::ArgumentParser::OptionalArgument>
    optional_arguments;

static void print_help() {
    const deco::Style placeholder = deco::fg(deco::bright_yellow);
    const deco::Style title = deco::bold | deco::underline;
    const deco::Style highlight = deco::bold | deco::fg(deco::bright_blue);

    std::println("{}", "使い方\n" | title);
    std::println("./genk {} {} {}",
                 "[オプション...]" | deco::dim,
                 "<コマンド>" | placeholder,
                 "<地図データファイル...>"
                     | placeholder);
    std::println("./genk {} {} {}",
                 "[オプション...]" | deco::dim,
                 "<コマンド>" | placeholder,
                 "-d <ディレクトリ>" | placeholder);
    std::println("");

    std::println("{}", "コマンド\n" | title);
    for (const auto& [name, desc, _] : commands)
        std::println("{:30}  {}", name | highlight, desc);
    std::println("");

    std::println("{}", "オプション\n" | title);
    for (const auto& [name, metavar, help] : optional_arguments) {
        std::string arg = std::format("{} {}",
                                      name | highlight,
                                      (metavar.empty() ? "" : metavar + " ") | placeholder);

        std::println("{:<50}  {}", arg, help);
    }
        
}

static void print_error(std::string_view msg) {
    std::println(stderr,
                 "{} {}\n",
                 "エラー: " | deco::bold | deco::fg(deco::bright_red),
                 msg);
}

Option parse_args(int argc, const char** argv) {
    Option opt;
    argparse::ArgumentParser parser(
        "genk", "0.1", argparse::default_arguments::none);
    std::string output_filename;

    parser.add_argument("command").store_into(selected_command);

    parser.add_argument("input_files")
        .nargs(argparse::nargs_pattern::any)
        .store_into(opt.input_files);

    parser.add_argument("-d", "--directory")
        .metavar("<DIRECTORY>")
        .help("指定されたディレクトリのすべての*.datを読み込む")
        .action([&](const std::string& dir){
            opt.input_dir = dir;
        });

    parser.add_argument("-o")
        .default_value("out.wrl")
        .metavar("<FILENAME>")
        .store_into(output_filename)
        .help("出力ファイル名を指定(デフォルト: out.wrl)")
        .action([&](const std::string& filename) {
            if (!filename.ends_with(".wrl")) {
                output_filename = filename + ".wrl";
            }
        });

    parser.add_argument("--smooth-ground")
        .store_into(opt.smooth_ground)
        .metavar("<N>")
        .help("地面の平滑化の強度を指定[デフォルト=16]");

    parser.add_argument("--smooth-building")
        .store_into(opt.smooth_building)
        .metavar("<N>")
        .help("建物の平滑化の強度を指定[デフォルト=16]");

    parser.add_argument("-a", "--append")
        .flag()
        .help("出力する際にファイルを上書きするのではなく追加する")
        .store_into(opt.append);

    parser.add_argument("--colorful-buildings")
        .flag()
        .help("建物をカラフルにして出力")
        .store_into(opt.colorful_buildings);

    parser.add_argument("-h", "--help")
        .flag()
        .help("ヘルプメッセージを出力")
        .action([](const auto&) {
            print_help();
            std::exit(0);
        });


    optional_arguments = parser.optional_arguments();

    try {
        parser.parse_args(argc, argv);
    } catch (const std::runtime_error& e) {
        print_error(e.what());
        print_help();
        throw std::exception();
    }

    opt.output_file = std::move(output_filename);
    if (opt.input_dir) {
        if (!opt.input_files.empty()) {
            print_error("ディレクトリを指定した場合は地図ファイルを指定していけません");
            print_help();
            throw std::exception();
        }

        std::println("ファイル読み込み:");
        for (const auto& entry :
             std::filesystem::directory_iterator(*opt.input_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".dat") {
                opt.input_files.push_back(entry.path().string());
                std::println("{}", entry.path().string());
            }
        }
    }

    return opt;
}

void add_command(std::string name,
                 std::string desc,
                 std::function<void(const Option&)> action) {
    commands.emplace_back(
        std::move(name), std::move(desc), CommandAction(std::move(action)));
}

void add_command(std::string name,
                 std::string desc,
                 std::function<void()> action) {
    commands.emplace_back(
        std::move(name), std::move(desc), CommandAction(std::move(action)));
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
    print_error(
        std::string("コマンド '" + selected_command + "' が見つかりません"));
    print_help();
    throw std::exception();
}

static std::filesystem::path add_suffix(const std::filesystem::path& path,
                                        std::string_view prefix) {
    std::filesystem::path new_path = path;
    new_path.replace_filename(
        path.filename().stem().string().append(prefix).append(
            path.extension().string()));
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

std::pair<std::ofstream, std::string> Option::open_output_file_suffix(
    std::string_view suffix) const {
    std::filesystem::path out_path = add_suffix(output_file, suffix);
    return std::make_pair(open_file(out_path, *this), out_path.string());
}
