#include <cli/cli.h>
#include <cli/clifilesession.h>
#include <cli/CLI.hpp>
#include <fmt/format.h>
#include <fmt/color.h>

import LibIBP;



using namespace cli;
using namespace std;

std::unique_ptr<Menu> rootMenu;
Cli* CliApp;
CliFileSession* input;
IBP::Intepreter interpreter;

std::vector<std::string> readNonEmptyLines(const std::string& filename) {
    std::vector<std::string> lines;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return lines;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }

    file.close();
    return lines;
}
void run_file(const std::string& filename)
{
    auto cmds = readNonEmptyLines(filename);
    if (cmds.size() > 0) {
        for (const auto& cmd : cmds) {
            fmt::print(fmt::fg(fmt::color::orange), "ibp> {}\n", cmd);
            input->Feed(cmd);
        }
    }

    auto err = interpreter.get_last_error();
    if (err != 0)
    {
        std::exit(err);
    }
}
void init_interactive()
{
    rootMenu = make_unique< Menu >("ibp");
    rootMenu->Insert(
        "ld",
        [&](std::ostream& out, const std::string& s) {
            auto res = interpreter.execute_open_file(s, "");
            if (res)
            {
                out << "File opened successfully.\n";
            }
            else
            {
                out << "Failed to open file: " << res.error().info << "\n";
            }
        },
        "open binary file");
    rootMenu->Insert(
        "ld",
        [](std::ostream& out, const std::string& s1, const std::string& s2) { out << "Hello, world\n" << s2 << std::endl; },
        "Print hello world");
    rootMenu->Insert(
        "r",
        [&](std::ostream& out, const std::vector<std::string>& op)
        {
            auto exec = interpreter.execute_read_file(op, false);
        },
        "Read common data type: i32, i64, f32, f64.");
    rootMenu->Insert(
        "rv",
        [&](std::ostream& out, const std::vector<std::string>& op)
        {
            auto exec = interpreter.execute_read_file(op, true);
        },
        "Read common data and move pointer.");
    rootMenu->Insert(
        "mv",
        [&](std::ostream& out, const std::string& op)
        {
            auto exec = interpreter.execute_move(op);
        },
        "Move pointer to relative postion: mr 4, mr -4");
    rootMenu->Insert(
        "ma",
        [&](std::ostream& out, const std::string& op)
        {
            auto exec = interpreter.execute_move(op, true);
        },
        "Move pointer to absolute position: ma 4, ma 5");
    rootMenu->Insert(
        "ls",
        [&](std::ostream& out, const std::vector<std::string>& ops)
        {
            //auto exec = interpreter.execute_move(op);
            for (int i = 0; i < ops.size(); ++i)
            {
                std::cout << ops[i] << std::endl;
            }
        },
        "Print hello world");

    CliApp = new Cli(std::move(rootMenu));
    // global exit action
    CliApp->ExitAction([](auto& out) { out << "Goodbye and thanks for all the fish.\n"; });

    input = new CliFileSession(*CliApp);
    CliApp->WrongCommandHandler([&](std::ostream& out, const std::string& cmd)
        {
            interpreter.execute(cmd);
        });
}

int main(int argc, char** argv)
{
    CLI::App app("ibp - Interactive Binary Processing");

    std::string filename, out_dir;

    app.add_option("-i,--input-file", filename, "Input file");

    app.set_help_flag("-h,--help", "Print this help message");

    // 解析
    try {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    init_interactive();
    if (app.get_options().empty())
    {
        input->Start();
    }
    else if (!filename.empty())
    {
        run_file(filename);
    }
    return 0;
}