#include <QApplication>
#include <QMessageBox>
#include <iostream>

#include "image_viewer.h"
#include "io/array.h"
#include "io/tiff/tiffio.h"

int main(int argc, char **argv) {
    // Check arguments before initializing Qt
    std::string filename;
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <tiff_file>" << std::endl;
        std::cerr << "\nKeyboard controls:" << std::endl;
        std::cerr << "  Up/Down arrows : Navigate pages" << std::endl;
        std::cerr << "  PageUp/PageDown: Jump 5 pages" << std::endl;
        std::cerr << "  Home/End       : First/Last page" << std::endl;
        std::cerr << "  Mouse wheel    : Navigate pages (Ctrl+wheel: 5 pages)"
                  << std::endl;
        std::cerr << "  Ctrl+1/2/3     : View along Z/Y/X axis" << std::endl;
        std::cerr << "  Ctrl+T         : Cycle view axis" << std::endl;
        return 1;
    } else {
        filename = argv[1];
    }

    QApplication app(argc, argv);

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

    if (imageData.size() == 0) {
        std::cerr << "Error: Empty or invalid TIFF file" << std::endl;
        return 1;
    }

    ImageViewer viewer(imageData);
    viewer.setWindowTitle(
        QString("TIFF Viewer - %1").arg(QString::fromStdString(filename)));
    viewer.resize(800, 600);
    viewer.show();

    return app.exec();
}
