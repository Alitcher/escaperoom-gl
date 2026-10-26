#include "Level.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;   // keeps keys in the order we write them

// ---------- Reading ----------

// Reads [x, y, z] under `key`, or returns `fallback` if the key is missing.
static glm::vec3 readVec3(const json& obj, const char* key, glm::vec3 fallback) {
    if (!obj.contains(key))
        return fallback;
    const json& a = obj.at(key);
    if (!a.is_array() || a.size() != 3)
        throw std::runtime_error(std::string("\"") + key + "\" must be [x, y, z]");
    return {a[0].get<float>(), a[1].get<float>(), a[2].get<float>()};
}

static Light readLight(const json& jl) {
    Light light;
    const std::string type = jl.value("type", "point");
    if (type == "point")
        light.type = Light::Type::Point;
    else if (type == "directional")
        light.type = Light::Type::Directional;
    else
        throw std::runtime_error("unknown light type \"" + type + "\"");

    light.position = readVec3(jl, "position", light.position);
    light.direction = readVec3(jl, "direction", light.direction);
    light.color = readVec3(jl, "color", light.color);
    light.intensity = jl.value("intensity", light.intensity);
    return light;
}

static Entity readEntity(const json& je) {
    Entity e;
    e.id = je.at("id").get<std::string>();
    e.model = je.value("model", "");
    e.parent = je.value("parent", "");

    if (je.contains("transform")) {
        const json& jt = je.at("transform");
        e.transform.position = readVec3(jt, "position", e.transform.position);
        e.transform.rotationDeg = readVec3(jt, "rotation", e.transform.rotationDeg);
        e.transform.scale = readVec3(jt, "scale", e.transform.scale);
    }

    if (je.contains("interaction")) {
        const json& ji = je.at("interaction");
        Interaction& i = e.interaction;
        i.type = ji.at("type").get<std::string>();
        i.item = ji.value("item", "");
        i.locked = ji.value("locked", false);
        i.unlockedBy = ji.value("unlockedBy", "");
        i.contains = ji.value("contains", "");
        i.text = ji.value("text", "");
        i.code = ji.value("code", "");
        i.codeFrom = ji.value("codeFrom", "");
        i.opens = ji.value("opens", "");
        i.name = ji.value("name", e.id);
        // An NPC can have one "line" or a list of "lines".
        if (ji.contains("lines"))
            i.lines = ji.at("lines").get<std::vector<std::string>>();
        else if (ji.contains("line"))
            i.lines.push_back(ji.at("line").get<std::string>());
        i.gives = ji.value("gives", "");
        i.count = ji.value("count", i.count);
        i.sides = ji.value("sides", i.sides);
        if (i.type == "dice" && (i.count < 1 || i.count > 10 || i.sides < 2))
            throw std::runtime_error("dice \"" + e.id + "\" needs count 1..10 and sides >= 2");
    }
    return e;
}

static ItemDef readItem(const json& jd) {
    ItemDef item;
    item.id = jd.at("id").get<std::string>();
    item.name = jd.value("name", item.id);
    item.icon = jd.value("icon", "");
    return item;
}

int Level::indexOf(const std::string& id) const {
    if (id.empty())
        return -1;
    for (size_t i = 0; i < entities.size(); ++i)
        if (entities[i].id == id)
            return (int)i;
    return -1;
}

const ItemDef* Level::findItem(const std::string& id) const {
    for (const ItemDef& item : items)
        if (item.id == id)
            return &item;
    return nullptr;
}

std::string Level::itemName(const std::string& id) const {
    const ItemDef* item = findItem(id);
    return item ? item->name : id;
}

std::string folderOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

