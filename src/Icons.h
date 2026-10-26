#pragma once
#include <string>
#include <unordered_map>

// Loads item icon images (PNG/JPG) into OpenGL textures, once per file.
// Needs a live GL context; destroy it before the context goes away.
class IconCache {
public:
    IconCache() = default;
    ~IconCache();
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    // GL texture for the image at `path`, or 0 if it can't be loaded.
    // Failures are remembered, so a missing file is only reported once.
    unsigned int get(const std::string& path);

private:
    std::unordered_map<std::string, unsigned int> m_textures;
};
