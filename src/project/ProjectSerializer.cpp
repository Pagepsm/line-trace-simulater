#include "project/ProjectSerializer.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <atomic>
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace lt {
using Json = nlohmann::json;

static Json vec(Vec2 p) {
    return Json::array({p.x, p.y});
}

static Vec2 vectorValue(const Json& value) {
    if (!value.is_array() || value.size() != 2)
        throw std::runtime_error("Invalid position");
    Vec2 p{value.at(0).get<float>(), value.at(1).get<float>()};
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || std::abs(p.x) > 10000 ||
        std::abs(p.y) > 10000)
        throw std::runtime_error("Position out of range");
    return p;
}

static void writeText(const std::filesystem::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), text.size());
    out.close();
    if (!out)
        throw std::runtime_error("Cannot write Project file");
}

void ProjectSerializer::save(const std::filesystem::path& file, const ProjectModel& model,
                             const std::string& source) {
    auto c = model.vehicle;
    c.start = model.start;
    c.validate();
    if (source.size() > 1024 * 1024)
        throw std::runtime_error("Controller source exceeds 1 MiB");

    auto root = std::filesystem::absolute(file).parent_path();
    std::filesystem::create_directories(root);
    static std::atomic<unsigned> sequence = 0;
    std::string id = std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                     "_" + std::to_string(sequence++);
    std::string relative = "sources/" + id + "/user_logic.cpp";
    std::filesystem::create_directories((root / relative).parent_path());
    writeText(root / relative, source);

    Json json;
    json["format_version"] = 1;
    json["controller_source"] = relative;
    json["start"] = {{"position", vec(model.start.position)}, {"angle_rad", model.start.angle}};
    auto& v = json["vehicle"];
    v = {{"length", c.length},
         {"width", c.width},
         {"mass", c.mass},
         {"wheel_track", c.wheelTrack},
         {"wheel_length", c.wheelLength},
         {"wheel_width", c.wheelWidth},
         {"max_force", c.maxForce},
         {"forward_drag", c.forwardDrag},
         {"lateral_drag", c.lateralDrag},
         {"angular_drag", c.angularDrag},
         {"fixed_dt", c.fixedDt},
         {"substeps", c.substeps}};
    v["sensors"] = Json::array();
    for (auto s : c.sensors)
        v["sensors"].push_back(vec(s));

    json["strokes"] = Json::array();
    for (const auto& stroke : model.track.strokes) {
        Json points = Json::array();
        for (auto p : stroke.points)
            points.push_back({{"position", vec(p.position)}, {"width", p.width}});
        json["strokes"].push_back(points);
    }
    json["world_objects"] = Json::array();

    auto temporary = root / ("project_" + id + ".tmp");
    writeText(temporary, json.dump(2));
    // ソースを書き終えてからJSONを差し替え、保存途中の世代が混ざるのを防ぐ。
    if (!MoveFileExW(temporary.c_str(), std::filesystem::absolute(file).c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace Project manifest");
}

LoadedProject ProjectSerializer::load(const std::filesystem::path& file) {
    if (std::filesystem::file_size(file) > 32 * 1024 * 1024)
        throw std::runtime_error("Project JSON exceeds 32 MiB");
    std::ifstream input(file, std::ios::binary);
    Json j;
    input >> j;
    if (j.at("format_version").get<int>() != 1)
        throw std::runtime_error("Unsupported Project version");
    if (!j.at("world_objects").is_array() || !j.at("world_objects").empty())
        throw std::runtime_error("World objects are not supported in this MVP");

    LoadedProject result;
    auto& m = result.model;
    auto& c = m.vehicle;
    auto& v = j.at("vehicle");
    c.length = v.at("length");
    c.width = v.at("width");
    c.mass = v.at("mass");
    c.wheelTrack = v.at("wheel_track");
    c.wheelLength = v.at("wheel_length");
    c.wheelWidth = v.at("wheel_width");
    c.maxForce = v.at("max_force");
    c.forwardDrag = v.at("forward_drag");
    c.lateralDrag = v.at("lateral_drag");
    c.angularDrag = v.at("angular_drag");
    c.fixedDt = v.at("fixed_dt");
    c.substeps = v.at("substeps");

    c.sensors.clear();
    for (const auto& sensor : v.at("sensors"))
        c.sensors.push_back(vectorValue(sensor));
    m.start = {vectorValue(j.at("start").at("position")),
               j.at("start").at("angle_rad").get<float>()};
    c.start = m.start;
    c.validate();

    if (!j.at("strokes").is_array() || j.at("strokes").size() > 10000)
        throw std::runtime_error("Invalid stroke list");
    size_t count = 0;
    for (const auto& points : j.at("strokes")) {
        if (!points.is_array() || points.empty())
            throw std::runtime_error("Invalid stroke");
        Stroke stroke;
        for (const auto& point : points) {
            if (++count > 1000000)
                throw std::runtime_error("Project point limit exceeded");
            float width = point.at("width").get<float>();
            if (!std::isfinite(width) || width < .001f || width > .2f)
                throw std::runtime_error("Invalid stroke width");
            stroke.points.push_back({vectorValue(point.at("position")), width});
        }
        // Current editor guarantees constant width per stroke. Reject data that
        // would otherwise have a different variable-width render/query meaning.
        for (auto p : stroke.points)
            if (p.width != stroke.points.front().width)
                throw std::runtime_error("Variable width within a stroke is not supported yet");
        m.track.strokes.push_back(std::move(stroke));
    }

    auto relative = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(
        j.at("controller_source").get_ref<const std::string&>().c_str())));
    if (relative.empty() || relative.is_absolute() || relative.has_root_path())
        throw std::runtime_error("Controller source must be relative to Project");
    for (const auto& part : relative)
        if (part == "..")
            throw std::runtime_error("Controller source cannot escape Project");
    auto root = std::filesystem::weakly_canonical(std::filesystem::absolute(file).parent_path());
    auto source = std::filesystem::weakly_canonical(root / relative);
    auto local = source.lexically_relative(root);
    for (const auto& part : local)
        if (part == "..")
            throw std::runtime_error("Controller source symlink escapes Project");
    if (std::filesystem::file_size(source) > 1024 * 1024)
        throw std::runtime_error("Controller source exceeds 1 MiB");

    std::ifstream cpp(source, std::ios::binary);
    if (!cpp)
        throw std::runtime_error("Cannot read Project Controller source");
    result.source.assign(std::istreambuf_iterator<char>(cpp), {});
    return result;
}
}