bool parseLevel(const std::string& text, const std::string& baseDir, Level& out, std::string& error) {
    try {
        const json j = json::parse(text);
        Level level;
        level.version = j.value("version", 1);
        level.name = j.value("name", "Untitled");
        level.author = j.value("author", "");

        if (j.contains("spawn")) {
            const json& js = j.at("spawn");
            level.spawnPosition = readVec3(js, "position", level.spawnPosition);
            level.spawnYawDeg = js.value("yaw", level.spawnYawDeg);
        }

        for (const json& jl : j.value("lights", json::array()))
            level.lights.push_back(readLight(jl));

        for (const json& jd : j.value("items", json::array()))
            level.items.push_back(readItem(jd));

        std::set<std::string> ids;
        for (const json& je : j.value("entities", json::array())) {
            Entity e = readEntity(je);
            if (!ids.insert(e.id).second)
                throw std::runtime_error("duplicate entity id \"" + e.id + "\"");
            level.entities.push_back(std::move(e));
        }

        if (j.contains("win")) {
            level.winExit = j.at("win").value("exit", "");
            level.winMessage = j.at("win").value("message", "");
        }
        level.baseDir = baseDir;

        out = std::move(level);
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}

bool loadLevel(const std::string& path, Level& out, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    std::stringstream text;
    text << file.rdbuf();
    if (!parseLevel(text.str(), folderOf(path), out, error)) {
        error = path + ": " + error;
        return false;
    }
    return true;
}

// ---------- Writing ----------

// 4 decimals is plenty for a room, and keeps 1.6 from turning into 1.600000023841858.
// Whole numbers are written as 180, not 180.0.
static ordered_json tidy(float v) {
    const double r = std::round((double)v * 10000.0) / 10000.0;
    if (r == std::floor(r) && std::abs(r) < 1e9)
        return (long long)r;   // also turns -0 into 0
    return r;
}

static ordered_json vec3Json(const glm::vec3& v) {
    return ordered_json::array({tidy(v.x), tidy(v.y), tidy(v.z)});
}

static ordered_json interactionJson(const Entity& e) {
    const Interaction& i = e.interaction;
    ordered_json ji;
    ji["type"] = i.type;
    if (i.type == "pickup") {
        ji["item"] = i.item;
    } else if (i.type == "container") {
        ji["locked"] = i.locked;
        if (!i.unlockedBy.empty()) ji["unlockedBy"] = i.unlockedBy;
        if (!i.contains.empty()) ji["contains"] = i.contains;
    } else if (i.type == "readable") {
        ji["text"] = i.text;
    } else if (i.type == "keypad") {
        if (!i.codeFrom.empty())
            ji["codeFrom"] = i.codeFrom;
        else
            ji["code"] = i.code;
        ji["opens"] = i.opens;
    } else if (i.type == "exit") {
        if (!i.unlockedBy.empty()) ji["unlockedBy"] = i.unlockedBy;
    } else if (i.type == "npc") {
        ji["name"] = i.name;
        ji["lines"] = i.lines;
        if (!i.gives.empty()) ji["gives"] = i.gives;
    } else if (i.type == "dice") {
        ji["count"] = i.count;
        ji["sides"] = i.sides;
    }
    return ji;
}

// json's pretty printer puts every array number on its own line. Put short
// number arrays like [0, 1.6, 2.2] back on one line so files stay readable.
static std::string compactNumberArrays(const std::string& text) {
    static const std::regex numberArray(R"(\[\s*(-?[0-9.eE+-]+(\s*,\s*-?[0-9.eE+-]+)*)\s*\])");
    static const std::regex spaces(R"(\s*,\s*)");
    std::string out;
    auto begin = std::sregex_iterator(text.begin(), text.end(), numberArray);
    size_t last = 0;
    for (auto it = begin; it != std::sregex_iterator(); ++it) {
        out += text.substr(last, it->position() - last);
        out += "[" + std::regex_replace((*it)[1].str(), spaces, ", ") + "]";
        last = it->position() + it->length();
    }
    out += text.substr(last);
    return out;
}

static ordered_json levelJson(const Level& level) {
    ordered_json j;
    j["version"] = level.version;
    j["name"] = level.name;
    j["author"] = level.author;
    j["spawn"] = {{"position", vec3Json(level.spawnPosition)}, {"yaw", tidy(level.spawnYawDeg)}};

    j["lights"] = ordered_json::array();
    for (const Light& l : level.lights) {
        ordered_json jl;
        if (l.type == Light::Type::Directional) {
            jl["type"] = "directional";
            jl["direction"] = vec3Json(l.direction);
        } else {
            jl["type"] = "point";
            jl["position"] = vec3Json(l.position);
        }
        jl["color"] = vec3Json(l.color);
        jl["intensity"] = tidy(l.intensity);
        j["lights"].push_back(jl);
    }

    if (!level.items.empty()) {
        j["items"] = ordered_json::array();
        for (const ItemDef& item : level.items) {
            ordered_json jd;
            jd["id"] = item.id;
            jd["name"] = item.name;
            if (!item.icon.empty()) jd["icon"] = item.icon;
            j["items"].push_back(jd);
        }
    }

    j["entities"] = ordered_json::array();
    for (const Entity& e : level.entities) {
        ordered_json je;
        je["id"] = e.id;
        if (!e.model.empty()) je["model"] = e.model;
        if (!e.parent.empty()) je["parent"] = e.parent;
        ordered_json jt;
        jt["position"] = vec3Json(e.transform.position);
        if (e.transform.rotationDeg != glm::vec3(0.0f)) jt["rotation"] = vec3Json(e.transform.rotationDeg);
        if (e.transform.scale != glm::vec3(1.0f)) jt["scale"] = vec3Json(e.transform.scale);
        je["transform"] = jt;
        if (!e.interaction.type.empty())
            je["interaction"] = interactionJson(e);
        j["entities"].push_back(je);
    }

    ordered_json win;
    win["exit"] = level.winExit;
    if (!level.winMessage.empty()) win["message"] = level.winMessage;
    j["win"] = win;
    return j;
}

std::string levelToJson(const Level& level) {
    return compactNumberArrays(levelJson(level).dump(2)) + "\n";
}

std::string levelSnapshot(const Level& level) {
    return levelJson(level).dump();
}

bool saveLevel(const std::string& path, const Level& level, std::string& error) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot write " + path;
        return false;
    }
    file << levelToJson(level);
    if (!file) {
        error = "write failed: " + path;
        return false;
    }
    return true;
}
