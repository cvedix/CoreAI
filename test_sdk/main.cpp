#include <iostream>
#include <cvedix/cvedix_version.h>
#include <cvedix/utils/cvedix_utils.h>

int main() {
    std::cout << "CVEDIX SDK Version: " << CVEDIX_VERSION << std::endl;
    std::cout << "Build Time: " << CVEDIX_BUILD_TIME << std::endl;
    return 0;
}
