#pragma once

#include "../builder.hpp"
#include "../core.hpp"
#include <sstream>

namespace co::http {

// =============================================================================
// Request Builder Implementation
// =============================================================================

inline request_builder& request_builder::method(enum method m) {
    req_.method_type = m;
    return *this;
}

inline request_builder& request_builder::method(std::string_view m) {
    req_.set_method(m);
    return *this;
}

inline request_builder& request_builder::uri(std::string u) {
    req_.uri = std::move(u);
    req_.target = req_.uri; // For HTTP/2 compatibility
    return *this;
}

inline request_builder& request_builder::version(enum version v) {
    req_.protocol_version = v;
    return *this;
}

inline request_builder& request_builder::header(std::string name, std::string value, bool sensitive) {
    req_.add_header(std::move(name), std::move(value), sensitive);
    return *this;
}

inline request_builder& request_builder::body(std::string b) {
    req_.body = std::move(b);
    return *this;
}

// =============================================================================
// Common HTTP Methods
// =============================================================================

inline request_builder& request_builder::GET(std::string uri) {
    return method(method::get).uri(std::move(uri));
}

inline request_builder& request_builder::POST(std::string uri) {
    return method(method::post).uri(std::move(uri));
}

inline request_builder& request_builder::PUT(std::string uri) {
    return method(method::put).uri(std::move(uri));
}

inline request_builder& request_builder::DELETE(std::string uri) {
    return method(method::delete_).uri(std::move(uri));
}

inline request_builder& request_builder::HEAD(std::string uri) {
    return method(method::head).uri(std::move(uri));
}

inline request_builder& request_builder::OPTIONS(std::string uri) {
    return method(method::options).uri(std::move(uri));
}

inline request_builder& request_builder::PATCH(std::string uri) {
    return method(method::patch).uri(std::move(uri));
}

// =============================================================================
// Common Headers
// =============================================================================

inline request_builder& request_builder::host(std::string h) {
    return header("Host", std::move(h));
}

inline request_builder& request_builder::user_agent(std::string ua) {
    return header("User-Agent", std::move(ua));
}

inline request_builder& request_builder::content_type(std::string ct) {
    return header("Content-Type", std::move(ct));
}

inline request_builder& request_builder::authorization(std::string auth) {
    return header("Authorization", std::move(auth), true); // Sensitive header
}

inline request_builder& request_builder::accept(std::string accept) {
    return header("Accept", std::move(accept));
}

inline request_builder& request_builder::cookie(std::string cookie) {
    return header("Cookie", std::move(cookie), true); // Sensitive header
}

inline request_builder& request_builder::referer(std::string ref) {
    return header("Referer", std::move(ref));
}

inline request_builder& request_builder::origin(std::string origin) {
    return header("Origin", std::move(origin));
}

// =============================================================================
// Content Helpers
// =============================================================================

inline request_builder& request_builder::json_body(std::string json) {
    return content_type("application/json").body(std::move(json));
}

inline request_builder& request_builder::form_body(const std::unordered_map<std::string, std::string>& form_data) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& [key, value] : form_data) {
        if (!first) oss << "&";
        oss << key << "=" << value; // TODO: URL encode
        first = false;
    }
    return content_type("application/x-www-form-urlencoded").body(oss.str());
}

inline request_builder& request_builder::text_body(std::string text) {
    return content_type("text/plain").body(std::move(text));
}

inline request request_builder::build() const {
    return req_;
}

// =============================================================================
// Response Builder Implementation
// =============================================================================

inline response_builder::response_builder() {
    resp_.protocol_version = version::http_1_1;
    resp_.status_code = 200;
    resp_.reason_phrase = "OK";
}

