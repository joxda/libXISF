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

#ifndef LIBXISF_H
#define LIBXISF_H

#include "libXISF_global.h"
#include <memory>
#include <QIODevice>
#include <QVariant>
#include <QXmlStreamReader>

class QXmlStreamReader;

namespace LibXISF
{

struct DataBlock
{
    enum CompressionCodec
    {
        None,
        Zlib,
        LZ4,
        LZ4HC
    };
    bool embedded = false;
    uint32_t byteShuffling = 0;
    uint64_t attachmentPos = 0;
    uint64_t attachmentSize = 0;
    uint64_t uncompressedSize = 0;
    CompressionCodec codec = None;
    QByteArray data;
    void decompress(const QByteArray &input, const QString &encoding = "");
    void compress();
};

struct Property
{
    QString id;
    QVariant value;
    QString comment;
    QString format;
};

struct Image
{
    enum Type
    {
        Bias,
        Dark,
        Flat,
        Light,
        MasterBias,
        MasterDark,
        MasterFlat,
        DefectMap,
        RejectionMapHigh,
        RejectionMapLow,
        BinaryRejectionMapHigh,
        BinaryRejectionMapLow,
        SlopeMap,
        WeightMap
    };
    enum PixelStorage
    {
        Planar,
        Normal
    };
    enum SampleFormat
    {
        UInt8,
        UInt16,
        UInt32,
        UInt64,
        Float32,
        Float64,
        Complex32,
        Complex64
    };
    enum ColorSpace
    {
        Gray,
        RGB,
        CIELab
    };

    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t channelCount = 0;
    double bounds[2] = {0.0, 1.0};
    Type imageType = Light;
    PixelStorage pixelStorage = Planar;
    SampleFormat sampleFormat = UInt16;
    ColorSpace colorSpace = Gray;
    DataBlock dataBlock;
    QByteArray iccProfile;
    std::vector<Property> properties;

    void convertPixelStorageTo(PixelStorage storage);

    static Type imageTypeEnum(const QString &type);
    static PixelStorage pixelStorageEnum(const QString &storage);
    static SampleFormat sampleFormatEnum(const QString &format);
    static ColorSpace colorSpaceEnum(const QString &colorSpace);
};

class LIBXISF_EXPORT XISFReader
{
public:
    XISFReader();
    void open(const QString &name);
    void open(const QByteArray &data);
    void open(QIODevice *io);
    void close();
    int imagesCount() const;
    const Image& getImage(uint32_t n);
private:
    void readXISFHeader();
    void readSignature();
    void readImageElement();
    Property readPropertyElement();
    void readDataElement(DataBlock &dataBlock);
    DataBlock readDataBlock();
    void readCompression(DataBlock &dataBlock);

    std::unique_ptr<QIODevice> _io;
    std::unique_ptr<QXmlStreamReader> _xml;
    std::vector<Image> _images;
    std::vector<Property> _properties;
};

class LIBXISF_EXPORT XISFWriter
{
public:
    XISFWriter();
    void save(const QString &name);
    void save(QByteArray &data);
    void save(QIODevice &io);
    void writeImage(const Image &image);
private:
    void writeHeader();
    void writeImageElement(const Image &image);
    void writeDataBlockAttributes(const DataBlock &dataBlock);
    void writeCompressionAttributes(const DataBlock &dataBlock);
    void writePropertyElement(const Property &property);
    void writeMetadata();
    std::unique_ptr<QXmlStreamWriter> _xml;
    QByteArray _xisfHeader;
    QByteArray _attachmentsData;
    std::vector<Image> _images;
};

struct Complex32
{
    float real;
    float imag;
};

struct Complex64
{
    double real;
    double imag;
};

template<typename T>
class Matrix
{
    int width;
    int height;
    std::vector<T> elem;
};

typedef bool Boolean;
typedef int8_t Int8;
typedef uint8_t UInt8;
typedef int16_t Int16;
typedef uint16_t UInt16;
typedef int32_t Int32;
typedef uint32_t UInt32;
typedef int64_t Int64;
typedef uint64_t UInt64;
typedef float Float32;
typedef double Float64;
typedef QDateTime TimePoint;
typedef std::vector<int8_t> I8Vector;
typedef std::vector<uint8_t> UI8Vector;
typedef std::vector<int16_t> I16Vector;
typedef std::vector<uint16_t> UI16Vector;
typedef std::vector<int32_t> I32Vector;
typedef std::vector<uint32_t> UI32Vector;
typedef std::vector<int64_t> I64Vector;
typedef std::vector<uint64_t> UI64Vector;
typedef std::vector<float> F32Vector;
typedef std::vector<double> F64Vector;
typedef std::vector<Complex32> C32Vector;
typedef std::vector<Complex64> C64Vector;
typedef Matrix<Int8> I8Matrix;
typedef Matrix<UInt8> UI8Matrix;
typedef Matrix<Int16> I16Matrix;
typedef Matrix<UInt16> UI16Matrix;
typedef Matrix<Int32> I32Matrix;
typedef Matrix<UInt32> UI32Matrix;
typedef Matrix<Int64> I64Matrix;
typedef Matrix<UInt64> UI64Matrix;
typedef Matrix<float> F32Matrix;
typedef Matrix<double> F64Matrix;
typedef Matrix<Complex32> C32Matrix;
typedef Matrix<Complex64> C64Matrix;
typedef QString String;

}

