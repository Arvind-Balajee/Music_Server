#pragma once

#include <string>
#include <vector>

namespace musicbox::sync {

// Transfers one local file to the server. Kept as an interface (Plan.md §25) so
// SyncEngine's push logic can be tested without a network round trip.
class UploadTransport {
public:
    virtual ~UploadTransport() = default;

    // Uploads the file at absoluteLocalPath, tagged with relativePath (its
    // identity in the library). Returns true on success. Implementations should
    // not throw for ordinary transfer failures (return false instead); throwing
    // is reserved for programmer errors.
    virtual bool uploadFile(const std::string& relativePath, const std::string& absoluteLocalPath) = 0;
};

// No-op transport that only records what would have been uploaded. Used by
// tests and by `--dry-run`-style callers.
class NullUploadTransport final : public UploadTransport {
public:
    bool uploadFile(const std::string& relativePath, const std::string& absoluteLocalPath) override {
        (void)absoluteLocalPath;
        uploaded_.push_back(relativePath);
        return true;
    }

    [[nodiscard]] const std::vector<std::string>& uploaded() const noexcept { return uploaded_; }

private:
    std::vector<std::string> uploaded_;
};

// POSTs the raw file bytes to the *proposed* POST /api/v1/sync/tracks endpoint
// (docs/api.md; NOT YET IMPLEMENTED server-side). Throws nothing for ordinary
// transfer failures; uploadFile() returns false instead so a `push` run can
// continue with the remaining files.
//
// Resumability (Plan.md §12 "design resumable transfers if practical") is NOT
// implemented: each call sends the whole file in one request. See the sync
// agent's final report for how a byte-range-resume scheme would slot in here
// (it would need a proposed server-side endpoint to query how many bytes of a
// given relativePath it already has, mirroring the streaming Range support in
// docs/http.md).
class HttpUploadTransport final : public UploadTransport {
public:
    explicit HttpUploadTransport(std::string serverHost, int defaultPort = 8080);

    bool uploadFile(const std::string& relativePath, const std::string& absoluteLocalPath) override;

private:
    std::string serverHost_;
    int defaultPort_;
};

} // namespace musicbox::sync
