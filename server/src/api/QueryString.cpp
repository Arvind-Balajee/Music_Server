#include "QueryString.hpp"

#include <charconv>

namespace musicbox::api {

namespace {

bool isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int hexValue(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

// Query-string decoding: '+' means space (form-encoding convention), and an
// invalid/truncated "%XX" escape is left as literal characters rather than
// failing the whole parse -- a client typo in one query value shouldn't make
// every other value on the request unreadable.
std::string decodeQueryComponent(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '+') {
            out.push_back(' ');
        } else if (c == '%' && i + 2 < in.size() && isHexDigit(in[i + 1]) &&
                   isHexDigit(in[i + 2])) {
            out.push_back(static_cast<char>((hexValue(in[i + 1]) << 4) | hexValue(in[i + 2])));
            i += 2;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

} // namespace

std::unordered_map<std::string, std::string> parseQueryString(std::string_view query) {
    std::unordered_map<std::string, std::string> result;
    std::size_t pos = 0;
    while (pos <= query.size()) {
        std::size_t ampersand = query.find('&', pos);
        std::string_view pair = query.substr(
            pos, ampersand == std::string_view::npos ? std::string_view::npos : ampersand - pos);
        if (!pair.empty()) {
            std::size_t equals = pair.find('=');
            std::string_view rawKey =
                equals == std::string_view::npos ? pair : pair.substr(0, equals);
            std::string_view rawValue =
                equals == std::string_view::npos ? std::string_view{} : pair.substr(equals + 1);
            result[decodeQueryComponent(rawKey)] = decodeQueryComponent(rawValue);
        }
        if (ampersand == std::string_view::npos) {
            break;
        }
        pos = ampersand + 1;
    }
    return result;
}

std::optional<std::int64_t> queryInt(const std::unordered_map<std::string, std::string>& query,
                                     const std::string& key, bool* invalid) {
    auto it = query.find(key);
    if (it == query.end() || it->second.empty()) {
        return std::nullopt;
    }
    std::int64_t value = 0;
    const auto& s = it->second;
    auto result = std::from_chars(s.data(), s.data() + s.size(), value);
    if (result.ec != std::errc{} || result.ptr != s.data() + s.size()) {
        if (invalid != nullptr) {
            *invalid = true;
        }
        return std::nullopt;
    }
    return value;
}

} // namespace musicbox::api
