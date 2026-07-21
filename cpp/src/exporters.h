#pragma once
#include "scene.h"
#include <string>

namespace ocp {

class OBJExporter {
public:
    static bool export_to_file(const std::string& path, Scene& scene);
};

class STLExporter {
public:
    static bool export_to_file(const std::string& path, Scene& scene);
};

class PNGExporter {
public:
    static bool export_framebuffer(const std::string& path, int width, int height);
};

} // namespace ocp
