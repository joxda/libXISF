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
#include <QXmlStreamReader>
#include <QDateTime>
#include <QtEndian>
#include <QElapsedTimer>
#include <QFile>
#include <QBuffer>
#include <QDebug>
#include "lz4/lz4.h"
#include "lz4/lz4hc.h"

#define STRING_ENUM(e) {#e, e}

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

static std::unordered_map<const char*, int> typeToId;
static std::unordered_map<int, const char*> idToType;

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

QString sampleFormatToString(Image::SampleFormat format)
{
    static QStringList sampleFormats = {"UInt8", "UInt16", "UInt32", "UInt64", "Float32", "Float64", "Complex32", "Complex64"};
    return sampleFormats[format];
}

QString colorSpaceToString(Image::ColorSpace colorSpace)
{
    static QStringList colorSpaces = {"Gray", "RGB", "CIELab"};
    return colorSpaces[colorSpace];
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
            throw std::runtime_error("LZ4 decompression failed");
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
            throw std::runtime_error("LZ4 compression failed");

        data.resize(compSize);
        break;
    }
    }
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

void Image::convertPixelStorageTo(PixelStorage storage)
{
    if(pixelStorage == storage)
        return;

    QByteArray tmp;
    tmp.resize(dataBlock.data.size());
    size_t size = width*height;

    switch(sampleFormat)
    {
    case UInt8:
        if(storage == Normal)
            planarToNormal<uint8_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        else
            normalToPlanar<uint8_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        break;
    case UInt16:
        if(storage == Normal)
            planarToNormal<uint16_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        else
            normalToPlanar<uint16_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        break;
    case UInt32:
    case Float32:
        if(storage == Normal)
            planarToNormal<uint32_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        else
            normalToPlanar<uint32_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        break;
    case UInt64:
    case Float64:
        if(storage == Normal)
            planarToNormal<uint64_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        else
            normalToPlanar<uint64_t>(dataBlock.data.data(), tmp.data(), channelCount, size);
        break;
    default:
        break;
    }
    dataBlock.data = tmp;
    pixelStorage = storage;
}

Image::Type Image::imageTypeEnum(const QString &type)
{
    static const std::unordered_map<QString, Image::Type> imageTypeMap = {STRING_ENUM(Bias),
                                                                          STRING_ENUM(Dark),
                                                                          STRING_ENUM(Flat),
                                                                          STRING_ENUM(Light),
                                                                          STRING_ENUM(MasterBias),
                                                                          STRING_ENUM(MasterDark),
                                                                          STRING_ENUM(MasterFlat),
                                                                          STRING_ENUM(DefectMap),
                                                                          STRING_ENUM(RejectionMapHigh),
                                                                          STRING_ENUM(RejectionMapLow),
                                                                          STRING_ENUM(BinaryRejectionMapHigh),
                                                                          STRING_ENUM(BinaryRejectionMapLow),
                                                                          STRING_ENUM(SlopeMap),
                                                                          STRING_ENUM(WeightMap});
    auto t = imageTypeMap.find(type);
    return t != imageTypeMap.end() ? t->second : Image::Light;
}

Image::PixelStorage Image::pixelStorageEnum(const QString &storage)
{
    if(storage == "Normal")return Image::Normal;
    return Image::Planar;
}

Image::SampleFormat Image::sampleFormatEnum(const QString &format)
{
    static const std::unordered_map<QString, SampleFormat> sampleFormatMap = {STRING_ENUM(UInt8),
                                                                              STRING_ENUM(UInt16),
                                                                              STRING_ENUM(UInt32),
                                                                              STRING_ENUM(UInt64),
                                                                              STRING_ENUM(Float32),
                                                                              STRING_ENUM(Float64),
                                                                              STRING_ENUM(Complex32),
                                                                              STRING_ENUM(Complex64)};
    auto t = sampleFormatMap.find(format);
    return t != sampleFormatMap.end() ? t->second : Image::UInt16;
}

