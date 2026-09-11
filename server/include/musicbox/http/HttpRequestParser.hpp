#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "musicbox/http/HttpRequest.hpp"
#include "musicbox/http/HttpStatus.hpp"

namespace musicbox::http {

// Outcome of one HttpRequestParser::next() call.
enum class ParseStatus {
    NeedMoreData, // not enough buffered data yet; feed() more and call next() again
    Complete,     // one full HttpRequest was parsed
    Error,        // malformed input / a configured limit was exceeded; the caller
                  // should respond with `errorStatus` and close the connection
};

struct ParseResult {
    ParseStatus status = ParseStatus::NeedMoreData;
    HttpRequest request;                             // valid iff status == Complete
    HttpStatus errorStatus = HttpStatus::BadRequest; // valid iff status == Error
};

// Incremental HTTP/1.1 request parser: a state machine
// (RequestLine -> Headers -> Body/Done) fed arbitrary byte chunks via feed().
//
// No assumption is made about how the bytes handed to feed() align with
// message boundaries: a request line, a header line, or even a lone "\r\n"
// may be split across an arbitrary number of feed() calls at any byte offset
// (this is the fragmentation scenario in Plan.md §8). The parser keeps only
// the not-yet-consumed bytes in an internal buffer, so a request split across
// many small feed() calls is handled the same way as one delivered whole.
//
// Pipelining: a single feed() call may deliver more than one complete request
// (e.g. two requests back-to-back on a keep-alive connection). next() consumes
// exactly the bytes of the request it parses; call it in a loop until it
// returns NeedMoreData to drain every request already buffered.
//
// Usage:
//
//   HttpRequestParser parser;
//   parser.feed(chunkFromRecv);
//   for (;;) {
//       ParseResult result = parser.next();
//       if (result.status == ParseStatus::NeedMoreData) break; // wait for more bytes
//       if (result.status == ParseStatus::Error) {
//           // respond with result.errorStatus, then close the connection.
//           break;
//       }
//       handleRequest(result.request); // ParseStatus::Complete
//       // loop again: another pipelined request may already be buffered
//   }
//
// A parser that has returned Error must be discarded — it does not attempt to
// resynchronize on malformed input, matching the "reject, don't guess" posture
// in Plan.md §17/§31.
//
// Not thread-safe: exactly one thread (the owning TcpConnection's event-loop
// thread) should drive a given parser instance, consistent with the threading
// model in docs/architecture.md §4.
class HttpRequestParser {
public:
    // Resource-exhaustion limits, enforced before any unbounded buffering
    // occurs (Plan.md §17 "resource exhaustion", §31 "load entire ... into
    // memory"). Exceeding any of these yields ParseStatus::Error(BadRequest)
    // rather than growing the internal buffer without bound.
    struct Limits {
        std::size_t maxRequestLineLength = 8 * 1024;     // bytes, excluding the CRLF
        std::size_t maxHeaderBytes = 16 * 1024;          // total header-section bytes
        std::size_t maxHeaderCount = 100;                // number of header lines
        std::uint64_t maxBodySize = 10ULL * 1024 * 1024; // bytes (10 MiB)
    };

    HttpRequestParser();
    explicit HttpRequestParser(Limits limits);

    // Appends bytes received from a socket recv() (or a test harness) to the
    // internal buffer. Does not itself parse — call next() afterwards.
    void feed(std::span<const std::byte> data);

    // Attempts to parse the next complete request out of the buffered bytes.
    // Call in a loop after feed() until it returns NeedMoreData: a single
    // feed() call may have delivered more than one pipelined request, and a
    // single next() call fully drains as much of the buffer as already
    // available (advancing through RequestLine -> Headers -> Body internally)
    // before returning.
    [[nodiscard]] ParseResult next();

private:
    enum class State { RequestLine, Headers, Body };

    // Parses the headers-complete transition: decides whether a body follows
    // (via Content-Length) and validates it against limits_.maxBodySize.
    // Returns the error status to report, or nullopt on success (state_ is
    // advanced to Body either way; a zero-length body completes immediately
    // the next time the Body branch of next() runs).
    [[nodiscard]] std::optional<HttpStatus> finishHeaders();

    [[nodiscard]] static ParseResult needMoreData();
    [[nodiscard]] static ParseResult error(HttpStatus status);
    ParseResult finalize();
    void resetForNextRequest();

    Limits limits_;
    std::string buffer_; // unconsumed bytes; std::byte data reinterpreted as char

    State state_ = State::RequestLine;
    HttpRequest inProgress_;
    std::size_t headerBytesConsumed_ = 0;
    std::size_t headerCount_ = 0;
    std::uint64_t bodyLength_ = 0;
};

} // namespace musicbox::http
