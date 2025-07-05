#include "http_parse.hpp"
#include <iostream>

using namespace co::http;

int main() {
    request req;
    req.method_type = method::get;
    req.target = "/api/users";
    req.protocol_version = version::http_1_1;
    req.add_header("host", "api.example.com");
    
    std::cout << "Request target: '" << req.target << "' (length: " << req.target.length() << ")" << std::endl;
    std::cout << "Request uri: '" << req.uri << "' (length: " << req.uri.length() << ")" << std::endl;
    std::cout << "Target empty: " << (req.target.empty() ? "true" : "false") << std::endl;
    
    // Try to directly call the encoder function
    #include "http_parse/v1/encoder_impl.hpp"
    auto encoded = co::http::v1::detail::encode_http1_request(req, version::http_1_1);
    std::cout << "Direct encoded:\n" << encoded << std::endl;
    
    // Also try the high-level function
    auto result = http1::encode_request(req);
    if (result.has_value()) {
        std::cout << "High-level encoded:\n" << result.value() << std::endl;
    } else {
        std::cout << "High-level encoding failed" << std::endl;
    }
    
    return 0;
}