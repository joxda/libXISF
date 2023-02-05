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

#include "libxisf.h"
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <QXmlStreamReader>
#include <QDateTime>
#include <QtEndian>
#include <QElapsedTimer>
#include <QFile>
#include <QBuffer>
#include "lz4/lz4.h"
#include "lz4/lz4hc.h"

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
template<> struct std::hash<QString>
{
    size_t operator()(const QString &string) const
    {
        return std::hash<std::string>{}(string.toStdString());
    }
};
#endif
namespace LibXISF
{
class MatrixConvert
{
    int _rows = 0;
    int _cols = 0;
    QByteArray _data;
public:
    const QByteArray& data() const
    {
        return _data;
    }
    int rows() const { return _rows; }
    int cols() const { return _cols; }
    MatrixConvert() = default;
    MatrixConvert(int rows, int cols, const QByteArray &data) : _rows(rows), _cols(cols), _data(data) {}

    template<typename T>
    static Matrix<T> toMatrix(const MatrixConvert &m)
    {
        Matrix<T> matrix(m._rows, m._cols);
        std::memcpy(&matrix._elem[0], m._data.constData(), m._data.size());
        return matrix;
    }
    template<typename T>
    static MatrixConvert fromMatrix(const Matrix<T> &m)
    {
        MatrixConvert mc;
        mc._rows = m._rows;
        mc._cols = m._cols;
        mc._data = QByteArray((const char*)&m._elem[0], mc._rows * mc._cols * sizeof(T));
        return mc;
    }
};
}

Q_DECLARE_METATYPE(LibXISF::MatrixConvert);

