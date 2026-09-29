#ifndef TOMOCAM_CONFIG__H
#define TOMOCAM_CONFIG__H

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <variant>

namespace tomocam::config {

    using TomlValue = std::variant<std::string, int64_t, bool>;

    // Parses a flat subset of TOML: blank lines; whole-line '#' comments
    // (no inline trailing comments); "key = value" lines where value is a
    // double-quoted string, a bare integer, or true/false. No
    // sections/tables/arrays -- sufficient for scalar app settings, with
    // room to add more keys later without changing the format. Malformed
    // lines are skipped with a stderr warning; never throws.
    std::map<std::string, TomlValue> parseTomlLite(const std::string &text);

    // Reads and parses the file at `path`. Returns an empty map, with no
    // warning, if the file does not exist -- this is the expected common
    // case and must not be treated as an error. Other I/O errors print a
    // warning and also return an empty map, so a broken config never
    // prevents startup.
    std::map<std::string, TomlValue>
    loadTomlLiteFile(const std::filesystem::path &path);

    // ~/.config/tiffview/config.toml, honoring $XDG_CONFIG_HOME if set and
    // non-empty (falling back to $HOME/.config per XDG basedir
    // convention). Returns an empty path if neither is set.
    std::filesystem::path configFilePath();

    constexpr int64_t kDefaultCacheMb = 512;

    // Creates `path` (and its parent directory) with commented-out
    // defaults if it doesn't already exist. A no-op if `path` already
    // exists, is empty, or can't be created (e.g. permission denied,
    // which is only a warning -- the app runs fine on hard-coded
    // defaults regardless). Never throws.
    void ensureDefaultConfigFileAt(const std::filesystem::path &path);

    // Convenience wrapper used by main(): creates configFilePath() if
    // missing, before it's loaded for this run.
    void ensureDefaultConfigFile();

    // Pure precedence resolution (unit-testable without touching disk):
    // CLI override > config file's "cache_mb" key > kDefaultCacheMb. A
    // non-positive or missing cache_mb in the config is ignored with a
    // warning, falling through to the default.
    size_t
    resolveCacheBudgetBytes(std::optional<int64_t> cliCacheMb,
                            const std::map<std::string, TomlValue> &configValues);

    // Convenience wrapper used by main(): loads configFilePath() from disk
    // and delegates to the pure overload above.
    size_t resolveCacheBudgetBytesFromDisk(std::optional<int64_t> cliCacheMb);

} // namespace tomocam::config
#endif // TOMOCAM_CONFIG__H
