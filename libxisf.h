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
#include <QDateTime>

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
    int compressLevel = -1;
    QByteArray data;
    void decompress(const QByteArray &input, const QString &encoding = "");
    void compress();
};

struct LIBXISF_EXPORT Property
{
    QString id;
    QVariant value;
    QString comment;

    Property() = default;
    Property(const Property &) = default;
    Property(const QString &_id, const char *_value);
    template<typename T>
    Property(const QString &_id, const T& _value) :
        id(_id),
        value(QVariant::fromValue<T>(_value)){}
};

struct LIBXISF_EXPORT FITSKeyword
{
    QString name;
    QString value;
    QString comment;
};

/**
Describe color filter array. Each letter in pattern describe color of element.
0 - A nonexistent or undefined CFA element
R - Red
G - Green
B - Blue
W - White or panchromatic
C - Cyan
M - Magenta
Y - Yellow
*/
struct LIBXISF_EXPORT ColorFilterArray
{
    int width = 0;
    int height = 0;
    QString pattern;
};

typedef std::pair<double, double> Bounds;

class LIBXISF_EXPORT Image
{
public:
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
    /**
    Planar - each channel samples are stored separately for example RGB image will be stored RRRRGGGGBBBB
    Normal - channel values for each pixel are stored inteleaved RGBRGBRGBRGB */
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
    Image() = default;
    Image(uint64_t width, uint64_t height, uint64_t channelCount = 1, SampleFormat sampleFormat = UInt16, ColorSpace colorSpace = Gray, PixelStorage pixelStorate = Planar);

    uint64_t width() const;
    uint64_t height() const;
    uint64_t channelCount() const;
    void setGeometry(uint64_t width, uint64_t height, uint64_t channelCount);
    const Bounds &bounds() const;
    void setBounds(const Bounds &newBounds);
    Type imageType() const;
    void setImageType(Type newImageType);
    PixelStorage pixelStorage() const;
    void setPixelStorage(PixelStorage newPixelStorage);
    SampleFormat sampleFormat() const;
    void setSampleFormat(SampleFormat newSampleFormat);
    ColorSpace colorSpace() const;
    void setColorSpace(ColorSpace newColorSpace);
    const ColorFilterArray colorFilterArray() const;
    void setColorFilterArray(const ColorFilterArray cfa);
    const std::vector<Property> &imageProperties() const;
    void addProperty(const Property &property);
    void updateProperty(const Property &property);
    const std::vector<FITSKeyword> fitsKeywords() const;
    void addFITSKeyword(const FITSKeyword &keyword);
    /** Add image property while doing automatic conversion of FITS name to XISF property
     *  For example OBSERVER => Observer:Name, SITELAT => Observation:Location:Latitude
    */
    bool addFITSKeywordAsProperty(const QString &name, const QVariant &value);

    void* imageData();
    template<typename T>
    T* imageData(){ return static_cast<T*>(imageData()); }
    size_t imageDataSize() const;
    DataBlock::CompressionCodec compression() const;
    void setCompression(DataBlock::CompressionCodec compression, int level = -1);
    bool byteShuffling() const;
    void setByteshuffling(bool enable);

    /** Convert between Planar and Normal storage format s*/
    void convertPixelStorageTo(PixelStorage storage);

    static Type imageTypeEnum(const QString &type);
    static QString imageTypeString(Type type);
    static PixelStorage pixelStorageEnum(const QString &storage);
    static QString pixelStorageString(PixelStorage storage);
    static SampleFormat sampleFormatEnum(const QString &format);
    template<typename T>
    static SampleFormat sampleFormatEnum();
    static QString sampleFormatString(SampleFormat format);
    static ColorSpace colorSpaceEnum(const QString &colorSpace);
    static QString colorSpaceString(ColorSpace colorSpace);
    static size_t sampleFormatSize(SampleFormat sampleFormat);

private:
    uint64_t _width = 0;
    uint64_t _height = 0;
    uint64_t _channelCount = 1;
    Bounds _bounds = {0.0, 1.0};
    Type _imageType = Light;
    PixelStorage _pixelStorage = Planar;
    SampleFormat _sampleFormat = UInt16;
    ColorSpace _colorSpace = Gray;
    DataBlock _dataBlock;
    QByteArray _iccProfile;
    ColorFilterArray _cfa;
    std::vector<Property> _properties;
    std::map<QString, uint32_t> _propertiesId;
    std::vector<FITSKeyword> _fitsKeywords;

    friend class XISFReader;
    friend class XISFWriter;
};

class LIBXISF_EXPORT XISFReader
{
public:
    XISFReader();
    void open(const QString &name);
    void open(const QByteArray &data);
    /** Open image from QIODevice. This method takes ownership of *io pointer */
    void open(QIODevice *io);
    /** Close opended file release all data. */
    void close();
    /** Return number of images inside file */
    int imagesCount() const;
    const Image& getImage(uint32_t n);
private:
    void readXISFHeader();
    void readSignature();
    void readImageElement();
    Property readPropertyElement();
    FITSKeyword readFITSKeyword();
    void readDataElement(DataBlock &dataBlock);
    DataBlock readDataBlock();
    void readCompression(DataBlock &dataBlock);
    ColorFilterArray readCFA();

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
    void writeFITSKeyword(const FITSKeyword &keyword);
    void writeMetadata();
    void writeCFA(const Image &image);
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
    int _rows = 0;
    int _cols = 0;
    std::vector<T> _elem;
public:
    Matrix() = default;
    Matrix(int rows, int cols) : _rows(rows), _cols(cols), _elem(rows * cols) {}
    void resize(int rows, int cols) { _rows = rows; _cols = cols; _elem.resize(rows * cols); }
    T& operator()(int row, int col) { return _elem[row * _cols + col]; }

    friend class MatrixConvert;
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

class LIBXISF_EXPORT Error : public std::exception
{
    std::string _msg;
public:
    Error() = default;
    explicit Error(const char *msg) : Error(std::string(msg)) {}
    explicit Error(const std::string &msg) : std::exception(), _msg(msg) {}
    const char* what() const noexcept { return _msg.c_str(); }
};

template<typename T>
Image::SampleFormat Image::sampleFormatEnum()
{
    if(std::is_same<T, LibXISF::UInt8>::value)return Image::UInt8;
    if(std::is_same<T, LibXISF::UInt16>::value)return Image::UInt16;
    if(std::is_same<T, LibXISF::UInt32>::value)return Image::UInt32;
    if(std::is_same<T, LibXISF::UInt64>::value)return Image::UInt64;
    if(std::is_same<T, LibXISF::Float32>::value)return Image::Float32;
    if(std::is_same<T, LibXISF::Float64>::value)return Image::Float64;
    if(std::is_same<T, LibXISF::Complex32>::value)return Image::Complex32;
    if(std::is_same<T, LibXISF::Complex64>::value)return Image::Complex64;
}

}

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
Q_DECLARE_METATYPE(LibXISF::TimePoint);
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
