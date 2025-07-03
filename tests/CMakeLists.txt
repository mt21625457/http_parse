# Tests CMakeLists.txt

# Only build tests if this is the main project or explicitly requested
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME OR HTTP_PARSE_BUILD_TESTS)
    
    # Find or fetch a testing framework (using a simple approach here)
    # You can replace this with Google Test, Catch2, or any other framework
    
    function(add_http_parse_test name source)
        add_executable(test_${name} ${source})
        target_link_libraries(test_${name} PRIVATE co::http_parse)
        
        # Set output directory
        set_target_properties(test_${name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        )
        
        # Add to tests folder in IDEs
        set_target_properties(test_${name} PROPERTIES FOLDER "Tests")
        
        # Register with CTest
        add_test(NAME ${name} COMMAND test_${name})
        
        message(STATUS "Added test: test_${name}")
    endfunction()
    
    # Simple test runner without external dependencies
    add_http_parse_test(basic_parsing "
#include <http_parse/http_parse.hpp>
#include <iostream>
#include <cassert>

int main() {
    using namespace co::http;
    
    // Test basic HTTP/1.1 parsing
    std::string_view request_data = 
        \"GET /test HTTP/1.1\\r\\n\"
        \"Host: example.com\\r\\n\"
        \"\\r\\n\";
    
    auto result = parse_request(request_data, version::http_1_1);
    assert(result.has_value());
    
    const auto& req = *result;
    assert(req.method == \"GET\");
    assert(req.uri == \"/test\");
    
    auto host = req.get_header(\"host\");
    assert(host.has_value());
    assert(*host == \"example.com\");
    
    std::cout << \"✓ Basic parsing test passed\\n\";
    return 0;
}
")
    
    # Create a simple test file if it doesn't exist
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test_basic_parsing.cpp")
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/test_basic_parsing.cpp" "
#include <http_parse/http_parse.hpp>
#include <iostream>
#include <cassert>
#include <string_view>

int main() {
    using namespace co::http;
    
    std::cout << \"Running basic HTTP parsing tests...\\n\";
    
    // Test 1: Basic HTTP/1.1 GET request
    {
        std::string_view request_data = 
            \"GET /test HTTP/1.1\\r\\n\"
            \"Host: example.com\\r\\n\"
            \"User-Agent: TestClient/1.0\\r\\n\"
            \"\\r\\n\";
        
        auto result = parse_request(request_data, version::http_1_1);
        assert(result.has_value() && \"Failed to parse basic GET request\");
        
        const auto& req = *result;
        assert(req.method == \"GET\" && \"Incorrect method parsed\");
        assert(req.uri == \"/test\" && \"Incorrect URI parsed\");
        
        auto host = req.get_header(\"host\");
        assert(host.has_value() && \"Host header not found\");
        assert(*host == \"example.com\" && \"Incorrect host value\");
        
        std::cout << \"  ✓ Basic GET request parsing\\n\";
    }
    
    // Test 2: POST request with body
    {
        std::string_view request_data = 
            \"POST /api/data HTTP/1.1\\r\\n\"
            \"Host: api.example.com\\r\\n\"
            \"Content-Type: application/json\\r\\n\"
            \"Content-Length: 13\\r\\n\"
            \"\\r\\n\"
            \"{\\\"test\\\":true}\";
        
        auto result = parse_request(request_data, version::http_1_1);
        assert(result.has_value() && \"Failed to parse POST request\");
        
        const auto& req = *result;
        assert(req.method == \"POST\" && \"Incorrect method for POST request\");
        assert(req.uri == \"/api/data\" && \"Incorrect URI for POST request\");
        assert(req.body == \"{\\\"test\\\":true}\" && \"Incorrect body content\");
        
        std::cout << \"  ✓ POST request with body parsing\\n\";
    }
    
    // Test 3: HTTP response parsing
    {
        std::string_view response_data = 
            \"HTTP/1.1 200 OK\\r\\n\"
            \"Content-Type: text/html\\r\\n\"
            \"Content-Length: 13\\r\\n\"
            \"\\r\\n\"
            \"Hello, World!\";
        
        auto result = parse_response(response_data, version::http_1_1);
        assert(result.has_value() && \"Failed to parse HTTP response\");
        
        const auto& resp = *result;
        assert(resp.status_code == 200 && \"Incorrect status code\");
        assert(resp.reason_phrase == \"OK\" && \"Incorrect reason phrase\");
        assert(resp.body == \"Hello, World!\" && \"Incorrect response body\");
        
        std::cout << \"  ✓ HTTP response parsing\\n\";
    }
    
    // Test 4: Error handling
    {
        std::string_view invalid_data = \"INVALID HTTP REQUEST\";
        auto result = parse_request(invalid_data, version::http_1_1);
        assert(!result.has_value() && \"Should have failed to parse invalid data\");
        
        std::cout << \"  ✓ Error handling for invalid data\\n\";
    }
    
    std::cout << \"\\n✓ All basic parsing tests passed!\\n\";
    return 0;
}
")
    endif()
    
    add_executable(test_basic_parsing "${CMAKE_CURRENT_BINARY_DIR}/test_basic_parsing.cpp")
    target_link_libraries(test_basic_parsing PRIVATE co::http_parse)
    set_target_properties(test_basic_parsing PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        FOLDER "Tests"
    )
    add_test(NAME basic_parsing COMMAND test_basic_parsing)
    
    # Custom target to run all tests
    add_custom_target(run_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS test_basic_parsing
        COMMENT "Running all HTTP parse tests"
    )
    
endif()