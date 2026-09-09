#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

namespace {

constexpr std::string_view kVersion = "0.1.0";

void printUsage() {
    std::cout << "Usage: musicbox-server <command> [--config <path>]\n"
                 "Commands:\n"
                 "  run       Start the server (event loop + HTTP API)\n"
                 "  scan      Run the library scanner once and exit\n"
                 "  status    Query a running instance's /api/v1/status\n"
                 "  doctor    Validate config, DB, and library root permissions\n"
                 "  version   Print the server version\n";
}

int runCommand(std::string_view command) {
    if (command == "version") {
        std::cout << "musicbox-server " << kVersion << '\n';
        return 0;
    }
    if (command == "run" || command == "scan" || command == "status" || command == "doctor") {
        // Placeholder until Agent 1 (networking), Agent 3 (library/db) and Agent 4
        // (API) land their pieces — see docs/architecture.md for the composition
        // root this will wire together (EventLoop + Router + SqliteRepositories).
        std::cerr << "musicbox-server: '" << command
                  << "' is not implemented yet (see Plan.md milestones 1-6)\n";
        return 1;
    }
    std::cerr << "musicbox-server: unknown command '" << command << "'\n";
    printUsage();
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }
    return runCommand(argv[1]);
}
