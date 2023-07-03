#include <iostream>
#include <random>
#include <chrono>
#include "libxisf.h"

using namespace LibXISF;

class Timer
{
    std::chrono::high_resolution_clock clock;
    std::chrono::high_resolution_clock::time_point startTime;
public:
    void start()
    {
        startTime = clock.now();
    }
    uint64_t elapsed()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(clock.now() - startTime).count();
    }
};

template<typename T>
void benchmarkType(float avg, float stdDev)
{
    std::mt19937 gen;
    std::normal_distribution<float> normalDist {avg, stdDev};

    Image image(2048, 2048, 1, Image::sampleFormatEnum<T>());
    UInt32 pixels = 2048*2048;
    UInt32 size = pixels*sizeof(T);
    T *ptr = image.imageData<T>();
    for(UInt32 i=0; i < pixels; i++)
    {
        ptr[i] = normalDist(gen);
    }

    Timer timer;
    double baseSize;

    {
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        baseSize = xisfImage.size();
        std::cout << "No compression      \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s" << std::endl;
    }
    {
        image.setCompression(DataBlock::Zlib);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "Zlib compression    \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.setCompression(DataBlock::LZ4);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4 compression     \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.setCompression(DataBlock::LZ4HC);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4HC compression   \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    if(DataBlock::CompressionCodecSupported(DataBlock::ZSTD))
    {
        image.setCompression(DataBlock::ZSTD);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "ZSTD compression   \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    image.setByteshuffling(true);
    {
        image.setCompression(DataBlock::Zlib);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "Zlib compression SH \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.setCompression(DataBlock::LZ4);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4 compression SH  \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.setCompression(DataBlock::LZ4HC);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4HC compression SH\tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    if(DataBlock::CompressionCodecSupported(DataBlock::ZSTD))
    {
        image.setCompression(DataBlock::ZSTD);
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        ByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "ZSTD compression SH\tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
}

void benchmark()
{
    std::cout << "UInt16 sample type" << std::endl;
    benchmarkType<UInt16>(500, 30);
    std::cout << "Float32 sample type" << std::endl;
    benchmarkType<float>(500 / 65535.0, 30 / 65535.0);
}