inline response_builder& response_builder::status(unsigned int code) {
    resp_.status_code = code;
    
    // Set default reason phrase
    switch (code) {
        case 200: resp_.reason_phrase = "OK"; break;
        case 201: resp_.reason_phrase = "Created"; break;
        case 202: resp_.reason_phrase = "Accepted"; break;
        case 204: resp_.reason_phrase = "No Content"; break;
        case 301: resp_.reason_phrase = "Moved Permanently"; break;
        case 302: resp_.reason_phrase = "Found"; break;
        case 304: resp_.reason_phrase = "Not Modified"; break;
        case 400: resp_.reason_phrase = "Bad Request"; break;
        case 401: resp_.reason_phrase = "Unauthorized"; break;
        case 403: resp_.reason_phrase = "Forbidden"; break;
        case 404: resp_.reason_phrase = "Not Found"; break;
        case 405: resp_.reason_phrase = "Method Not Allowed"; break;
        case 409: resp_.reason_phrase = "Conflict"; break;
        case 500: resp_.reason_phrase = "Internal Server Error"; break;
        case 501: resp_.reason_phrase = "Not Implemented"; break;
        case 502: resp_.reason_phrase = "Bad Gateway"; break;
        case 503: resp_.reason_phrase = "Service Unavailable"; break;
        default: resp_.reason_phrase = "Unknown"; break;
    }
    
    return *this;
}

inline response_builder& response_builder::status(unsigned int code, std::string reason) {
    resp_.status_code = code;
    resp_.reason_phrase = std::move(reason);
    return *this;
}

inline response_builder& response_builder::version(enum version v) {
    resp_.protocol_version = v;
    return *this;
}

inline response_builder& response_builder::header(std::string name, std::string value, bool sensitive) {
    resp_.add_header(std::move(name), std::move(value), sensitive);
    return *this;
}

inline response_builder& response_builder::body(std::string b) {
    resp_.body = std::move(b);
    return *this;
}

// =============================================================================
// Common Status Codes
// =============================================================================

inline response_builder& response_builder::ok() {
    return status(200);
}

inline response_builder& response_builder::created() {
    return status(201);
}

inline response_builder& response_builder::accepted() {
    return status(202);
}

inline response_builder& response_builder::no_content() {
    return status(204);
}

inline response_builder& response_builder::moved_permanently(std::string location) {
    return status(301).header("Location", std::move(location));
}

inline response_builder& response_builder::found(std::string location) {
    return status(302).header("Location", std::move(location));
}

inline response_builder& response_builder::not_modified() {
    return status(304);
}

inline response_builder& response_builder::bad_request() {
    return status(400);
}

inline response_builder& response_builder::unauthorized() {
    return status(401);
}

inline response_builder& response_builder::forbidden() {
    return status(403);
}

inline response_builder& response_builder::not_found() {
    return status(404);
}

inline response_builder& response_builder::method_not_allowed() {
    return status(405);
}

inline response_builder& response_builder::conflict() {
    return status(409);
}

inline response_builder& response_builder::internal_server_error() {
    return status(500);
}

inline response_builder& response_builder::not_implemented() {
    return status(501);
}

inline response_builder& response_builder::bad_gateway() {
    return status(502);
}

inline response_builder& response_builder::service_unavailable() {
    return status(503);
}

// =============================================================================
// Common Response Headers
// =============================================================================

inline response_builder& response_builder::content_type(std::string ct) {
    return header("Content-Type", std::move(ct));
}

inline response_builder& response_builder::content_length(size_t length) {
    return header("Content-Length", std::to_string(length));
}

inline response_builder& response_builder::server(std::string s) {
    return header("Server", std::move(s));
}

inline response_builder& response_builder::cache_control(std::string cc) {
    return header("Cache-Control", std::move(cc));
}

inline response_builder& response_builder::location(std::string loc) {
    return header("Location", std::move(loc));
}

inline response_builder& response_builder::set_cookie(std::string cookie) {
    return header("Set-Cookie", std::move(cookie));
}

inline response_builder& response_builder::cors_origin(std::string origin) {
    return header("Access-Control-Allow-Origin", std::move(origin));
}

// =============================================================================
// Content Helpers
// =============================================================================

inline response_builder& response_builder::json_body(std::string json) {
    return content_type("application/json").body(std::move(json));
}

inline response_builder& response_builder::html_body(std::string html) {
    return content_type("text/html").body(std::move(html));
}

inline response_builder& response_builder::text_body(std::string text) {
    return content_type("text/plain").body(std::move(text));
}

inline response response_builder::build() const {
    return resp_;
}

} // namespace co::http