namespace LibXISF
{

static std::unordered_map<QString, int> typeToId;
static std::unordered_map<int, QString> idToType;
static std::unordered_map<QString, Image::Type> imageTypeToEnum;
static std::unordered_map<Image::Type, QString> imageTypeToString;
static std::unordered_map<QString, Image::SampleFormat> sampleFormatToEnum;
static std::unordered_map<Image::SampleFormat, QString> sampleFormatToString;
static std::unordered_map<QString, Image::ColorSpace> colorSpaceToEnum;
static std::unordered_map<Image::ColorSpace, QString> colorSpaceToString;
static std::unordered_map<int, size_t> vectorTypeSizes;
static std::unordered_map<int, size_t> matrixTypeSizes;

static const std::unordered_map<QString, std::pair<QString, int>> fitsNameToPropertyIdTypeConvert = {
    {"OBSERVER", {"Observer:Name", QMetaType::QString}},
    {"RADECSYS", {"Observation:CelestialReferenceSystem", QMetaType::QString}},
    {"CRVAL1",   {"Observation:Center:Dec", QMetaType::Double}},
    {"CRVAL2",   {"Observation:Center:RA", QMetaType::Double}},
    {"CRPIX1",   {"Observation:Center:X", QMetaType::Double}},
    {"CRPIX2",   {"Observation:Center:Y", QMetaType::Double}},
    {"EQUINOX",  {"Observation:Equinox", QMetaType::Double}},
    {"SITELAT",  {"Observation:Location:Latitude", QMetaType::Double}},
    {"SITELONG", {"Observation:Location:Longitude", QMetaType::Double}},
    {"OBJECT",   {"Observation:Object:Name", QMetaType::QString}},
    {"DEC",      {"Observation:Object:Dec", QMetaType::Double}},
    {"RA",       {"Observation:Object:RA", QMetaType::Double}},
    {"DATE-OBS", {"Observation:Time:Start", QMetaType::QDateTime}},
    {"DATE-END", {"Observation:Time:End", QMetaType::QDateTime}},
    {"GAIN",     {"Instrument:Camera:Gain", QMetaType::Float}},
    {"ISOSPEED", {"Instrument:Camera:ISOSpeed", QMetaType::Int}},
    {"INSTRUME", {"Instrument:Camera:Name", QMetaType::QString}},
    {"ROTATANG", {"Instrument:Camera:Rotation", QMetaType::Float}},
    {"XBINNING", {"Instrument:Camera:XBinning", QMetaType::Int}},
    {"YBINNING", {"Instrument:Camera:YBinning", QMetaType::Int}},
    {"EXPTIME",  {"Instrument:ExposureTime", QMetaType::Float}},
    {"FILTER",   {"Instrument:Filter:Name", QMetaType::QString}},
    {"FOCUSPOS", {"Instrument:Focuser:Position", QMetaType::Float}},
    {"CCD-TEMP", {"Instrument:Sensor:Temperature", QMetaType::Float}},
    {"APTDIA",   {"Instrument:Telescope:Aperture", QMetaType::Float}},
    {"FOCALLEN", {"Instrument:Telescope:FocalLength", QMetaType::Float}},
    {"TELESCOP", {"Instrument:Telescope:Name", QMetaType::QString}},
};

static void byteShuffle(QByteArray &data, int itemSize)
{
    if(itemSize > 1)
    {
        QByteArray &input = data;
        QByteArray output(input.size(), 0);
        int num = input.size() / itemSize;
        char *s = output.data();
        for(int i=0; i<itemSize; i++)
        {
            const char *u = input.constData() + i;
            for(int o=0; o<num; o++, s++, u += itemSize)
                *s = *u;
        }
        memcpy(s, input.constData() + num * itemSize, input.size() % itemSize);
        data = output;
    }
}

static void byteUnshuffle(QByteArray &data, int itemSize)
{
    if(itemSize > 1)
    {
        QByteArray &input = data;
        QByteArray output(input.size(), 0);
        int num = input.size() / itemSize;
        const char *s = input.constData();
        for(int i=0; i<itemSize; i++)
        {
            char *u = output.data() + i;
            for(int o=0; o<num; o++, s++, u += itemSize)
                *u = *s;
        }
        memcpy(output.data() + num * itemSize, s, input.size() % itemSize);
        data = output;
    }
}

bool isString(QMetaType::Type type)
{
    return type == QMetaType::QString;
}

void DataBlock::decompress(const QByteArray &input, const QString &encoding)
{
    QByteArray tmp = input;

    if(encoding == "base64")
        tmp = QByteArray::fromBase64(tmp);
    else if(encoding == "base16")
        tmp = QByteArray::fromHex(tmp);

    switch(codec)
    {
    case None:
        data = tmp;
        break;
    case Zlib:
    {
        uint32_t size;
        qToBigEndian<uint32_t>(uncompressedSize, &size);
        tmp.prepend((char*)&size, sizeof(size));
        data = qUncompress(tmp);
        break;
    }
    case LZ4:
    case LZ4HC:
        data.resize(uncompressedSize);
        if(LZ4_decompress_safe(tmp.constData(), data.data(), tmp.size(), data.size()) < 0)
            throw Error("LZ4 decompression failed");
        break;
    }

    byteUnshuffle(data, byteShuffling);
    attachmentPos = 0;
}

void DataBlock::compress()
{
    QByteArray tmp = data;
    uncompressedSize = data.size();

    byteShuffle(tmp, byteShuffling);

    switch(codec)
    {
    case None:
        data = tmp;
        break;
    case Zlib:
        data = qCompress(tmp);
        data.remove(0, sizeof(uint32_t));
        break;
    case LZ4:
    case LZ4HC:
    {
        int compSize = 0;
        data.resize(LZ4_compressBound(tmp.size()));
        if(codec == LZ4)
            compSize = LZ4_compress_default(tmp.constData(), data.data(), tmp.size(), data.size());
        else
            compSize = LZ4_compress_HC(tmp.constData(), data.data(), tmp.size(), data.size(), LZ4HC_CLEVEL_DEFAULT);

        if(compSize <= 0)
            throw Error("LZ4 compression failed");

        data.resize(compSize);
        break;
    }
    }
}

Property::Property(const QString &_id, const char *_value) :
    id(_id),
    value(_value)
{
}

template<typename T>
void planarToNormal(void *_in, void *_out, size_t channels, size_t size)
{
    T *in = static_cast<T*>(_in);
    T *out = static_cast<T*>(_out);
    for(size_t i=0; i<size; i++)
        for(size_t o=0; o<channels; o++)
            out[i*channels + o] = in[o*size + i];
}

template<typename T>
void normalToPlanar(void *_in, void *_out, size_t channels, size_t size)
{
    T *in = static_cast<T*>(_in);
    T *out = static_cast<T*>(_out);
    for(size_t i=0; i<size; i++)
        for(size_t o=0; o<channels; o++)
            out[o*size + i] = in[i*channels + o];
}

Image::Image(uint64_t width, uint64_t height, uint64_t channelCount, SampleFormat sampleFormat, ColorSpace colorSpace, PixelStorage pixelStorate) :
    _pixelStorage(pixelStorate),
    _sampleFormat(sampleFormat),
    _colorSpace(colorSpace)
{
    setGeometry(width, height, channelCount);
}

uint64_t Image::width() const
{
    return _width;
}

uint64_t Image::height() const
{
    return _height;
}

uint64_t Image::channelCount() const
{
    return _channelCount;
}

void Image::setGeometry(uint64_t width, uint64_t height, uint64_t channelCount)
{
    _width = width;
    _height = height;
    _channelCount = channelCount;
    _dataBlock.data.resize(width * height * channelCount * sampleFormatSize(_sampleFormat));
}

const Bounds &Image::bounds() const
{
    return _bounds;
}

void Image::setBounds(const Bounds &newBounds)
{
    _bounds = newBounds;
}

Image::Type Image::imageType() const
{
    return _imageType;
}

void Image::setImageType(Type newImageType)
{
    _imageType = newImageType;
}

Image::PixelStorage Image::pixelStorage() const
{
    return _pixelStorage;
}

void Image::setPixelStorage(PixelStorage newPixelStorage)
{
    _pixelStorage = newPixelStorage;
}

Image::SampleFormat Image::sampleFormat() const
{
    return _sampleFormat;
}

void Image::setSampleFormat(SampleFormat newSampleFormat)
{
    _sampleFormat = newSampleFormat;
    _dataBlock.data.resize(_width * _height * _channelCount * sampleFormatSize(_sampleFormat));
}

Image::ColorSpace Image::colorSpace() const
{
    return _colorSpace;
}

void Image::setColorSpace(ColorSpace newColorSpace)
{
    _colorSpace = newColorSpace;
}

const ColorFilterArray Image::colorFilterArray() const
{
    return _cfa;
}

void Image::setColorFilterArray(const ColorFilterArray cfa)
{
    _cfa = cfa;
}

const std::vector<Property>& Image::imageProperties() const
{
    return _properties;
}

void Image::addProperty(const Property &property)
{
    if(_propertiesId.count(property.id))
        throw Error("Duplicate property id");

    _propertiesId[property.id] = _properties.size();
    _properties.push_back(property);
}

void Image::updateProperty(const Property &property)
{
    if(!_propertiesId.count(property.id))
        addProperty(property);
    else
        _properties[_propertiesId[property.id]] = property;
}

const std::vector<FITSKeyword> Image::fitsKeywords() const
{
    return _fitsKeywords;
}

void Image::addFITSKeyword(const FITSKeyword &keyword)
{
    _fitsKeywords.push_back(keyword);
}

bool Image::addFITSKeywordAsProperty(const QString &name, const QVariant &value)
{
    if(fitsNameToPropertyIdTypeConvert.count(name))
    {
        auto &c = fitsNameToPropertyIdTypeConvert.at(name);
        Property prop(c.first, value);
        prop.value.convert(c.second);
        updateProperty(prop);
        return true;
    }
    return false;
}

void *Image::imageData()
{
    return _dataBlock.data.data();
}

const void *Image::imageData() const
{
    return _dataBlock.data.data();
}

size_t Image::imageDataSize() const
{
    return _dataBlock.data.size();
}

DataBlock::CompressionCodec Image::compression() const
{
    return _dataBlock.codec;
}

void Image::setCompression(DataBlock::CompressionCodec compression, int level)
{
    _dataBlock.codec = compression;
    _dataBlock.compressLevel = level;
}

bool Image::byteShuffling() const
{
    return _dataBlock.byteShuffling;
}

void Image::setByteshuffling(bool enable)
{
    _dataBlock.byteShuffling = enable ? sampleFormatSize(_sampleFormat) : 0;
}

void Image::convertPixelStorageTo(PixelStorage storage)
{
    if(_pixelStorage == storage || _channelCount <= 1)
    {
        _pixelStorage = storage;
        return;
    }

    QByteArray tmp;
    tmp.resize(_dataBlock.data.size());
    size_t size = _width*_height;

    switch(_sampleFormat)
    {
    case UInt8:
        if(storage == Normal)
            planarToNormal<uint8_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        else
            normalToPlanar<uint8_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        break;
    case UInt16:
        if(storage == Normal)
            planarToNormal<uint16_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        else
            normalToPlanar<uint16_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        break;
    case UInt32:
    case Float32:
        if(storage == Normal)
            planarToNormal<uint32_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        else
            normalToPlanar<uint32_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        break;
    case UInt64:
    case Float64:
        if(storage == Normal)
            planarToNormal<uint64_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        else
            normalToPlanar<uint64_t>(_dataBlock.data.data(), tmp.data(), _channelCount, size);
        break;
    default:
        break;
    }
    _dataBlock.data = tmp;
    _pixelStorage = storage;
}

Image::Type Image::imageTypeEnum(const QString &type)
{
    auto t = imageTypeToEnum.find(type);
    return t != imageTypeToEnum.end() ? t->second : Image::Light;
}

QString Image::imageTypeString(Type type)
{
    auto t = imageTypeToString.find(type);
    return t != imageTypeToString.end() ? t->second : "Light";
}

Image::PixelStorage Image::pixelStorageEnum(const QString &storage)
{
    if(storage == "Normal")return Image::Normal;
    return Image::Planar;
}

QString Image::pixelStorageString(PixelStorage storage)
{
    if(storage == Normal)return "Normal";
    return "Planar";
}

Image::SampleFormat Image::sampleFormatEnum(const QString &format)
{
    auto t = sampleFormatToEnum.find(format);
    return t != sampleFormatToEnum.end() ? t->second : Image::UInt16;
}

QString Image::sampleFormatString(SampleFormat format)
{
    auto t = sampleFormatToString.find(format);
    return t != sampleFormatToString.end() ? t->second : "UInt16";
}

Image::ColorSpace Image::colorSpaceEnum(const QString &colorSpace)
{
    auto t = colorSpaceToEnum.find(colorSpace);
    return t != colorSpaceToEnum.end() ? t->second : Image::Gray;
}

QString Image::colorSpaceString(ColorSpace colorSpace)
{
    auto t = colorSpaceToString.find(colorSpace);
    return t != colorSpaceToString.end() ? t->second : "Gray";
}

size_t Image::sampleFormatSize(SampleFormat sampleFormat)
{
    switch(sampleFormat)
    {
        case Image::UInt8:   return sizeof(LibXISF::UInt8);
        case Image::UInt16:  return sizeof(LibXISF::UInt16);
        case Image::UInt32:  return sizeof(LibXISF::UInt32);
        case Image::UInt64:  return sizeof(LibXISF::UInt64);
        case Image::Float32: return sizeof(LibXISF::Float32);
        case Image::Float64: return sizeof(LibXISF::Float64);
        case Image::Complex32: return sizeof(LibXISF::Complex32);
        case Image::Complex64: return sizeof(LibXISF::Complex64);
    }
    return sizeof(UInt16);
}

XISFReader::XISFReader()
{
    _xml = std::make_unique<QXmlStreamReader>();
}

void XISFReader::open(const QString &name)
{
    QFile *fr = new QFile(name);
    open(fr);
}

void XISFReader::open(const QByteArray &data)
{
    QBuffer *buffer = new QBuffer();
    buffer->setData(data);
    open(buffer);
}

void XISFReader::open(QIODevice *io)
{
    close();
    _io.reset(io);
    if(!_io->open(QIODevice::ReadOnly))
        throw Error("Failed to open file");

    readSignature();
    readXISFHeader();
}

void XISFReader::close()
{
    _xml->clear();
    _io.reset();
    _images.clear();
    _properties.clear();
}

int XISFReader::imagesCount() const
{
    return _images.size();
}

const Image& XISFReader::getImage(uint32_t n)
{
    if(n >= _images.size())
        throw Error("Out of bounds");

    Image &img = _images[n];
    if(img._dataBlock.attachmentPos)
    {
        _io->seek(img._dataBlock.attachmentPos);
        img._dataBlock.decompress(_io->read(img._dataBlock.attachmentSize));
    }
    return img;
}

void XISFReader::readXISFHeader()
{
    uint32_t headerLen[2] = {0};
    _io->read((char*)&headerLen, sizeof(headerLen));

    QByteArray xisfHeader = _io->read(headerLen[0]);
    _xml->addData(xisfHeader);

    _xml->readNextStartElement();

    if(_xml->name() == "xisf" && _xml->attributes().value("version") == "1.0")
    {
        while(!_xml->atEnd())
        {
            if(!_xml->readNextStartElement())
                break;

            if(_xml->name() == "Image")
                readImageElement();
            else if(_xml->name() == "Property")
                _properties.push_back(readPropertyElement());
        }
    }
    else throw Error("Unknown root XML element");

    if(_xml->hasError())
        throw Error(_xml->errorString().toStdString());
}

void XISFReader::readSignature()
{
    char signature[8];
    if(_io->read(signature, sizeof(signature)) != sizeof(signature))
        throw Error("Failed to read from file");

    if(memcmp(signature, "XISF0100", sizeof(signature)) != 0)
        throw Error("Not valid XISF 1.0 file");
}

void XISFReader::readImageElement()
{
    QXmlStreamAttributes attributes = _xml->attributes();

    Image image;

    QVector<QStringRef> geometry = attributes.value("geometry").split(":");
    if(geometry.size() != 3)throw Error("We support only 2D images");
    image._width = geometry[0].toULongLong();
    image._height = geometry[1].toULongLong();
    image._channelCount = geometry[2].toULongLong();
    if(!image._width || !image._height || !image._channelCount)throw Error("Invalid image geometry");

    QVector<QStringRef> bounds = attributes.value("bounds").split(":");
    if(bounds.size() == 2)
    {
        image._bounds.first = bounds[0].toDouble();
        image._bounds.second = bounds[1].toDouble();
    }
    image._imageType = Image::imageTypeEnum(attributes.value("imageType").toString());
    image._pixelStorage = Image::pixelStorageEnum(attributes.value("pixelStorage").toString());
    image._sampleFormat = Image::sampleFormatEnum(attributes.value("sampleFormat").toString());
    image._colorSpace = Image::colorSpaceEnum(attributes.value("colorSpace").toString());

    image._dataBlock = readDataBlock();

    while(_xml->readNext() != QXmlStreamReader::EndElement || _xml->name() != "Image")
    {
        if(_xml->tokenType() == QXmlStreamReader::StartElement)
        {
            if(_xml->name() == "Property")
                image.addProperty(readPropertyElement());
            else if(_xml->name() == "FITSKeyword")
                image._fitsKeywords.push_back(readFITSKeyword());
            else if(_xml->name() == "ColorFilterArray")
                image._cfa = readCFA();
            else if(_xml->name() == "ICCProfile")
            {
                DataBlock icc = readDataBlock();
                image._iccProfile = icc.data;
            }
            else
                _xml->skipCurrentElement();
        }
        if(_xml->hasError())
            throw Error("Error while reading XISF header");
    }

    _images.push_back(std::move(image));
}

Property XISFReader::readPropertyElement()
{
    QXmlStreamAttributes attributes = _xml->attributes();
    Property property;
    property.id = attributes.value("id").toString();
    property.comment = attributes.value("comment").toString();

    QString type = attributes.value("type").toString();
    if(typeToId.count(type) == 0)
        throw Error("Invalid type in property");

    int typeId = typeToId[type];
    QVariant value;

    if(type == "String" && !attributes.hasAttribute("location"))
    {
        property.value = _xml->readElementText();
    }
    else if(attributes.hasAttribute("value"))
    {
        value = attributes.value("value").toString();
        value.convert(typeId);
        property.value = value;
    }
    else
    {
        DataBlock dataBlock = readDataBlock();
        if(dataBlock.attachmentPos)
        {
            _io->seek(dataBlock.attachmentPos);
            dataBlock.decompress(_io->read(dataBlock.attachmentSize));
        }
        if(vectorTypeSizes.count(typeId))
        {
            property.value = dataBlock.data;
            if(!property.value.convert(typeId))
                throw Error("Failed to convert vector property data");
        }
        else if(matrixTypeSizes.count(typeId))
        {
            bool ok1, ok2;
            int rows = attributes.value("rows").toInt(&ok1);
            int cols = attributes.value("columns").toInt(&ok2);
            if(!ok1 || !ok2)
                throw Error("Invalid rows and/or columns");

            property.value = QVariant::fromValue<MatrixConvert>(MatrixConvert(rows, cols, dataBlock.data));
            if(!property.value.convert(typeId))
                throw Error("Failed to convert matrix property data");
        }
        else
        {
            property.value = dataBlock.data;
        }
    }

    return property;
}

FITSKeyword XISFReader::readFITSKeyword()
{
    QXmlStreamAttributes attributes = _xml->attributes();
    if(attributes.hasAttribute("name") && attributes.hasAttribute("value") && attributes.hasAttribute("comment"))
        return { attributes.value("name").toString(), attributes.value("value").toString(), attributes.value("comment").toString() };
    else
        throw Error("Invalid FITSKeyword element");
}

void XISFReader::readDataElement(DataBlock &dataBlock)
{
    _xml->readNextStartElement();
    if(_xml->name() == "Data")
    {
        readCompression(dataBlock);
        QString encoding = _xml->attributes().value("encoding").toString();
        QByteArray text = _xml->readElementText().toUtf8();
        dataBlock.decompress(text, encoding);
    }
    else
        throw Error("Unexpected XML element");
}

DataBlock XISFReader::readDataBlock()
{
    DataBlock dataBlock;
    QXmlStreamAttributes attributes = _xml->attributes();
    QVector<QStringRef> location = attributes.value("location").split(":");

    readCompression(dataBlock);

    if(location.size() && location[0] == "embedded")
    {
        dataBlock.embedded = true;
    }
    else if(location.size() >= 2 && location[0] == "inline")
    {
        QByteArray text = _xml->readElementText().toUtf8();
        dataBlock.decompress(text, location[1].toString());
    }
    else if(location.size() >= 3 && location[0] == "attachment")
    {
        bool ok1, ok2;
        dataBlock.attachmentPos = location[1].toULongLong(&ok1);
        dataBlock.attachmentSize = location[2].toULongLong(&ok2);
        if(!ok1 || !ok2)throw Error("Invalid attachment");
    }
    else
    {
        throw Error("Invalid data block");
    }

    if(dataBlock.embedded)
        readDataElement(dataBlock);

    return dataBlock;
}

void XISFReader::readCompression(DataBlock &dataBlock)
{
    QVector<QStringRef> compression = _xml->attributes().value("compression").split(":");
    if(compression.size() >= 2)
    {
        if(compression[0].startsWith("zlib"))
            dataBlock.codec = DataBlock::Zlib;
        else if(compression[0].startsWith("lz4hc"))
            dataBlock.codec = DataBlock::LZ4HC;
        else if(compression[0].startsWith("lz4"))
            dataBlock.codec = DataBlock::LZ4;
        else
            throw Error("Unknown compression codec");

        dataBlock.uncompressedSize = compression[1].toULongLong();

        if(compression[0].endsWith("+sh"))
        {
            if(compression.size() == 3)
                dataBlock.byteShuffling = compression[2].toInt();
            else
                throw Error("Missing byte shuffling size");
        }
    }
}

ColorFilterArray XISFReader::readCFA()
{
    ColorFilterArray cfa;
    QXmlStreamAttributes attributes = _xml->attributes();
    if(attributes.hasAttribute("pattern") && attributes.hasAttribute("width") && attributes.hasAttribute("height"))
    {
        cfa.pattern = attributes.value("pattern").toString();
        cfa.width = attributes.value("width").toInt();
        cfa.height = attributes.value("height").toInt();
    }
    else
    {
        throw Error("ColorFilterArray element missing one of mandatory attributes");
    }
    return cfa;
}

XISFWriter::XISFWriter()
{
    _xml = std::make_unique<QXmlStreamWriter>();
    //_xml->setAutoFormatting(true);
}

void XISFWriter::save(const QString &name)
{
    QFile fw(name);

    if(!fw.open(QIODevice::WriteOnly))
        throw Error("Failed to open file");

    save(fw);
}

void XISFWriter::save(QByteArray &data)
{
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    save(buffer);
}

void XISFWriter::save(QIODevice &io)
{
    writeHeader();

    io.write(_xisfHeader);

    for(auto &image : _images)
    {
        io.write(image._dataBlock.data);
    }
}

void XISFWriter::writeImage(const Image &image)
{
    _images.push_back(image);
    _images.back()._dataBlock.attachmentPos = 1;
    _images.back()._dataBlock.compress();
}

void XISFWriter::writeHeader()
{
    const char signature[16] = {'X', 'I', 'S', 'F', '0', '1', '0', '0', 0, 0, 0, 0, 0, 0, 0, 0};
    QBuffer buffer(&_xisfHeader);
    buffer.open(QIODevice::WriteOnly);
    buffer.write(signature, sizeof(signature));

    _xml->setDevice(&buffer);

    _xml->writeStartDocument();
    _xml->writeComment("\nExtensible Image Serialization Format - XISF version 1.0\nCreated with libXISF - https://nouspiro.space\n");
    _xml->writeStartElement("xisf");
    _xml->writeAttribute("version", "1.0");
    _xml->writeDefaultNamespace("http://www.pixinsight.com/xisf");
    _xml->writeNamespace("http://www.w3.org/2001/XMLSchema-instance", "xsi");
    _xml->writeAttribute("http://www.w3.org/2001/XMLSchema-instance", "schemaLocation", "http://www.pixinsight.com/xisf http://pixinsight.com/xisf/xisf-1.0.xsd");

    for(Image &image : _images)
    {
        writeImageElement(image);
    }

    writeMetadata();

    _xml->writeEndElement();
    _xml->writeEndDocument();

    uint32_t size = _xisfHeader.size();

    uint32_t offset = 0;
    const char replace[] = "attachment:2147483648";
    for(auto &image : _images)
    {
        QByteArray blockPos = QByteArray("attachment:") + QByteArray::number(size + offset);
        _xisfHeader.replace(_xisfHeader.indexOf(replace), sizeof(replace) - 1, blockPos);
        offset += image._dataBlock.data.size();
    }

    uint32_t headerSize = _xisfHeader.size() - sizeof(signature);
    _xisfHeader.append(size - _xisfHeader.size(), '\0');

    buffer.seek(8);
    buffer.write((char*)&headerSize, sizeof(size));

    if(_xml->hasError())
        throw Error("Failed to write XML header");
}

void XISFWriter::writeImageElement(const Image &image)
{
    _xml->writeStartElement("Image");
    _xml->writeAttribute("geometry", QString("%1:%2:%3").arg(image._width).arg(image._height).arg(image._channelCount));
    _xml->writeAttribute("sampleFormat", Image::sampleFormatString(image._sampleFormat));
    _xml->writeAttribute("colorSpace", Image::colorSpaceString(image._colorSpace));
    _xml->writeAttribute("imageType", Image::imageTypeString(image._imageType));
    if((image._sampleFormat == Image::Float32 || image._sampleFormat == Image::Float64) ||
            image._bounds.first != 0.0 || image._bounds.second != 1.0)
    {
        _xml->writeAttribute("bounds", QString("%1:%2").arg(image._bounds.first).arg(image._bounds.second));
    }

    writeDataBlockAttributes(image._dataBlock);
    for(auto &property : image._properties)
        writePropertyElement(property);

    for(auto &fitsKeyword : image._fitsKeywords)
        writeFITSKeyword(fitsKeyword);

    writeCFA(image);

    _xml->writeEndElement();
}

void XISFWriter::writeDataBlockAttributes(const DataBlock &dataBlock)
{
    writeCompressionAttributes(dataBlock);

    if(dataBlock.embedded)
    {
        _xml->writeAttribute("location", "embedded");
    }
    else if(dataBlock.attachmentPos == 0)
    {
        _xml->writeAttribute("location", QString("inline:base64"));
    }
    else
    {
        _xml->writeAttribute("location", QString("attachment:2147483648:%1").arg(dataBlock.data.size()));
    }
}

void XISFWriter::writeCompressionAttributes(const DataBlock &dataBlock)
{
    QString codec;

    if(dataBlock.codec == DataBlock::Zlib)
        codec = "zlib";
    else if(dataBlock.codec == DataBlock::LZ4)
        codec = "lz4";
    else if(dataBlock.codec == DataBlock::LZ4HC)
        codec = "lz4hc";

    if(dataBlock.byteShuffling > 1)
        codec += "+sh";

    if(!codec.isEmpty())
    {
        codec += QString(":%1").arg(dataBlock.uncompressedSize);
        if(dataBlock.byteShuffling > 1)
            codec += QString(":%1").arg(dataBlock.byteShuffling);

        _xml->writeAttribute("compression", codec);
    }
}

void XISFWriter::writePropertyElement(const Property &property)
{
    int type = property.value.userType();

    _xml->writeStartElement("Property");
    _xml->writeAttribute("id", property.id);
    _xml->writeAttribute("type", idToType[type]);

    if(!property.comment.isEmpty())
        _xml->writeAttribute("comment", property.comment);

    if(type == QMetaType::QString)
        _xml->writeCharacters(property.value.toString());
    else if(type == QMetaType::Bool)
        _xml->writeAttribute("value", property.value.toBool() ? "1" : "0");
    else if(type == QMetaType::SChar || type == QMetaType::UChar)
        _xml->writeAttribute("value", QString::number(property.value.toInt()));
    else if(vectorTypeSizes.count(type))
    {
        DataBlock dataBlock;
        dataBlock.data = property.value.toByteArray();
        writeDataBlockAttributes(dataBlock);
        _xml->writeAttribute("length", QString::number(dataBlock.data.size() / vectorTypeSizes[type]));
        _xml->writeCharacters(dataBlock.data.toBase64());
    }
    else if(matrixTypeSizes.count(type))
    {
        MatrixConvert mc = property.value.value<MatrixConvert>();
        DataBlock dataBlock;
        dataBlock.data = mc.data();
        writeDataBlockAttributes(dataBlock);
        _xml->writeAttribute("rows", QString::number(mc.rows()));
        _xml->writeAttribute("columns", QString::number(mc.cols()));
        _xml->writeCharacters(dataBlock.data.toBase64());
    }
    else
        _xml->writeAttribute("value", property.value.toString());
    _xml->writeEndElement();
    if(_xml->hasError())
        throw Error("Failed to write property");
}

void XISFWriter::writeFITSKeyword(const FITSKeyword &keyword)
{
    _xml->writeEmptyElement("FITSKeyword");
    _xml->writeAttribute("name", keyword.name);
    _xml->writeAttribute("value", keyword.value);
    _xml->writeAttribute("comment", keyword.comment);
    if(_xml->hasError())
        throw Error("Failed to write FITS keyword");
}

void XISFWriter::writeMetadata()
{
    _xml->writeStartElement("Metadata");
    writePropertyElement(Property("XISF:CreationTime", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)));
    writePropertyElement(Property("XISF:CreatorApplication", "LibXISF"));
    _xml->writeEndElement();
}

