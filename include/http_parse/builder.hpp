#pragma once

#include "core.hpp"
#include <unordered_map>

namespace co::http {

// =============================================================================
// Builder Pattern Classes
// =============================================================================

class request_builder {
public:
    request_builder() = default;
    
    request_builder& method(enum method m);
    request_builder& method(std::string_view m);
    request_builder& uri(std::string u);
    request_builder& version(enum version v);
    request_builder& header(std::string name, std::string value, bool sensitive = false);
    request_builder& body(std::string b);
    
    // Common methods
    request_builder& GET(std::string uri);
    request_builder& POST(std::string uri);
    request_builder& PUT(std::string uri);
    request_builder& DELETE(std::string uri);
    request_builder& HEAD(std::string uri);
    request_builder& OPTIONS(std::string uri);
    request_builder& PATCH(std::string uri);
    
    // Common headers
    request_builder& host(std::string h);
    request_builder& user_agent(std::string ua);
    request_builder& content_type(std::string ct);
    request_builder& authorization(std::string auth);
    request_builder& accept(std::string accept);
    request_builder& cookie(std::string cookie);
    request_builder& referer(std::string ref);
    request_builder& origin(std::string origin);
    
    // Content helpers
    request_builder& json_body(std::string json);
    request_builder& form_body(const std::unordered_map<std::string, std::string>& form_data);
    request_builder& text_body(std::string text);
    
    request build() const;
    operator request() const { return build(); }
    
private:
    request req_;
};

class response_builder {
public:
    response_builder();
    
    response_builder& status(unsigned int code);
    response_builder& status(unsigned int code, std::string reason);
    response_builder& version(enum version v);
    response_builder& header(std::string name, std::string value, bool sensitive = false);
    response_builder& body(std::string b);
    
    // Common status codes
    response_builder& ok();
    response_builder& created();
    response_builder& accepted();
    response_builder& no_content();
    response_builder& moved_permanently(std::string location);
    response_builder& found(std::string location);
    response_builder& not_modified();
    response_builder& bad_request();
    response_builder& unauthorized();
    response_builder& forbidden(); 
    response_builder& not_found();
    response_builder& method_not_allowed();
    response_builder& conflict();
    response_builder& internal_server_error();
    response_builder& not_implemented();
    response_builder& bad_gateway();
    response_builder& service_unavailable();
    
    // Common headers
    response_builder& content_type(std::string ct);
    response_builder& content_length(size_t length);
    response_builder& server(std::string s);
    response_builder& cache_control(std::string cc);
    response_builder& location(std::string loc);
    response_builder& set_cookie(std::string cookie);
    response_builder& cors_origin(std::string origin);
    
    // Content helpers
    response_builder& json_body(std::string json);
    response_builder& html_body(std::string html);
    response_builder& text_body(std::string text);
    
    response build() const;
    operator response() const { return build(); }
    
private:
    response resp_;
};

} // namespace co::http

// Include implementation
#include "detail/builder_impl.hpp"