#include "config.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace tomocam::config {

    namespace {

        std::string trim(const std::string &s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            size_t end = s.find_last_not_of(" \t\r\n");
            return s.substr(start, end - start + 1);
        }

    } // namespace

    std::map<std::string, TomlValue> parseTomlLite(const std::string &text) {
        std::map<std::string, TomlValue> result;
        std::istringstream stream(text);
        std::string line;
        int lineNo = 0;

        while (std::getline(stream, line)) {
            lineNo++;
            std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') continue;

            size_t eq = trimmed.find('=');
            if (eq == std::string::npos) {
                std::cerr << "Warning: config line " << lineNo
                          << ": missing '=', skipping" << std::endl;
                continue;
            }

            std::string key = trim(trimmed.substr(0, eq));
            std::string value = trim(trimmed.substr(eq + 1));
            if (key.empty()) {
                std::cerr << "Warning: config line " << lineNo
                          << ": empty key, skipping" << std::endl;
                continue;
            }

            if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
                result[key] = value.substr(1, value.size() - 2);
            } else if (value == "true") {
                result[key] = true;
            } else if (value == "false") {
                result[key] = false;
            } else {
                try {
                    size_t pos = 0;
                    int64_t n = std::stoll(value, &pos);
                    if (pos != value.size())
                        throw std::invalid_argument("trailing characters");
                    result[key] = n;
                } catch (const std::exception &) {
                    std::cerr << "Warning: config line " << lineNo
                              << ": cannot parse value '" << value << "', skipping"
                              << std::endl;
                }
            }
        }
        return result;
    }

    std::map<std::string, TomlValue>
    loadTomlLiteFile(const std::filesystem::path &path) {
        if (path.empty() || !std::filesystem::exists(path)) return {};

        std::ifstream in(path);
        if (!in) {
            std::cerr << "Warning: could not open config file " << path << std::endl;
            return {};
        }

        std::ostringstream buf;
        buf << in.rdbuf();
        return parseTomlLite(buf.str());
    }

    std::filesystem::path configFilePath() {
        const char *xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg && *xdg)
            return std::filesystem::path(xdg) / "tiffview" / "config.toml";

        const char *home = std::getenv("HOME");
        if (home && *home)
            return std::filesystem::path(home) / ".config" / "tiffview" /
                   "config.toml";

        return {};
    }

    size_t
    resolveCacheBudgetBytes(std::optional<int64_t> cliCacheMb,
                            const std::map<std::string, TomlValue> &configValues) {
        auto toBytes = [](int64_t mb) {
            return static_cast<size_t>(mb) * 1024 * 1024;
        };

        if (cliCacheMb.has_value()) {
            if (*cliCacheMb > 0) return toBytes(*cliCacheMb);
            std::cerr << "Warning: --cache-mb value must be positive; ignoring '"
                      << *cliCacheMb << "'" << std::endl;
        }

        auto it = configValues.find("cache_mb");
        if (it != configValues.end()) {
            if (const int64_t *mb = std::get_if<int64_t>(&it->second)) {
                if (*mb > 0) return toBytes(*mb);
                std::cerr << "Warning: cache_mb in config file must be positive; "
                             "ignoring '"
                          << *mb << "'" << std::endl;
            } else {
                std::cerr << "Warning: cache_mb in config file is not an integer; "
                             "ignoring"
                          << std::endl;
            }
        }

        return toBytes(kDefaultCacheMb);
    }

    size_t resolveCacheBudgetBytesFromDisk(std::optional<int64_t> cliCacheMb) {
        auto configValues = loadTomlLiteFile(configFilePath());
        return resolveCacheBudgetBytes(cliCacheMb, configValues);
    }

    void ensureDefaultConfigFileAt(const std::filesystem::path &path) {
        if (path.empty()) return;

        std::error_code ec;
        if (std::filesystem::exists(path, ec)) return;

        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            std::cerr << "Warning: could not create config directory "
                      << path.parent_path() << ": " << ec.message() << std::endl;
            return;
        }

        std::ofstream out(path);
        if (!out) {
            std::cerr << "Warning: could not create config file " << path
                      << std::endl;
            return;
        }

        out << "# tiffview configuration\n"
            << "#\n"
            << "# cache_mb: memory budget (in MB) for the Z-axis slice cache used\n"
            << "# on datasets too large to fully load. Overridden by --cache-mb on\n"
            << "# the command line.\n"
            << "cache_mb = " << kDefaultCacheMb << "\n";

        if (!out) {
            std::cerr << "Warning: failed writing config file " << path << std::endl;
            return;
        }

        std::cout << "Created default config file at " << path << std::endl;
    }

    void ensureDefaultConfigFile() { ensureDefaultConfigFileAt(configFilePath()); }

} // namespace tomocam::config