void XISFWriter::writeCFA(const Image &image)
{
    if(image._cfa.width && image._cfa.height)
    {
        _xml->writeEmptyElement("ColorFilterArray");
        _xml->writeAttribute("pattern", image._cfa.pattern);
        _xml->writeAttribute("width", QString::number(image._cfa.width));
        _xml->writeAttribute("height", QString::number(image._cfa.height));
    }
}

#define REGISTER_METATYPE(type) { int id = qMetaTypeId<type>(); \
    typeToId.insert({#type, id}); idToType.insert({id, #type}); }

#define STRING_ENUM(map, map2, c, e) { map.insert({#e, c::e}); map2.insert({c::e, #e}); }

#define REGISTER_VECTOR_TYPE(type) { vectorTypeSizes.insert({typeToId[#type], sizeof(type::value_type)});   \
    QMetaType::registerConverter<type, QByteArray>([](const type &v) {                                      \
        return QByteArray((const char*)&v[0], v.size() * sizeof(type::value_type));                         \
    });                                                                                                     \
    QMetaType::registerConverter<QByteArray, type>([](const QByteArray &v) {                                \
        type t(v.size() / sizeof(type::value_type));  std::memcpy(&t[0], v.constData(), v.size());          \
        return t;                                                                                           \
    }); }

#define REGISTER_MATRIX_TYPE(type, mtype) { matrixTypeSizes.insert({typeToId[#type], sizeof(mtype)});       \
    QMetaType::registerConverter<type, MatrixConvert>(MatrixConvert::fromMatrix<mtype>);                    \
    QMetaType::registerConverter<MatrixConvert, type>(MatrixConvert::toMatrix<mtype>);}

