#include "core/camera_model.hpp"
#include "core/register_map.hpp"
#include "protocol/genicam_xml_generator.hpp"

#include <fstream>
#include <iostream>

int main(int argc, char* argv[]) {
    camera::core::CameraConfig config;
    const auto xml = camera::protocol::GenicamXmlGenerator::generate(camera::core::RegisterMap(config));
    if (argc > 1) {
        std::ofstream output(argv[1], std::ios::binary);
        if (!output) return 2;
        output << xml;
        return output ? 0 : 3;
    }
    std::cout << xml;
    return 0;
}