Image::ColorSpace Image::colorSpaceEnum(const QString &colorSpace)
{
    static const std::unordered_map<QString, ColorSpace> colorSpaceMap = { STRING_ENUM(Gray), STRING_ENUM(RGB), STRING_ENUM(CIELab) };
    auto t = colorSpaceMap.find(colorSpace);
    return t != colorSpaceMap.end() ? t->second : Image::Gray;
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
        throw std::runtime_error("Failed to open file");

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
        throw std::runtime_error("Out of bounds");

    Image &img = _images[n];
    if(img.dataBlock.attachmentPos)
    {
        _io->seek(img.dataBlock.attachmentPos);
        img.dataBlock.decompress(_io->read(img.dataBlock.attachmentSize));
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
    else throw std::runtime_error("Unknown root XML element");

    if(_xml->hasError())
        throw std::runtime_error(_xml->errorString().toStdString());
}

void XISFReader::readSignature()
{
    char signature[8];
    if(_io->read(signature, sizeof(signature)) != sizeof(signature))
        throw std::runtime_error("Failed to read from file");

    if(memcmp(signature, "XISF0100", sizeof(signature)) != 0)
        throw std::runtime_error("Not valid XISF 1.0 file");
}

void XISFReader::readImageElement()
{
    QXmlStreamAttributes attributes = _xml->attributes();

    Image image;

    QVector<QStringRef> geometry = attributes.value("geometry").split(":");
    if(geometry.size() != 3)throw std::runtime_error("We support only 2D images");
    image.width = geometry[0].toULongLong();
    image.height = geometry[1].toULongLong();
    image.channelCount = geometry[2].toULongLong();
    if(!image.width || !image.height || !image.channelCount)throw std::runtime_error("Invalid image geometry");

    QVector<QStringRef> bounds = attributes.value("bounds").split(":");
    if(bounds.size() == 2)
    {
        image.bounds[0] = bounds[0].toDouble();
        image.bounds[1] = bounds[1].toDouble();
    }
    image.imageType = Image::imageTypeEnum(attributes.value("imageType").toString());
    image.pixelStorage = Image::pixelStorageEnum(attributes.value("pixelStorage").toString());
    image.sampleFormat = Image::sampleFormatEnum(attributes.value("sampleFormat").toString());
    image.colorSpace = Image::colorSpaceEnum(attributes.value("colorSpace").toString());

    image.dataBlock = readDataBlock();

    while(_xml->readNext() != QXmlStreamReader::EndElement || _xml->name() != "Image")
    {
        if(_xml->tokenType() == QXmlStreamReader::StartElement)
        {
            if(_xml->name() == "Property")
                image.properties.push_back(readPropertyElement());
            else if(image.dataBlock.embedded && _xml->name() == "Data")
                readDataElement(image.dataBlock);
            else if(_xml->name() == "ICCProfile")
            {
                DataBlock icc = readDataBlock();
                image.iccProfile = icc.data;
            }
            else
                _xml->skipCurrentElement();
        }
    }

    _images.push_back(std::move(image));
}

Property XISFReader::readPropertyElement()
{
    QXmlStreamAttributes attributes = _xml->attributes();
    Property property;
    property.id = attributes.value("id").toString();
    property.format = attributes.value("format").toString();
    property.comment = attributes.value("comment").toString();

    QStringRef type = attributes.value("type");
    QStringRef value = attributes.value("value");
    if(type == "Int8")
        property.value.setValue((Int8)value.toInt());
    else if(type == "Int16")
        property.value.setValue((Int16)value.toInt());
    else if(type == "Int32")
        property.value.setValue((Int32)value.toInt());
    else if(type == "Int64")
        property.value.setValue((Int64)value.toLongLong());
    else if(type == "UInt8")
        property.value.setValue((Int8)value.toInt());
    else if(type == "UInt16")
        property.value.setValue((Int16)value.toInt());
    else if(type == "UInt32")
        property.value.setValue<UInt32>(value.toUInt());
    else if(type == "UInt64")
        property.value.setValue<UInt64>(value.toULongLong());
    else if(type == "Float32")
        property.value = value.toFloat();
    else if(type == "Float64")
        property.value = value.toDouble();
    else if(type == "TimePoint")
        property.value = QDateTime::fromString(value.toString(), Qt::ISODate);
    else if(type == "String")
    {
        if(attributes.hasAttribute("location"))
        {
            DataBlock dataBlock = readDataBlock();
            if(dataBlock.embedded)
                readDataElement(dataBlock);
            property.value = QString::fromUtf8(dataBlock.data);
        }
        else
            property.value = _xml->readElementText();
    }
    else
        property.value = value.toString();

    qDebug() << property.id << type << property.value.typeName() << property.value;

    return property;
}

void XISFReader::readDataElement(DataBlock &dataBlock)
{
    readCompression(dataBlock);
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
        if(!ok1 || !ok2)throw std::runtime_error("Invalid attachment");
    }
    else
    {
        throw std::runtime_error("Invalid data block");
    }

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
            throw std::runtime_error("Unknown compression codec");

        dataBlock.uncompressedSize = compression[1].toULongLong();

        if(compression[0].endsWith("+sh"))
        {
            if(compression.size() == 3)
                dataBlock.byteShuffling = compression[2].toInt();
            else
                throw std::runtime_error("Missing byte shuffling size");
        }
    }
}

