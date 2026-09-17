# HTTP Layer

Owner: Agent 2 (HTTP + Streaming). Contract for `server/include/musicbox/http/`.

## Request/Response Types (`shared` between parser, router, handlers)

```cpp
struct HttpRequest {
    std::string method;   // "GET", "POST", "HEAD", ...
    std::string path;     // decoded, without query string
    std::string query;    // raw query string, "" if none
    std::string version;  // "HTTP/1.1"
    std::unordered_map<std::string, std::string> headers; // lower-cased keys
    std::vector<std::byte> body;
};

class ResponseBody {
public:
    virtual ~ResponseBody() = default;
    // Fills up to destination.size() bytes; returns bytes written, 0 at EOF.
    virtual std::size_t read(std::span<std::byte> destination) = 0;
    // Total bytes remaining, if known (used for Content-Length); nullopt if unknown.
    virtual std::optional<std::uint64_t> remaining() const = 0;
};

struct HttpResponse {
    int statusCode = 200;
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::byte> body;                    // used when streamingBody is null
    std::unique_ptr<ResponseBody> streamingBody;     // used for large/streamed payloads
};
```

`HttpResponse` is move-only (owns a `unique_ptr`). A handler returns exactly one of
`body` (small, fully in memory — JSON, error payloads) or `streamingBody` (audio,
artwork) — never both.

## Incremental Parsing

The parser is a state machine fed arbitrary byte chunks from `TcpConnection`. It
**must not** assume any alignment between TCP segments and HTTP message boundaries:
a request line, a header, or even `\r\n` may be split across multiple `recv()`
calls. States:

```text
RequestLine -> Headers -> (Body | Done)
```

The parser owns an internal buffer of unconsumed bytes and returns one of:

* `NeedMoreData` — keep buffering.
* `Complete(HttpRequest)` — one full request parsed; remaining buffered bytes (from
  the next pipelined request) are preserved for the next call.
* `Error(HttpStatus)` — malformed input; the connection responds with that status
  and closes.

Limits (protect against resource exhaustion): max request line length, max header
bytes, max header count, max body size for non-streaming endpoints — all
configurable, all enforced before allocating unbounded buffers.

## HttpRequestParser (`server/include/musicbox/http/HttpRequestParser.hpp`)

Concrete incremental parser implementing the state machine described above.
Added by Agent 2 alongside the implementation; this section documents its
contract.

```cpp
enum class ParseStatus { NeedMoreData, Complete, Error };

struct ParseResult {
    ParseStatus status = ParseStatus::NeedMoreData;
    HttpRequest request;                            // valid iff status == Complete
    HttpStatus errorStatus = HttpStatus::BadRequest; // valid iff status == Error
};

class HttpRequestParser {
public:
    struct Limits {
        std::size_t maxRequestLineLength = 8 * 1024;
        std::size_t maxHeaderBytes = 16 * 1024;
        std::size_t maxHeaderCount = 100;
        std::uint64_t maxBodySize = 10ULL * 1024 * 1024;
    };

    HttpRequestParser();
    explicit HttpRequestParser(Limits limits);

    void feed(std::span<const std::byte> data);
    [[nodiscard]] ParseResult next();
};
```

Contract:

* `feed()` appends bytes to an internal buffer; it never parses. This lets a
  caller hand it exactly what `recv()` returned, whatever that chunk boundary
  happens to be — a request line, a header line, or a lone `\r\n` may be split
  across any number of `feed()` calls at any byte offset.
* `next()` drains as much of the buffered bytes as already available,
  advancing through `RequestLine -> Headers -> Body` internally, and returns:
  * `NeedMoreData` — call `feed()` again before calling `next()` again.
  * `Complete` — one full `HttpRequest` was parsed and removed from the
    buffer; any remaining bytes (e.g. a pipelined next request delivered in
    the same `feed()` call) stay buffered. Call `next()` again immediately to
    check for another complete request before waiting on more socket data.
  * `Error` — malformed input or a configured limit was exceeded; respond
    with `errorStatus` and close the connection. The parser instance must be
    discarded — it does not attempt to resynchronize.