struct Init
{
    Init()
    {
        REGISTER_METATYPE(Boolean);
        REGISTER_METATYPE(Int8);
        REGISTER_METATYPE(UInt8);
        REGISTER_METATYPE(Int16);
        REGISTER_METATYPE(UInt16);
        REGISTER_METATYPE(Int32);
        REGISTER_METATYPE(UInt32);
        REGISTER_METATYPE(Int64);
        REGISTER_METATYPE(UInt64);
        REGISTER_METATYPE(Float32);
        REGISTER_METATYPE(Float64);
        REGISTER_METATYPE(Complex32);
        REGISTER_METATYPE(Complex64);
        REGISTER_METATYPE(TimePoint);
        REGISTER_METATYPE(I8Vector);
        REGISTER_METATYPE(UI8Vector);
        REGISTER_METATYPE(I16Vector);
        REGISTER_METATYPE(UI16Vector);
        REGISTER_METATYPE(I32Vector);
        REGISTER_METATYPE(UI32Vector);
        REGISTER_METATYPE(I64Vector);
        REGISTER_METATYPE(UI64Vector);
        REGISTER_METATYPE(F32Vector);
        REGISTER_METATYPE(F64Vector);
        REGISTER_METATYPE(C32Vector);
        REGISTER_METATYPE(C64Vector);
        REGISTER_METATYPE(I8Matrix);
        REGISTER_METATYPE(UI8Matrix);
        REGISTER_METATYPE(I16Matrix);
        REGISTER_METATYPE(UI16Matrix);
        REGISTER_METATYPE(I32Matrix);
        REGISTER_METATYPE(UI32Matrix);
        REGISTER_METATYPE(I64Matrix);
        REGISTER_METATYPE(UI64Matrix);
        REGISTER_METATYPE(F32Matrix);
        REGISTER_METATYPE(F64Matrix);
        REGISTER_METATYPE(C32Matrix);
        REGISTER_METATYPE(C64Matrix);
        REGISTER_METATYPE(String);

        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, Bias);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, Dark);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, Flat);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, Light);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, MasterBias);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, MasterDark);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, MasterFlat);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, DefectMap);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, RejectionMapHigh);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, RejectionMapLow);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, BinaryRejectionMapHigh);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, BinaryRejectionMapLow);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, SlopeMap);
        STRING_ENUM(imageTypeToEnum, imageTypeToString, Image, WeightMap);

        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, UInt8);
        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, UInt16);
        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, UInt32);
        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, UInt64);
        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, Float32);
        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, Float64);
        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, Complex32);
        STRING_ENUM(sampleFormatToEnum, sampleFormatToString, Image, Complex64);

        STRING_ENUM(colorSpaceToEnum, colorSpaceToString, Image, Gray);
        STRING_ENUM(colorSpaceToEnum, colorSpaceToString, Image, RGB);
        STRING_ENUM(colorSpaceToEnum, colorSpaceToString, Image, CIELab);

        REGISTER_VECTOR_TYPE(I8Vector);
        REGISTER_VECTOR_TYPE(UI8Vector);
        REGISTER_VECTOR_TYPE(I16Vector);
        REGISTER_VECTOR_TYPE(UI16Vector);
        REGISTER_VECTOR_TYPE(I32Vector);
        REGISTER_VECTOR_TYPE(UI32Vector);
        REGISTER_VECTOR_TYPE(I64Vector);
        REGISTER_VECTOR_TYPE(UI64Vector);
        REGISTER_VECTOR_TYPE(F32Vector);
        REGISTER_VECTOR_TYPE(F64Vector);
        REGISTER_VECTOR_TYPE(C32Vector);
        REGISTER_VECTOR_TYPE(C64Vector);

        REGISTER_MATRIX_TYPE(I8Matrix, Int8);
        REGISTER_MATRIX_TYPE(UI8Matrix, UInt8);
        REGISTER_MATRIX_TYPE(I16Matrix, Int16);
        REGISTER_MATRIX_TYPE(UI16Matrix, UInt16);
        REGISTER_MATRIX_TYPE(I32Matrix, Int32);
        REGISTER_MATRIX_TYPE(UI32Matrix, UInt32);
        REGISTER_MATRIX_TYPE(I64Matrix, Int64);
        REGISTER_MATRIX_TYPE(UI64Matrix, UInt64);
        REGISTER_MATRIX_TYPE(F32Matrix, Float32);
        REGISTER_MATRIX_TYPE(F64Matrix, Float64);
        REGISTER_MATRIX_TYPE(C32Matrix, Complex32);
        REGISTER_MATRIX_TYPE(C64Matrix, Complex64);

        QMetaType::registerConverter<Complex32, QString>([](const Complex32 &c){ return QString("(%1,%2)").arg(c.real).arg(c.imag); });
        QMetaType::registerConverter<Complex64, QString>([](const Complex64 &c){ return QString("(%1,%2)").arg(c.real).arg(c.imag); });
        QMetaType::registerConverter<QString, Complex32>([](QString s)
        {
            Complex32 c;
            s.remove('(');
            s.remove(')');
            int comma = s.indexOf(',');
            c.real = s.leftRef(comma).toFloat();
            c.imag = s.rightRef(comma+1).toFloat();
            return c;
        });
        QMetaType::registerConverter<QString, Complex64>([](QString s)
        {
            Complex64 c;
            s.remove('(');
            s.remove(')');
            int comma = s.indexOf(',');
            c.real = s.leftRef(comma).toDouble();
            c.imag = s.rightRef(comma+1).toDouble();
            return c;
        });
    }
};

static Init init;

}
