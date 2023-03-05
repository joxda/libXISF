/************************************************************************
 * LibXISF - library to load and save XISF files                        *
 * Copyright (C) 2023 Dušan Poizl                                       *
 *                                                                      *
 * This program is free software: you can redistribute it and/or modify *
 * it under the terms of the GNU General Public License as published by *
 * the Free Software Foundation, either version 3 of the License, or    *
 * (at your option) any later version.                                  *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.*
 ************************************************************************/

#include <iostream>
#include "libxisf.h"

using namespace LibXISF;

void benchmark();

#define TEST(cond, msg) if(cond){ std::cerr << msg << std::endl; return 1; }

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
        {
            XISFWriter writer;
            Image image(5, 7);
            image.setImageType(Image::Light);
            image.addProperty(Property("PropertyString", "Hello XISF"));
            image.addProperty(Property("PropertyBoolean", (Boolean)true));
            image.addProperty(Property("PropertyInt8", (Int8)(8)));
            image.addProperty(Property("PropertyInt16", (Int16)16));
            image.addProperty(Property("PropertyInt32", 32));
            image.addProperty(Property("PropertyUInt8", (UInt8)8));
            image.addProperty(Property("PropertyUInt16", (UInt16)(16)));
            image.addProperty(Property("PropertyUInt32", (uint32_t)32));
            image.addProperty(Property("PropertyFloat32", (Float32) 0.32));
            image.addProperty(Property("PropertyFloat64", (Float64) 0.64));
            image.addProperty(Property("PropertyComplex32", Complex32{3.0, -2.0}));
            image.addProperty(Property("PropertyComplex64", Complex64{-3.0, 2.0}));
            image.addProperty(Property("VectorUInt16", UI16Vector({23, 45, 86})));
            image.addProperty(Property("VectorComplex32", C32Vector({{1, 2}, {3, 4}, {5, 6}})));
            UI16Matrix m(2, 3);
            m(0, 0) = 0;
            m(0, 1) = 1;
            m(0, 2) = 2;
            m(1, 0) = 10;
            image.addProperty(Property("UI16Matrix", m));
            std::tm tm = {12, 22, 23, 1, 2, 2023, 0, 0, 0, 0, 0};
            image.addProperty(Property("TimeObs", tm));
            image.addFITSKeyword({"RA", "226.9751163116387", "Right ascension of the center of the image (deg)"});
            image.addFITSKeyword({"DEC", "62.02302376908295", "Declination of the center of the image (deg)"});
            image.setCompression(DataBlock::Zlib, 9);
            writer.writeImage(image);

            image.setImageType(Image::Flat);
            image.setCompression(DataBlock::LZ4);
            image.setByteshuffling(true);
            writer.writeImage(image);
            ByteArray data;
            std::cout << "Saving image" << std::endl;
            writer.save(data);

            XISFReader reader;
            std::cout << "Loading image" << std::endl;
            reader.open(data);
            const Image &img0 = reader.getImage(0);
            const Image &img1 = reader.getImage(1);

            TEST(image.imageProperties().size() != img0.imageProperties().size(), "Property count doesn't match");
            TEST(std::memcmp(image.imageData(), img0.imageData(), image.imageDataSize()), "Images doesn't match");
            TEST(std::memcmp(image.imageData(), img1.imageData(), image.imageDataSize()), "Images doesn't match");
        }
        else if(argc == 2 && std::strcmp(argv[1], "bench") == 0)
        {
            benchmark();
        }
        else
        {
            LibXISF::XISFReader reader;
            reader.open(argv[1]);
            TEST(reader.imagesCount() != 1, "No image");

            const LibXISF::Image &image = reader.getImage(0);
            TEST(image.width() != 8, "Invalid width")
            TEST(image.height() != 10, "Invalid height");
            TEST(image.colorSpace() != LibXISF::Image::Gray, "Invalid color space");
            TEST(image.pixelStorage() != LibXISF::Image::Planar, "Invalid pixel storage");
            TEST(image.compression() != LibXISF::DataBlock::None, "Invalid compression codec");
            //TEST(!image.dataBlock.embedded, "Not embedded");
            TEST(image.imageDataSize() != 80*2, "Invalid data size");
        }
    }
    catch (const LibXISF::Error &e)
    {
        std::cout << e.what() << std::endl;
        return 2;
    }
    return 0;
}