XISFWriter::XISFWriter()
{
    _xml = std::make_unique<QXmlStreamWriter>();
}

void XISFWriter::save(const QString &name)
{
    QFile fw(name);

    if(!fw.open(QIODevice::WriteOnly))
        throw std::runtime_error("Failed to open file");

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
        io.write(image.dataBlock.data);
    }
}

void XISFWriter::writeImage(const Image &image)
{
    _images.push_back(image);
    _images.back().dataBlock.attachmentPos = 1;
    _images.back().dataBlock.compress();
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

    uint32_t size = _xisfHeader.size();
    QByteArray blockPos = QByteArray::number(size);
    _xisfHeader.replace("#########", blockPos);
    uint32_t headerSize = _xisfHeader.size() - sizeof(signature);
    _xisfHeader.append(size - _xisfHeader.size(), '\0');


    buffer.seek(8);
    buffer.write((char*)&headerSize, sizeof(size));

    _xml->writeEndDocument();

    if(_xml->hasError())
        throw std::runtime_error("Failed to write XML header");
}

void XISFWriter::writeImageElement(const Image &image)
{
    _xml->writeStartElement("Image");
    _xml->writeAttribute("geometry", QString("%1:%2:%3").arg(image.width).arg(image.height).arg(image.channelCount));
    _xml->writeAttribute("sampleFormat", sampleFormatToString(image.sampleFormat));
    _xml->writeAttribute("colorSpace", colorSpaceToString(image.colorSpace));
    writeDataBlockAttributes(image.dataBlock);
    for(auto &property : image.properties)
        writePropertyElement(property);

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
        _xml->writeAttribute("location", QString("attachment:#########:%1").arg(dataBlock.data.size()));
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
    _xml->writeStartElement("Property");
    _xml->writeAttribute("id", property.id);
    _xml->writeAttribute("type", idToType[property.value.type()]);

    if(!property.format.isEmpty())
        _xml->writeAttribute("format", property.format);

    if(!property.comment.isEmpty())
        _xml->writeAttribute("comment", property.comment);

    if((QMetaType::Type)property.value.type() == QMetaType::QString)
        _xml->writeCharacters(property.value.toString());
    else
        _xml->writeAttribute("value", property.value.toString());
    _xml->writeEndElement();
}

void XISFWriter::writeMetadata()
{
    _xml->writeStartElement("Metadata");

    writePropertyElement({"XISF:CreationTime", QDateTime::currentDateTimeUtc().toString(Qt::ISODate), QString(), QString()});

    _xml->writeStartElement("Property");
    _xml->writeAttribute("id", "XISF:CreatorApplication");
    _xml->writeAttribute("type", "String");
    _xml->writeCharacters("LibXISF");
    _xml->writeEndElement();

    _xml->writeEndElement();
}

#define REGISTER_METATYPE(type) { int id = qRegisterMetaType<type>("LibXISF::"#type); \
    typeToId.insert({#type, id}); idToType.insert({id, #type}); }

struct TypesInit
{
    TypesInit()
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

static TypesInit typesInit;

}
