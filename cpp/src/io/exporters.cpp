#include "exporters.h"
#include <glad/gl.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <cstdio>
#include <cmath>
#include <vector>

namespace ocp {

bool OBJExporter::export_to_file(const std::string& path, Scene& scene) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "# OCP OBJ Export - KitariosWebStudio - KWS\n");
    fprintf(f, "# Scene: %s\n\n", scene.name.c_str());
    uint32_t offset = 0;
    for (auto* node : scene.get_all_nodes()) {
        if (!node->mesh || !node->visible) continue;
        fprintf(f, "o %s\n", node->name.c_str());
        mat4 wm = node->get_world_matrix();
        for (auto& v : node->mesh->vertices) {
            vec3 wp = vec3(wm * vec4(v.position, 1.0f));
            fprintf(f, "v %f %f %f\n", wp.x, wp.y, wp.z);
        }
        for (auto& v : node->mesh->vertices) {
            vec3 wn = glm::normalize(vec3(wm * vec4(v.normal, 0.0f)));
            fprintf(f, "vn %f %f %f\n", wn.x, wn.y, wn.z);
        }
        for (auto& v : node->mesh->vertices) fprintf(f, "vt %f %f\n", v.uv.x, v.uv.y);
        for (auto& face : node->mesh->faces) {
            fprintf(f, "f");
            for (auto& vi : face.vertices) fprintf(f, " %d/%d/%d", vi + 1 + offset, vi + 1 + offset, vi + 1 + offset);
            fprintf(f, "\n");
        }
        offset += (uint32_t)node->mesh->vertices.size();
        fprintf(f, "\n");
    }
    fclose(f);
    return true;
}

bool STLExporter::export_to_file(const std::string& path, Scene& scene) {
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "solid OCP\n");
    for (auto* node : scene.get_all_nodes()) {
        if (!node->mesh || !node->visible) continue;
        mat4 wm = node->get_world_matrix();
        for (auto& face : node->mesh->faces) {
            if (face.vertices.size() < 3) continue;
            vec3 v0 = vec3(wm * vec4(node->mesh->vertices[face.vertices[0]].position, 1.0f));
            for (size_t i = 1; i + 1 < face.vertices.size(); i++) {
                vec3 v1 = vec3(wm * vec4(node->mesh->vertices[face.vertices[i]].position, 1.0f));
                vec3 v2 = vec3(wm * vec4(node->mesh->vertices[face.vertices[i + 1]].position, 1.0f));
                vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
                fprintf(f, "  facet normal %f %f %f\n", n.x, n.y, n.z);
                fprintf(f, "    outer loop\n");
                fprintf(f, "      vertex %f %f %f\n", v0.x, v0.y, v0.z);
                fprintf(f, "      vertex %f %f %f\n", v1.x, v1.y, v1.z);
                fprintf(f, "      vertex %f %f %f\n", v2.x, v2.y, v2.z);
                fprintf(f, "    endloop\n");
                fprintf(f, "  endfacet\n");
            }
        }
    }
    fprintf(f, "endsolid OCP\n");
    fclose(f);
    return true;
}

bool PNGExporter::export_framebuffer(const std::string& path, int width, int height) {
    std::vector<unsigned char> pixels(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::vector<unsigned char> flipped(pixels.size());
    for (int y = 0; y < height; y++) {
        memcpy(&flipped[y * width * 4], &pixels[(height - 1 - y) * width * 4], width * 4);
    }
    return stbi_write_png(path.c_str(), width, height, 4, flipped.data(), width * 4) != 0;
}

} // namespace ocp
