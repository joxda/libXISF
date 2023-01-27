#include <iostream>
#include <random>
#include <QElapsedTimer>
#include "libxisf.h"

using namespace LibXISF;

template<typename T>
void benchmarkType(float avg, float stdDev)
{
    std::mt19937 gen;
    std::normal_distribution<float> normalDist {avg, stdDev};

    UInt32 pixels = 2048*2048;
    UInt32 size = pixels*sizeof(T);
    QByteArray imageData;
    imageData.resize(size);
    T *ptr = (T*)imageData.data();
    for(UInt32 i=0; i < pixels; i++)
    {
        ptr[i] = normalDist(gen);
    }

    QElapsedTimer timer;
    Image image;
    image.width = 2048;
    image.height = 2048;
    image.dataBlock.data = imageData;
    double baseSize;

    {
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        QByteArray xisfImage;
        writer.save(xisfImage);
        baseSize = xisfImage.size();
        std::cout << "No compression      \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s" << std::endl;
    }
    {
        image.dataBlock.codec = DataBlock::Zlib;
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        QByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "Zlib compression    \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.dataBlock.codec = DataBlock::LZ4;
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        QByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4 compression     \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.dataBlock.codec = DataBlock::LZ4HC;
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        QByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4HC compression   \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    image.dataBlock.byteShuffling = sizeof(T);
    {
        image.dataBlock.codec = DataBlock::Zlib;
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        QByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "Zlib compression SH \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.dataBlock.codec = DataBlock::LZ4;
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        QByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4 compression SH  \tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
                  << size/1024.0/1.024/timer.elapsed() << "MiB/s\tRatio: " << baseSize/xisfImage.size() << std::endl;
    }
    {
        image.dataBlock.codec = DataBlock::LZ4HC;
        timer.start();
        XISFWriter writer;
        writer.writeImage(image);
        QByteArray xisfImage;
        writer.save(xisfImage);
        std::cout << "LZ4HC compression SH\tElapsed time: " << timer.elapsed() << " " << "ms\tSpeed: "
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
