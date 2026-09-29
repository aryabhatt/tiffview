#include <QApplication>
#include <QMessageBox>
#include <iostream>
#include <optional>

#include "config.h"
#include "image_viewer.h"
#include "io/array.h"
#include "io/tiff/tiff_reader.h"
#include "io/tiff/tiffio.h"
#include "slice_cache.h"

static void printUsage(const char *argv0) {
    std::cerr << "Usage: " << argv0 << " [--cache-mb N] <tiff_file>" << std::endl;
    std::cerr << "\nKeyboard controls:" << std::endl;
    std::cerr << "  Up/Down arrows : Navigate pages" << std::endl;
    std::cerr << "  PageUp/PageDown: Jump 5 pages" << std::endl;
    std::cerr << "  Home/End       : First/Last page" << std::endl;
    std::cerr << "  Mouse wheel    : Navigate pages (Ctrl+wheel: 5 pages)"
              << std::endl;
    std::cerr << "  Ctrl+1/2/3     : View along Z/Y/X axis" << std::endl;
    std::cerr << "  Ctrl+T         : Cycle view axis" << std::endl;
    std::cerr << "  Z/X            : Zoom in/out" << std::endl;
    std::cerr << "  R              : Fit image to window" << std::endl;
    std::cerr
        << "  C              : Cycle colormap (Grayscale -> Viridis -> Inferno)"
        << std::endl;
    std::cerr << "  Q              : Quit" << std::endl;
    std::cerr << "\nOptions:" << std::endl;
    std::cerr << "  --cache-mb N   : cache budget in MB for the Z-axis slice cache "
                 "(default "
              << tomocam::config::kDefaultCacheMb
              << "; overrides ~/.config/tiffview/config.toml)" << std::endl;
}

int main(int argc, char **argv) {
    // Check arguments before initializing Qt
    std::string filename;
    std::optional<int64_t> cliCacheMb;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--cache-mb") {
            if (i + 1 >= argc) {
                std::cerr << "Error: --cache-mb requires a value" << std::endl;
                return 1;
            }
            try {
                cliCacheMb = std::stoll(argv[++i]);
            } catch (const std::exception &) {
                std::cerr << "Error: invalid --cache-mb value '" << argv[i] << "'"
                          << std::endl;
                return 1;
            }
        } else if (arg.rfind("--cache-mb=", 0) == 0) {
            try {
                cliCacheMb = std::stoll(arg.substr(11));
            } catch (const std::exception &) {
                std::cerr << "Error: invalid --cache-mb value '" << arg.substr(11)
                          << "'" << std::endl;
                return 1;
            }
        } else if (filename.empty()) {
            filename = arg;
        } else {
            std::cerr << "Error: unexpected argument '" << arg << "'" << std::endl;
            return 1;
        }
    }

    if (filename.empty()) {
        printUsage(argv[0]);
        return 1;
    }

    tomocam::config::ensureDefaultConfigFile();
    size_t cacheBudgetBytes =
        tomocam::config::resolveCacheBudgetBytesFromDisk(cliCacheMb);

    tomocam::tiff::TiffMeta meta;
    try {
        meta = tomocam::tiff::probe(filename);
    } catch (const std::exception &e) {
        std::cerr << "Error probing file: " << e.what() << std::endl;
        return 1;
    }

    if (meta.n_pages == 0 || meta.width == 0 || meta.height == 0) {
        std::cerr << "Error: Empty or invalid TIFF file" << std::endl;
        return 1;
    }

    QApplication app(argc, argv);

    uint64_t fullVolumeBytes = tomocam::tiff::normalizedVolumeBytes(meta);
    std::unique_ptr<ImageViewer> viewer;

    if (fullVolumeBytes <= cacheBudgetBytes) {
        tomocam::Array<uint8_t> imageData;
        try {
            imageData = tomocam::tiff::read(filename);
            std::cout << "Loaded TIFF file: " << filename << std::endl;
            std::cout << "  Pages: " << imageData.nslices() << std::endl;
            std::cout << "  Dimensions: " << imageData.nrows() << " x "
                      << imageData.ncols() << std::endl;
        } catch (const std::exception &e) {
            std::cerr << "Error loading file: " << e.what() << std::endl;
            return 1;
        }
        viewer = std::make_unique<ImageViewer>(imageData);
    } else {
        std::unique_ptr<SliceCache> cache;
        try {
            cache = std::make_unique<SliceCache>(filename, meta, cacheBudgetBytes);
        } catch (const std::exception &e) {
            std::cerr << "Error opening file for streaming: " << e.what()
                      << std::endl;
            return 1;
        }
        std::cout << "Streaming large TIFF stack ("
                  << fullVolumeBytes / (1024 * 1024) << " MB) with a "
                  << cacheBudgetBytes / (1024 * 1024)
                  << " MB slice cache; Y/X views disabled." << std::endl;
        viewer = std::make_unique<ImageViewer>(std::move(cache), fullVolumeBytes,
                                               cacheBudgetBytes);
    }

    viewer->setWindowTitle(
        QString("TIFF Viewer - %1").arg(QString::fromStdString(filename)));
    viewer->resize(800, 600);
    viewer->show();

    return app.exec();
}
