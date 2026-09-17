#include "musicbox/http/HttpStatus.hpp"

namespace musicbox::http {

const char* reasonPhrase(HttpStatus status) noexcept {
    switch (status) {
    case HttpStatus::Ok:
        return "OK";
    case HttpStatus::Created:
        return "Created";
    case HttpStatus::NoContent:
        return "No Content";
    case HttpStatus::PartialContent:
        return "Partial Content";
    case HttpStatus::BadRequest:
        return "Bad Request";
    case HttpStatus::NotFound:
        return "Not Found";
    case HttpStatus::MethodNotAllowed:
        return "Method Not Allowed";
    case HttpStatus::RangeNotSatisfiable:
        return "Range Not Satisfiable";
    case HttpStatus::InternalServerError:
        return "Internal Server Error";
    }
    return "Unknown Status";
}

} // namespace musicbox::http
