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
`Accept-Ranges` headers).
