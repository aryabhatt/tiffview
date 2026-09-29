#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <variant>

#include "colormap_lut.h"
#include "config.h"
#include "io/tiff/normalize.h"
#include "slice_cache_policy.h"

using tomocam::config::TomlValue;

namespace {

    int g_failures = 0;

    void check(bool cond, const std::string &what) {
        if (!cond) {
            std::cerr << "FAILED: " << what << std::endl;
            g_failures++;
        }
    }

    void testNormalizeSliceToU8() {
        uint16_t src[] = {100, 100, 300, 500};
        uint8_t dst[4] = {};
        tomocam::tiff::normalizeSliceToU8<uint16_t>(src, dst, 4);
        check(dst[0] == 0, "normalizeSliceToU8: min maps to 0");
        check(dst[3] == 255, "normalizeSliceToU8: max maps to 255");

        uint16_t constSrc[] = {7, 7, 7};
        uint8_t constDst[3] = {9, 9, 9};
        tomocam::tiff::normalizeSliceToU8<uint16_t>(constSrc, constDst, 3);
        check(constDst[0] == 0 && constDst[1] == 0 && constDst[2] == 0,
              "normalizeSliceToU8: constant slice maps to all zeros");
    }

    void testTomlLiteParsesScalars() {
        std::string text = "# a comment\n"
                           "\n"
                           "cache_mb = 256\n"
                           "name = \"tiffview\"\n"
                           "verbose = true\n";
        auto values = tomocam::config::parseTomlLite(text);

        check(values.count("cache_mb") == 1, "toml: cache_mb parsed");
        if (auto *v = std::get_if<int64_t>(&values["cache_mb"]))
            check(*v == 256, "toml: cache_mb == 256");
        else
            check(false, "toml: cache_mb is an integer");

        check(values.count("name") == 1, "toml: name parsed");
        if (auto *v = std::get_if<std::string>(&values["name"]))
            check(*v == "tiffview", "toml: name == tiffview");
        else
            check(false, "toml: name is a string");

        check(values.count("verbose") == 1, "toml: verbose parsed");
        if (auto *v = std::get_if<bool>(&values["verbose"]))
            check(*v == true, "toml: verbose == true");
        else
            check(false, "toml: verbose is a bool");
    }

    void testTomlLiteSkipsMalformedLines() {
        std::string text = "no_equals_sign_here\n"
                           "bad_int = not_a_number\n"
                           "good = 42\n";
        auto values = tomocam::config::parseTomlLite(text);
        check(values.count("no_equals_sign_here") == 0,
              "toml: line without '=' dropped");
        check(values.count("bad_int") == 0, "toml: unparseable value dropped");
        check(values.count("good") == 1,
              "toml: valid line after malformed ones still parsed");
    }

    void testLoadTomlLiteFileMissingReturnsEmpty() {
        auto values = tomocam::config::loadTomlLiteFile(
            "/nonexistent/path/tiffview-test/config.toml");
        check(values.empty(),
              "toml: missing config file returns empty map, no throw");
    }

    void testEnsureDefaultConfigFileCreatesAndDoesNotOverwrite() {
        std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                    "tiffview-test-config-XXXXXX-ensure-default";
        std::filesystem::remove_all(dir);
        std::filesystem::path path = dir / "config.toml";

        tomocam::config::ensureDefaultConfigFileAt(path);
        check(std::filesystem::exists(path),
              "ensureDefaultConfigFile: creates missing file and parent dir");

        auto values = tomocam::config::loadTomlLiteFile(path);
        if (auto *v = std::get_if<int64_t>(&values["cache_mb"]))
            check(*v == tomocam::config::kDefaultCacheMb,
                  "ensureDefaultConfigFile: written cache_mb matches the default");
        else
            check(false, "ensureDefaultConfigFile: cache_mb is an integer");

        // Overwrite with a sentinel value, then confirm a second call leaves
        // an already-existing file untouched.
        {
            std::ofstream out(path);
            out << "cache_mb = 999\n";
        }
        tomocam::config::ensureDefaultConfigFileAt(path);
        auto reloaded = tomocam::config::loadTomlLiteFile(path);
        if (auto *v = std::get_if<int64_t>(&reloaded["cache_mb"]))
            check(*v == 999,
                  "ensureDefaultConfigFile: existing file is never overwritten");
        else
            check(false, "ensureDefaultConfigFile: cache_mb is an integer");

        std::filesystem::remove_all(dir);
    }

    void testResolveCacheBudgetPrecedence() {
        std::map<std::string, TomlValue> withConfig{{"cache_mb", int64_t{128}}};
        std::map<std::string, TomlValue> empty;

        size_t cliWins = tomocam::config::resolveCacheBudgetBytes(
            std::optional<int64_t>{64}, withConfig);
        check(cliWins == size_t(64) * 1024 * 1024,
              "config precedence: CLI overrides config");

        size_t configWins =
            tomocam::config::resolveCacheBudgetBytes(std::nullopt, withConfig);
        check(configWins == size_t(128) * 1024 * 1024,
              "config precedence: config used when CLI unset");

        size_t defaultWins =
            tomocam::config::resolveCacheBudgetBytes(std::nullopt, empty);
        check(defaultWins == size_t(tomocam::config::kDefaultCacheMb) * 1024 * 1024,
              "config precedence: default used when both unset");
    }

