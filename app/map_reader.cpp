#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <istream>
#include <stdexcept>
#include <fstream>

// #include <turbojpeg.h>
#include <png++/png.hpp>

namespace {

auto validateArgs(int argc, const char * const argv[]) 
    -> std::pair<std::filesystem::path, std::filesystem::path> {
    if(argc != 3) {
        throw std::runtime_error("Invalid number of arguments");
    }

    const char * const mapFileStr = argv[1];
    const char * const outputPathStr = argv[2];

    using namespace std::filesystem;
    
    path mapFile{mapFileStr};
    if(!is_regular_file(mapFile)) {
        throw std::runtime_error(std::string(mapFileStr) + " is not a regular file");
    }

    path outputPath{outputPathStr};
    if(exists(outputPath) && !is_directory(outputPath)) {
        throw std::runtime_error(std::string(outputPath) + " is not a folder");
    }

    if(exists(outputPath)) {
        remove_all(outputPath);
    }

    return std::make_pair(std::move(mapFile), std::move(outputPath));
}

void printUsage() {
    std::clog << "Usage:\n"
                 "map_reader mapfile.map output_dir/" << std::endl;
}

struct MapData {
    std::uint32_t width, height;
    std::uint64_t bytesPerMap;
};

MapData readMapHeader(std::istream &ins) {
    MapData res; // NOLINT
    std::istream::iostate prevIoState = ins.exceptions();
    ins.exceptions(std::istream::badbit);
    ins.read(reinterpret_cast<char*>(&res.width), sizeof(res.width)); // NOLINT
    ins.read(reinterpret_cast<char*>(&res.height), sizeof(res.height)); // NOLINT
    ins.read(reinterpret_cast<char*>(&res.bytesPerMap), sizeof(res.bytesPerMap)); // NOLINT

    if(ins.fail()) {
        throw std::runtime_error("Failed to read map header!");
    }
    
    ins.exceptions(prevIoState);

    return res;
}

struct RGB888 {
    std::uint8_t r, g, b;
};

struct MapGenConfig {
    RGB888 water, fish, shark;
};

#if 0

bool readMap(std::istream &ins, const MapData &map, RGB888 water, RGB888 fish, RGB888 shark, std::uint8_t *res) {
    std::istream::iostate prevIoState = ins.exceptions();
    ins.clear();

    ins.exceptions(std::istream::badbit);

    ins.peek();
    if(ins.fail()) {
        // here we should have ins.eof()
        ins.exceptions(prevIoState);
        return false;
    }

    ins.exceptions(std::istream::failbit);

    std::size_t curPos = 0;
    unsigned shiftCnt = 8;
    std::uint8_t buffer = 0;

    for(std::uint32_t i=0; i<map.height; ++i) {
        for(std::uint32_t j=0; j<map.width; ++j) {
            if(shiftCnt >= 8) {
                ins.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));
                shiftCnt = 0;
            }
            unsigned curEnt = buffer & 0x03U; 
            buffer >>= 2U; shiftCnt += 2;
            
            RGB888 curCol {};

            switch(curEnt) {
                case 0: curCol = water; break;
                case 1: curCol = fish; break;
                case 2: curCol = shark; break;
                default: assert(0);
            }

            res[curPos++] = curCol.r;
            res[curPos++] = curCol.g;
            res[curPos++] = curCol.b;
        }
    }

    ins.exceptions(prevIoState);
    return true;
}