* Limits are enforced as soon as they *can* be checked, not after buffering
  the offending data: an over-length request line or header section is
  rejected while it is still being accumulated (checked against the running
  buffer size each time a terminating `\r\n` isn't yet found), and an
  over-limit `Content-Length` is rejected immediately after the header
  section completes, before any body bytes are buffered.
* `Transfer-Encoding` (chunked bodies) is not supported in the MVP and is
  rejected with `400 Bad Request` rather than mishandled — request bodies use
  `Content-Length` only.
* The request-target's path component is percent-decoded into
  `HttpRequest::path`; the query string (if any) is kept raw and undecoded in
  `HttpRequest::query`, matching the `HttpRequest` contract above.
* Not thread-safe — one parser instance is driven by exactly one thread
  (the owning connection's event-loop thread), consistent with
  `docs/architecture.md` §4.

## Router

```cpp
using RouteParams = std::unordered_map<std::string, std::string>;
using RequestHandler = std::function<HttpResponse(const HttpRequest&, const RouteParams&)>;

class Router {
public:
    virtual ~Router() = default;
    virtual void addRoute(std::string method, std::string pattern, RequestHandler handler) = 0;
    virtual HttpResponse dispatch(const HttpRequest& request) const = 0;
};
```

`pattern` supports `{name}` path segments (e.g. `/api/v1/tracks/{id}`). No route
matches unless both method and full path match; unmatched path -> 404, matched path
with no matching method -> 405.

**HEAD is handled generically**, not by registering it per route: `PathRouter::dispatch()`
matches a `HEAD` request against whatever `GET` route handles the same path and
runs that handler unmodified (RFC 7231 §4.3.2 -- HEAD is identical to GET except
no body is sent). The returned `HttpResponse` still has its body/streamingBody
fully populated, since headers like `Content-Length`/`Content-Range` need to
reflect exactly what the equivalent `GET` would have sent; the composition root
(`writeHttpResponse()` in `server/src/main.cpp`) is what actually omits the body
bytes on the wire for a HEAD request. Routes never need to be registered under
`"HEAD"` explicitly -- doing so would have no effect, since dispatch never
matches a request's real method against a registered `HEAD` route.

## Range Requests

Header: `Range: bytes=<spec>` where `<spec>` is one of:

```text
bytes=0-999        -> first 1000 bytes
bytes=1000-        -> byte 1000 to EOF
bytes=-500         -> last 500 bytes
```

```cpp
struct ByteRange { std::uint64_t start; std::uint64_t end; }; // inclusive, resolved

enum class RangeParseOutcome { NoRangeHeader, Satisfiable, NotSatisfiable };
struct RangeParseResult { RangeParseOutcome outcome; ByteRange range; };

// Resolves and validates a Range header value against a known resource size.
// C++20 target avoids std::expected (C++23); a tagged result struct is used instead.
RangeParseResult parseRange(std::string_view headerValue, std::uint64_t resourceSize);
```

Rules:

* No `Range` header -> `200 OK`, full body, response includes `Accept-Ranges: bytes`.
* Valid range -> `206 Partial Content` with:

  ```http
  Accept-Ranges: bytes
  Content-Range: bytes <start>-<end>/<resourceSize>
  Content-Length: <end-start+1>
  ```
* `start > end`, `start >= resourceSize`, or unparsable spec -> `416 Range Not
  Satisfiable` with `Content-Range: bytes */<resourceSize>`.
* Multi-range (`bytes=0-99,200-299`) is **not** required for the MVP; reject with
  `416` (documented limitation, not a bug).

## Streaming Implementation

`FileStreamResponseBody` (concrete `ResponseBody`) wraps an open file descriptor and
a `[offset, offset+length)` window. `read()` calls `pread()` at the current cursor —
never a whole-file `read()`/`fstream` slurp. Where available, the connection layer
may instead use `sendfile()` directly from the fd to the socket for the common
"stream the whole remaining range" case, with `pread`-based `ResponseBody` as the
portable fallback (see `docs/architecture.md` §7 for the zero-copy vs. fallback
split). Either path is driven by the event loop's writability events — a slow
client backs up the outbound buffer, not server memory.

## Status Codes Required for MVP

`200, 206, 400, 404, 405, 416, 500` — see `Plan.md` §8 for the full list this maps
to (`Content-Length`, `Content-Type`, `Connection`, `Range`, `Content-Range`,
`Accept-Ranges` headers). Agent 4 (docs/api.md) additively extended
`HttpStatus` with `201 Created`/`204 No Content` for the playlist CRUD
endpoints, which weren't part of this streaming-focused original set.
