// musicbox-sync: desktop sync client CLI (Plan.md §12).
//
//   musicbox-sync <localDir> <serverHost>   scan + push in one shot
//   musicbox-sync push <localDir> [host]    scan + push (host defaults to "musicbox.local")
//   musicbox-sync status [host]             check server reachability + manifest size
//   musicbox-sync verify <localDir> [host]  full re-hash diff (catches silent corruption)
//   musicbox-sync list <localDir>           list local supported audio files (no network)
//
// There is currently no persisted config, so localDir/host must be passed
// explicitly to every invocation that needs them (see the agent report's
// "known limitations" for a proposed follow-up: a small state file remembering
// the last-used pair, analogous to config/musicbox.toml on the server side).
#include <iostream>
#include <string>
#include <vector>

#include "musicbox/sync/FileHasher.hpp"
#include "musicbox/sync/LocalScanner.hpp"
#include "musicbox/sync/ServerManifestSource.hpp"
#include "musicbox/sync/SyncEngine.hpp"
#include "musicbox/sync/UploadTransport.hpp"

namespace {

constexpr const char* kDefaultHost = "musicbox.local";
constexpr int kDefaultPort = 8080;

void printUsage() {
    std::cerr << "Usage:\n"
              << "  musicbox-sync <localDir> <serverHost>   scan + push in one shot\n"
              << "  musicbox-sync push <localDir> [host]    scan + push (host defaults to \""
              << kDefaultHost << "\")\n"
              << "  musicbox-sync status [host]             check server reachability\n"
              << "  musicbox-sync verify <localDir> [host]  full re-hash diff\n"
              << "  musicbox-sync list <localDir>            list local audio files\n";
}

int runPush(const std::string& localDir, const std::string& host) {
    musicbox::sync::HttpServerManifestSource manifestSource(host, kDefaultPort);
    musicbox::sync::HttpUploadTransport uploadTransport(host, kDefaultPort);
    musicbox::sync::Sha256FileHasher hasher;
    musicbox::sync::SyncEngine engine(manifestSource, uploadTransport, hasher);

    try {
        const musicbox::sync::PushSummary summary = engine.push(localDir);
        std::cout << "push " << localDir << " -> " << host << "\n"
                  << "  new:       " << summary.newCount << "\n"
                  << "  modified:  " << summary.modifiedCount << "\n"
                  << "  deleted:   " << summary.deletedCount
                  << " (not removed from server; see docs/api.md)\n"
                  << "  unchanged: " << summary.unchangedCount << "\n"
                  << "  uploaded:  " << summary.uploadedOk << " ok, " << summary.uploadFailed
                  << " failed\n";
        return summary.uploadFailed == 0 ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "push failed: " << e.what() << "\n";
        return 1;
    }
}

int runStatus(const std::string& host) {
    musicbox::sync::HttpServerManifestSource manifestSource(host, kDefaultPort);
    try {
        const auto manifest = manifestSource.fetchManifest();
        std::cout << "server " << host << ": reachable, " << manifest.size()
                  << " tracks in manifest\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "server " << host << ": unreachable (" << e.what() << ")\n";
        return 1;
    }
}

int runVerify(const std::string& localDir, const std::string& host) {
    musicbox::sync::HttpServerManifestSource manifestSource(host, kDefaultPort);
    musicbox::sync::NullUploadTransport uploadTransport; // verify never uploads
    musicbox::sync::Sha256FileHasher hasher;
    musicbox::sync::SyncEngine engine(manifestSource, uploadTransport, hasher);

    try {
        const musicbox::sync::DiffResult result = engine.computeDiff(localDir, /*forceHash=*/true);
        std::cout << "verify " << localDir << " against " << host << "\n"
                  << "  unchanged: " << result.unchangedFiles.size() << "\n"
                  << "  new:       " << result.newFiles.size() << "\n"
                  << "  modified:  " << result.modifiedFiles.size()
                  << " (content differs from server)\n"
                  << "  deleted:   " << result.deletedFiles.size()
                  << " (present on server, missing locally)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "verify failed: " << e.what() << "\n";
        return 1;
    }
}

int runList(const std::string& localDir) {
    const musicbox::sync::LocalScanner scanner;
    try {
        const auto entries = scanner.scan(localDir);
        for (const auto& entry : entries) {
            std::cout << entry.relativePath << "\t" << entry.sizeBytes << " bytes\n";
        }
        std::cout << entries.size() << " file(s)\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "list failed: " << e.what() << "\n";
        return 1;
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        printUsage();
        return 1;
    }

    const std::string& first = args[0];

    if (first == "status") {
        const std::string host = args.size() > 1 ? args[1] : kDefaultHost;
        return runStatus(host);
    }
    if (first == "push") {
        if (args.size() < 2) {
            printUsage();
            return 1;
        }
        const std::string host = args.size() > 2 ? args[2] : kDefaultHost;
        return runPush(args[1], host);
    }
    if (first == "verify") {
        if (args.size() < 2) {
            printUsage();
            return 1;
        }
        const std::string host = args.size() > 2 ? args[2] : kDefaultHost;
        return runVerify(args[1], host);
    }
    if (first == "list") {
        if (args.size() < 2) {
            printUsage();
            return 1;
        }
        return runList(args[1]);
    }

    // Shorthand: `musicbox-sync <localDir> <serverHost>` is scan + push in one shot.
    if (args.size() >= 2) {
        return runPush(args[0], args[1]);
    }

    printUsage();
    return 1;
}