    void testSliceCachePolicyNeverEvictsCurrent() {
        SliceCachePolicy policy(/*pageCount=*/100, /*bytesPerSlice=*/1,
                                /*budgetBytes=*/3);
        std::set<uint32_t> cached;
        for (uint32_t i = 0; i < 10; i++) {
            policy.setCurrentIndex(i);
            cached.insert(i);
            for (uint32_t evict : policy.indicesToEvict(cached)) cached.erase(evict);
            check(cached.count(i) == 1,
                  "policy: current index stays cached after eviction");
        }
        check(cached.size() <= policy.maxResidentSlices(),
              "policy: cached set converges to the budget");
    }

    void testSliceCachePolicyEvictsFarthestFirst() {
        SliceCachePolicy policy(100, 1, 4);
        std::set<uint32_t> cached{0, 1, 2, 3, 10};
        policy.setCurrentIndex(10);
        auto evict = policy.indicesToEvict(cached);
        check(!evict.empty() && evict.front() == 0,
              "policy: farthest cached index evicted first");
    }

    void testSliceCachePolicyPrefetchIsDirectionAware() {
        SliceCachePolicy policy(100, 1, 1000);
        policy.setCurrentIndex(4);
        policy.setCurrentIndex(5);
        auto forward = policy.nextPrefetchTarget({5});
        check(forward.has_value() && *forward == 6,
              "policy: prefetch moves forward with scroll direction");

        policy.setCurrentIndex(4); // reverse direction
        auto backward = policy.nextPrefetchTarget({4});
        check(backward.has_value() && *backward == 3,
              "policy: prefetch flips direction on reversal");
    }

    void testSliceCachePolicyPrefetchExhausted() {
        SliceCachePolicy policy(100, 1, 1000);
        policy.setCurrentIndex(50);
        std::set<uint32_t> cached;
        for (uint32_t i = 50 - SliceCachePolicy::kTrailingWindow;
             i <= 50 + SliceCachePolicy::kPrefetchWindow; i++)
            cached.insert(i);
        auto target = policy.nextPrefetchTarget(cached);
        check(!target.has_value(), "policy: fully warmed window returns nullopt");
    }

    void testColormapLut() {
        using tomocam::colormap::Colormap;

        auto gray = tomocam::colormap::buildLut(Colormap::Grayscale);
        check(gray[0] == tomocam::colormap::Rgb{0, 0, 0},
              "colormap: grayscale[0] is black");
        check(gray[128] == tomocam::colormap::Rgb{128, 128, 128},
              "colormap: grayscale[128] is identity");
        check(gray[255] == tomocam::colormap::Rgb{255, 255, 255},
              "colormap: grayscale[255] is white");

        auto viridis = tomocam::colormap::buildLut(Colormap::Viridis);
        check(viridis[0] == tomocam::colormap::Rgb{68, 1, 84},
              "colormap: viridis[0] matches its anchor endpoint");
        check(viridis[255] == tomocam::colormap::Rgb{253, 231, 37},
              "colormap: viridis[255] matches its anchor endpoint");

        auto inferno = tomocam::colormap::buildLut(Colormap::Inferno);
        check(inferno[0] == tomocam::colormap::Rgb{0, 0, 4},
              "colormap: inferno[0] matches its anchor endpoint");
        check(inferno[255] == tomocam::colormap::Rgb{252, 255, 164},
              "colormap: inferno[255] matches its anchor endpoint");

        bool continuous = true;
        for (int i = 1; i < 256; i++)
            for (int c = 0; c < 3; c++)
                if (std::abs(int(viridis[i][c]) - int(viridis[i - 1][c])) > 32)
                    continuous = false;
        check(continuous, "colormap: viridis LUT has no implausible jumps");

        check(tomocam::colormap::next(Colormap::Grayscale) == Colormap::Viridis,
              "colormap: cycle Grayscale -> Viridis");
        check(tomocam::colormap::next(Colormap::Viridis) == Colormap::Inferno,
              "colormap: cycle Viridis -> Inferno");
        check(tomocam::colormap::next(Colormap::Inferno) == Colormap::Grayscale,
              "colormap: cycle Inferno -> Grayscale");
    }

} // namespace

int main() {
    testNormalizeSliceToU8();
    testTomlLiteParsesScalars();
    testTomlLiteSkipsMalformedLines();
    testLoadTomlLiteFileMissingReturnsEmpty();
    testEnsureDefaultConfigFileCreatesAndDoesNotOverwrite();
    testResolveCacheBudgetPrecedence();
    testSliceCachePolicyNeverEvictsCurrent();
    testSliceCachePolicyEvictsFarthestFirst();
    testSliceCachePolicyPrefetchIsDirectionAware();
    testSliceCachePolicyPrefetchExhausted();
    testColormapLut();

    if (g_failures > 0) {
        std::cerr << g_failures << " test(s) failed." << std::endl;
        return 1;
    }
    std::cout << "All tests passed." << std::endl;
    return 0;
}