void writeFrames(std::istream &ins, const std::filesystem::path &outputPath, const MapData &mapData, const MapGenConfig &conf) {
    std::unique_ptr<std::uint8_t[]> fbf =  // NOLINT
        std::make_unique<std::uint8_t[]>(static_cast<std::size_t>(mapData.width) * mapData.height * 3); // NOLINT

    using namespace std::filesystem;

    std::size_t frame = 0;

    tjhandle rawHandleTJ = tjInitCompress();
    if(rawHandleTJ == nullptr) {
        throw std::runtime_error(std::string("Failed to init turbo jpeg") + tjGetErrorStr2(nullptr));
    }
    std::unique_ptr<void, void(*)(void*)> handleTJ{rawHandleTJ, [](void *handle) -> void {
        tjDestroy(handle);
    }};

    std::size_t imgBufSize = tjBufSize(mapData.width, mapData.height, TJSAMP_422);
    std::unique_ptr<std::uint8_t[]> imgBuf = std::make_unique<std::uint8_t[]>(imgBufSize);

    bool hasNewFrame = true;
    do { // NOLINT

        hasNewFrame = readMap(ins, mapData, conf.water, conf.fish, conf.shark, fbf.get());
        if(!hasNewFrame) {
            return;
        }

        path outputPicPath = outputPath/(std::to_string(frame) + ".jpeg");

        uint8_t *rawImgBuf = imgBuf.get();
        std::size_t curImgSize = imgBufSize;
        int ret = tjCompress2(handleTJ.get(), fbf.get(), mapData.width, 0, mapData.height, 
                                TJPF_RGB, &rawImgBuf, &curImgSize, TJSAMP_422, 100, TJFLAG_NOREALLOC);

        if(ret != 0) {
            throw std::runtime_error("Failed converting to JPEG, reason: " + std::string(tjGetErrorStr2(handleTJ.get())));
        }

        std::fstream imgFout{outputPicPath.c_str(), std::fstream::out | std::fstream::trunc};
        imgFout.exceptions(std::fstream::failbit | std::fstream::badbit);
        imgFout.write(reinterpret_cast<const char*>(imgBuf.get()), curImgSize);
        imgFout.close();

        ++ frame;
    } while(hasNewFrame);
}

#else 

bool readMap(std::istream &ins, const MapData &map, png::image<png::index_pixel_2> &image) {
    std::istream::iostate prevIoState = ins.exceptions();
    ins.clear();

    ins.exceptions(std::istream::badbit);

    ins.peek();
    if(!ins.good()) {
        // here we should have ins.eof()
        ins.exceptions(prevIoState);
        return false;
    }

    ins.exceptions(std::istream::failbit);

    std::size_t curPos = 0;
    unsigned shiftCnt = 8;
    std::uint8_t buffer = 0;

    for(std::uint32_t i=0; i<map.height; ++i) {
        for(std::uint32_t j=0; j<map.width; ++j) {
            if(shiftCnt >= 8) {
                ins.read(reinterpret_cast<char*>(&buffer), sizeof(buffer));
                shiftCnt = 0;
            }
            unsigned curEnt = buffer & 0x03U; 
            buffer >>= 2U; shiftCnt += 2;

            // 0 - water
            // 1 - fish
            // 2 - shark
            if(!(0 <= curEnt && curEnt <= 2)) {
                // image is corrupted
                throw std::runtime_error("Corrupted image");
            }

            image[i][j] = static_cast<png::byte>(curEnt);
        }
    }

    ins.exceptions(prevIoState);
    return true;
}



void writeFrames(std::istream &ins, const std::filesystem::path &outputPath, const MapData &mapData, const MapGenConfig &conf) {
    png::palette pal = {png::color(conf.water.r, conf.water.g, conf.water.b),
                        png::color(conf.fish.r, conf.fish.g, conf.fish.b),
                        png::color(conf.shark.r, conf.shark.g, conf.shark.b)};

    png::image<png::index_pixel_2> image(mapData.width, mapData.height);
    image.set_palette(pal);

    using namespace std::filesystem;

    std::size_t frame = 0;

    bool hasNewFrame = true;
    do { // NOLINT

        hasNewFrame = readMap(ins, mapData, image);
        if(!hasNewFrame) {
            return;
        }

        path outputPicPath = outputPath/(std::to_string(frame) + ".png");

        image.write(outputPicPath.c_str());

        ++ frame;
    } while(hasNewFrame);
}

#endif

}

int main(int argc, char **argv) {

    using namespace std::filesystem;

    path mapFile;
    path outputPath;

    try {
        auto res = validateArgs(argc, argv);
        mapFile = std::move(res.first);
        outputPath = std::move(res.second);
    } catch(const std::exception &ex) {
        std::clog << ex.what() << '\n';
        printUsage();
    }

    create_directory(outputPath);

    std::fstream fin(mapFile.c_str(), std::fstream::in);

    fin.exceptions(std::fstream::badbit);
    
    MapData mapData = readMapHeader(fin);
    
    // MapGenConfig conf { {0, 0, 255}, {0, 255, 0}, {255, 0, 0} };
    MapGenConfig conf { {0, 0, 255}, {108, 102, 112}, {255, 87, 51} };

    writeFrames(fin, outputPath, mapData, conf);

    return 0;
}
