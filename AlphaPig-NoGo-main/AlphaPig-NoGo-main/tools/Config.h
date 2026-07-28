#pragma once
// ============================================================
// Config — lightweight key=value config reader.
// Format:
//   # comment
//   key = value
//   tree_reuse = true
//   search_time = 950
//   num_games = 100
// ============================================================

#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <algorithm>

class Config
{
public:
    Config() = default;

    bool load(const std::string& path)
    {
        m_path = path;
        std::ifstream f(path);
        if (!f.is_open()) return false;

        std::string line;
        int ln = 0;
        while (std::getline(f, line)) {
            ln++;
            // Trim
            line.erase(0, line.find_first_not_of(" \t\r"));
            line.erase(line.find_last_not_of(" \t\r") + 1);
            // Skip comments and blanks
            if (line.empty() || line[0] == '#') continue;

            auto eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            // Trim key and val
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            val.erase(0, val.find_first_not_of(" \t"));
            val.erase(val.find_last_not_of(" \t") + 1);

            m_map[key] = val;
        }
        return true;
    }

    std::string getStr(const std::string& key, const std::string& def = "") const
    {
        auto it = m_map.find(key);
        return (it != m_map.end()) ? it->second : def;
    }

    bool getBool(const std::string& key, bool def = false) const
    {
        auto it = m_map.find(key);
        if (it == m_map.end()) return def;
        std::string v = it->second;
        std::transform(v.begin(), v.end(), v.begin(), ::tolower);
        return (v == "true" || v == "1" || v == "yes" || v == "on");
    }

    int getInt(const std::string& key, int def = 0) const
    {
        auto it = m_map.find(key);
        if (it == m_map.end()) return def;
        return std::stoi(it->second);
    }

    double getDouble(const std::string& key, double def = 0.0) const
    {
        auto it = m_map.find(key);
        if (it == m_map.end()) return def;
        return std::stod(it->second);
    }

    const std::string& path() const { return m_path; }

private:
    std::string m_path;
    std::unordered_map<std::string, std::string> m_map;
};
