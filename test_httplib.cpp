#include "third_party/cpp_httplib/httplib.h"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    httplib::Server svr;

    svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("Hello from httplib!", "text/plain");
    });

    std::cout << "Starting server on http://0.0.0.0:9090" << std::endl;

    std::thread server_thread([&svr]() {
        if (!svr.listen("0.0.0.0", 9090)) {
            std::cerr << "Failed to start server!" << std::endl;
        }
    });

    std::cout << "Server started. Press Enter to stop..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "Test with: curl http://localhost:9090" << std::endl;

    std::string wait;
    std::getline(std::cin, wait);

    svr.stop();
    if (server_thread.joinable()) {
        server_thread.join();
    }

    return 0;
}