QDebug operator<<(QDebug dbg, const LibXISF::Complex32 &c);

Q_DECLARE_METATYPE(LibXISF::Boolean);
Q_DECLARE_METATYPE(LibXISF::Int8);
Q_DECLARE_METATYPE(LibXISF::UInt8);
Q_DECLARE_METATYPE(LibXISF::Int16);
Q_DECLARE_METATYPE(LibXISF::UInt16);
Q_DECLARE_METATYPE(LibXISF::Int32);
Q_DECLARE_METATYPE(LibXISF::UInt32);
Q_DECLARE_METATYPE(LibXISF::Int64);
Q_DECLARE_METATYPE(LibXISF::UInt64);
Q_DECLARE_METATYPE(LibXISF::Float32);
Q_DECLARE_METATYPE(LibXISF::Float64);
Q_DECLARE_METATYPE(LibXISF::Complex32);
Q_DECLARE_METATYPE(LibXISF::Complex64);
Q_DECLARE_METATYPE(LibXISF::I8Vector);
Q_DECLARE_METATYPE(LibXISF::UI8Vector);
Q_DECLARE_METATYPE(LibXISF::I16Vector);
Q_DECLARE_METATYPE(LibXISF::UI16Vector);
Q_DECLARE_METATYPE(LibXISF::I32Vector);
Q_DECLARE_METATYPE(LibXISF::UI32Vector);
Q_DECLARE_METATYPE(LibXISF::I64Vector);
Q_DECLARE_METATYPE(LibXISF::UI64Vector);
Q_DECLARE_METATYPE(LibXISF::F32Vector);
Q_DECLARE_METATYPE(LibXISF::F64Vector);
Q_DECLARE_METATYPE(LibXISF::C32Vector);
Q_DECLARE_METATYPE(LibXISF::C64Vector);
Q_DECLARE_METATYPE(LibXISF::I8Matrix);
Q_DECLARE_METATYPE(LibXISF::UI8Matrix);
Q_DECLARE_METATYPE(LibXISF::I16Matrix);
Q_DECLARE_METATYPE(LibXISF::UI16Matrix);
Q_DECLARE_METATYPE(LibXISF::I32Matrix);
Q_DECLARE_METATYPE(LibXISF::UI32Matrix);
Q_DECLARE_METATYPE(LibXISF::I64Matrix);
Q_DECLARE_METATYPE(LibXISF::UI64Matrix);
Q_DECLARE_METATYPE(LibXISF::F32Matrix);
Q_DECLARE_METATYPE(LibXISF::F64Matrix);
Q_DECLARE_METATYPE(LibXISF::C32Matrix);
Q_DECLARE_METATYPE(LibXISF::C64Matrix);
Q_DECLARE_METATYPE(LibXISF::String);

#endif // LIBXISF_H
