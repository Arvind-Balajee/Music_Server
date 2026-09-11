#include <catch2/catch_test_macros.hpp>

#include "musicbox/library/MetadataExtractor.hpp"

#include "TempDirFixture.hpp"

using namespace musicbox::library;
using musicbox::library::test::TempDirFixture;

TEST_CASE("MetadataExtractor reads duration and codec from a real WAV file via TagLib",
          "[library][metadata]") {
    TempDirFixture dir;
    const auto path = dir.writeMinimalWav("silence.wav", /*durationMs=*/1000);

    auto extractor = makeDefaultMetadataExtractor();
    const auto metadata = extractor->extract(path.string());

    REQUIRE(metadata.has_value());
    CHECK(metadata->codec == "wav");
    // TagLib's WAV duration is derived from data size / byte rate; allow a
    // small tolerance rather than asserting exact equality with our own
    // computation of the same thing.
    CHECK(metadata->durationMs > 900);
    CHECK(metadata->durationMs < 1100);
    // An untagged WAV has no title frame -- extractor falls back to the
    // filename stem rather than leaving it empty.
    CHECK(metadata->title == "silence");
}

TEST_CASE("MetadataExtractor returns nullopt for an unparsable file", "[library][metadata]") {
    TempDirFixture dir;
    // .mp3 is a poor choice for this case: TagLib's MPEG backend happily
    // returns a valid (empty) ID3 tag even for content with no MPEG frames at
    // all. .flac requires a "fLaC" magic marker its RIFF-style parser checks
    // up front, so garbage content reliably fails to open.
    const auto path = dir.writeFile("not-audio.flac", "this is not a valid flac file");

    auto extractor = makeDefaultMetadataExtractor();
    CHECK_FALSE(extractor->extract(path.string()).has_value());
}

TEST_CASE("MetadataExtractor returns nullopt for a nonexistent file", "[library][metadata]") {
    auto extractor = makeDefaultMetadataExtractor();
    CHECK_FALSE(extractor->extract("/nonexistent/path/does-not-exist.wav").has_value());
}
