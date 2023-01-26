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

#define TEST(cond, msg) if(cond){ std::cerr << msg << std::endl; return 1; }

int main(int argc, char **argv)
{
    try
    {
        if (argc < 2)
        {
            XISFWriter writer;
            Image image;
            image.width = 5;
            image.height = 7;
            image.imageType = Image::Light;
            image.dataBlock.data.resize(image.width*image.height*2);
            image.properties.push_back(Property("PropertyString", "Hello XISF"));
            image.properties.push_back(Property("PropertyBoolean", (Boolean)true));
            image.properties.push_back(Property("PropertyInt8", (Int8)(8)));
            image.properties.push_back(Property("PropertyInt16", (Int16)16));
            image.properties.push_back(Property("PropertyInt32", 32));
            image.properties.push_back(Property("PropertyUInt8", (UInt8)8));
            image.properties.push_back(Property("PropertyUInt16", (UInt16)(16)));
            image.properties.push_back(Property("PropertyUInt32", (uint32_t)32));
            image.properties.push_back(Property("PropertyFloat32", (Float32) 0.32));
            image.properties.push_back(Property("PropertyFloat64", (Float64) 0.64));
            image.properties.push_back(Property("PropertyComplex32", Complex32{3.0, -2.0}));
            image.properties.push_back(Property("PropertyComplex64", Complex64{-3.0, 2.0}));
            image.fitsKeywords.push_back({"RA", "226.9751163116387", "Right ascension of the center of the image (deg)"});
            image.fitsKeywords.push_back({"DEC", "62.02302376908295", "Declination of the center of the image (deg)"});
            writer.writeImage(image);

            image.imageType = Image::Flat;
            image.dataBlock.codec = DataBlock::LZ4;
            image.dataBlock.byteShuffling = 2;
            writer.writeImage(image);
            QByteArray data;
            std::cout << "Saving image" << std::endl;
            writer.save(data);

            XISFReader reader;
            std::cout << "Loading image" << std::endl;
            reader.open(data);
            const Image &img0 = reader.getImage(0);
            const Image &img1 = reader.getImage(1);
            TEST(image.properties.size() != img0.properties.size(), "Property count doesn't match");
            TEST(image.dataBlock.data != img0.dataBlock.data, "Images doesn't match");
            TEST(img0.dataBlock.data != img1.dataBlock.data, "Images doesn't match");
        }
        else
        {
            LibXISF::XISFReader reader;
            reader.open(QString(argv[1]));
            TEST(reader.imagesCount() != 1, "No image");

            const LibXISF::Image &image = reader.getImage(0);
            TEST(image.width != 8, "Invalid width")
            TEST(image.height != 10, "Invalid height");
            TEST(image.colorSpace != LibXISF::Image::Gray, "Invalid color space");
            TEST(image.pixelStorage != LibXISF::Image::Planar, "Invalid pixel storage");
            //TEST(image.dataBlock.codec != LibXISF::DataBlock::None, "Invalid compression codec");
            TEST(!image.dataBlock.embedded, "Not embedded");
            TEST(image.dataBlock.data.size() != 80*2, "Invalid data size");
        }
    }
    catch (const LibXISF::Error &e)
    {
        std::cout << e.what() << std::endl;
        return 2;
    }
    return 0;
}
