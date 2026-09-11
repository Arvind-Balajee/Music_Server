#include "musicbox/http/HttpRequestParser.hpp"

#include <cctype>
#include <charconv>
#include <cstring>

namespace musicbox::http {

namespace {

[[nodiscard]] bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

[[nodiscard]] int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

// Percent-decodes a request-target path component. '+' is left untouched
// (that is a form-encoding convention for query strings, not paths). Returns
// nullopt on a truncated/invalid "%XX" escape.
[[nodiscard]] std::optional<std::string> percentDecode(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '%') {
            if (i + 2 >= in.size() || !isHexDigit(in[i + 1]) || !isHexDigit(in[i + 2])) {
                return std::nullopt;
            }
            out.push_back(static_cast<char>((hexValue(in[i + 1]) << 4) | hexValue(in[i + 2])));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

[[nodiscard]] std::string_view trimOws(std::string_view s) {
    std::size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t')) {
        --end;
    }
    return s.substr(start, end - start);
}

[[nodiscard]] std::string toLowerCopy(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

} // namespace

HttpRequestParser::HttpRequestParser() : HttpRequestParser(Limits{}) {}

HttpRequestParser::HttpRequestParser(Limits limits) : limits_(limits) {}

void HttpRequestParser::feed(std::span<const std::byte> data) {
    if (!data.empty()) {
        buffer_.append(reinterpret_cast<const char*>(data.data()), data.size());
    }
}

ParseResult HttpRequestParser::needMoreData() {
    return ParseResult{ParseStatus::NeedMoreData, HttpRequest{}, HttpStatus::BadRequest};
}

ParseResult HttpRequestParser::error(HttpStatus status) {
    return ParseResult{ParseStatus::Error, HttpRequest{}, status};
}

ParseResult HttpRequestParser::finalize() {
    ParseResult result;
    result.status = ParseStatus::Complete;
    result.request = std::move(inProgress_);
    resetForNextRequest();
    return result;
}

void HttpRequestParser::resetForNextRequest() {
    state_ = State::RequestLine;
    inProgress_ = HttpRequest{};
    headerBytesConsumed_ = 0;
    headerCount_ = 0;
    bodyLength_ = 0;
}

std::optional<HttpStatus> HttpRequestParser::finishHeaders() {
    // Chunked transfer encoding is not required for the MVP (docs/http.md);
    // rather than mishandle it, reject explicitly.
    if (inProgress_.header("transfer-encoding") != nullptr) {
        return HttpStatus::BadRequest;
    }

    const std::string* contentLength = inProgress_.header("content-length");
    if (contentLength == nullptr) {
        bodyLength_ = 0;
        state_ = State::Body;
        return std::nullopt;
    }

    std::uint64_t length = 0;
    const char* begin = contentLength->data();
    const char* end = contentLength->data() + contentLength->size();
    auto [ptr, ec] = std::from_chars(begin, end, length);
    if (ec != std::errc{} || ptr != end) {
        return HttpStatus::BadRequest;
    }
    if (length > limits_.maxBodySize) {
        return HttpStatus::BadRequest;
    }

    bodyLength_ = length;
    state_ = State::Body;
    return std::nullopt;
}

ParseResult HttpRequestParser::next() {
    for (;;) {
        switch (state_) {
        case State::RequestLine: {
            const auto pos = buffer_.find("\r\n");
            if (pos == std::string::npos) {
                if (buffer_.size() > limits_.maxRequestLineLength) {
                    return error(HttpStatus::BadRequest);
                }
                return needMoreData();
            }
            if (pos > limits_.maxRequestLineLength) {
                return error(HttpStatus::BadRequest);
            }

            const std::string_view line(buffer_.data(), pos);

            const auto firstSpace = line.find(' ');
            if (firstSpace == std::string_view::npos) {
                return error(HttpStatus::BadRequest);
            }
            const auto secondSpace = line.find(' ', firstSpace + 1);
            if (secondSpace == std::string_view::npos) {
                return error(HttpStatus::BadRequest);
            }
            if (line.find(' ', secondSpace + 1) != std::string_view::npos) {
                // A literal space in the request-target (unencoded) or a
                // trailing token after the version -- malformed either way.
                return error(HttpStatus::BadRequest);
            }

            const std::string_view method = line.substr(0, firstSpace);
            const std::string_view target =
                line.substr(firstSpace + 1, secondSpace - firstSpace - 1);
            const std::string_view version = line.substr(secondSpace + 1);

            if (method.empty() || target.empty() || target.front() != '/') {
                return error(HttpStatus::BadRequest);
            }
            const bool versionWellFormed =
                version.size() == 8 && version.rfind("HTTP/", 0) == 0 &&
                std::isdigit(static_cast<unsigned char>(version[5])) != 0 && version[6] == '.' &&
                std::isdigit(static_cast<unsigned char>(version[7])) != 0;
            if (!versionWellFormed) {
                return error(HttpStatus::BadRequest);
            }

            const auto queryPos = target.find('?');
            const std::string_view rawPath =
                queryPos == std::string_view::npos ? target : target.substr(0, queryPos);
            const std::string_view rawQuery = queryPos == std::string_view::npos
                                                  ? std::string_view{}
                                                  : target.substr(queryPos + 1);

            auto decodedPath = percentDecode(rawPath);
            if (!decodedPath.has_value()) {
                return error(HttpStatus::BadRequest);
            }

            inProgress_ = HttpRequest{};
            inProgress_.method = std::string(method);
            inProgress_.path = std::move(*decodedPath);
            inProgress_.query = std::string(rawQuery);
            inProgress_.version = std::string(version);

            buffer_.erase(0, pos + 2);
            headerBytesConsumed_ = 0;
            headerCount_ = 0;
            state_ = State::Headers;
            continue;
        }

        case State::Headers: {
            const auto pos = buffer_.find("\r\n");
            if (pos == std::string::npos) {
                if (buffer_.size() > limits_.maxHeaderBytes) {
                    return error(HttpStatus::BadRequest);
                }
                return needMoreData();
            }

            if (pos == 0) {
                // Blank line: end of the header section.
                buffer_.erase(0, 2);
                if (auto errStatus = finishHeaders(); errStatus.has_value()) {
                    return error(*errStatus);
                }
                continue;
            }

            if (headerBytesConsumed_ + pos + 2 > limits_.maxHeaderBytes) {
                return error(HttpStatus::BadRequest);
            }
            if (headerCount_ + 1 > limits_.maxHeaderCount) {
                return error(HttpStatus::BadRequest);
            }

            const std::string_view line(buffer_.data(), pos);
            const auto colon = line.find(':');
            if (colon == std::string_view::npos) {
                return error(HttpStatus::BadRequest);
            }
            const std::string_view name = trimOws(line.substr(0, colon));
            const std::string_view value = trimOws(line.substr(colon + 1));
            if (name.empty()) {
                return error(HttpStatus::BadRequest);
            }
            inProgress_.headers[toLowerCopy(name)] = std::string(value);

            headerBytesConsumed_ += pos + 2;
            headerCount_ += 1;
            buffer_.erase(0, pos + 2);
            continue;
        }

        case State::Body: {
            if (buffer_.size() < bodyLength_) {
                return needMoreData();
            }
            inProgress_.body.resize(bodyLength_);
            if (bodyLength_ > 0) {
                std::memcpy(inProgress_.body.data(), buffer_.data(), bodyLength_);
                buffer_.erase(0, bodyLength_);
            }
            return finalize();
        }
        }
    }
}

} // namespace musicbox::http
