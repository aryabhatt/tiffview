#include <cstdlib>
#include <iostream>
#include <string>

#include "io/array.h"
#include "io/tiff/tiffio.h"

template <typename T>
T random() {
    T NORM = static_cast<T>(RAND_MAX);
    return static_cast<T>(rand()) / NORM;
}

bool write_tiff() {

    // create a filename
    int counter = 13;
    char pathbuf[20];
    sprintf(pathbuf, "test%04d.tif", counter);
    auto path = std::string(pathbuf);
    return true;
}

int main(int argc, char **argv) {

    if (!write_tiff()) { std::cerr << "failed tiff write test.\n"; }
    return 0;
}
