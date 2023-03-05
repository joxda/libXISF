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

#include "propertyvariant.h"
#include <charconv>
#include <type_traits>
#include <array>
#include <map>
#include <iostream>
#include <regex>
#include <iomanip>
#include "libxisf.h"
#include "pugixml/pugixml.hpp"

template<class... Ts> struct overload : Ts... { bool fail = true; using Ts::operator()...; };
template<class... Ts> overload(Ts...) -> overload<Ts...>;

namespace LibXISF
{

static std::map<std::string, Variant::Type> typeToId = {
    {"Monostate",     Variant::Type::Monostate},
    {"Boolean",       Variant::Type::Boolean},
    {"Int8",          Variant::Type::Int8},
    {"UInt8",         Variant::Type::UInt8},
    {"Int16",         Variant::Type::Int16},
    {"UInt16",        Variant::Type::UInt16},
    {"Int32",         Variant::Type::Int32},
    {"UInt32",        Variant::Type::UInt32},
    {"Int64",         Variant::Type::Int64},
    {"UInt64",        Variant::Type::UInt64},
    {"Float32",       Variant::Type::Float32},
    {"Float64",       Variant::Type::Float64},
    {"Complex32",     Variant::Type::Complex32},
    {"Complex64",     Variant::Type::Complex64},
    {"String",        Variant::Type::String},
    {"TimePoint",     Variant::Type::TimePoint},
    {"I8Vector",      Variant::Type::I8Vector},
    {"UI8Vector",     Variant::Type::UI8Vector},
    {"I16Vector",     Variant::Type::I16Vector},
    {"UI16Vector",    Variant::Type::UI16Vector},
    {"I32Vector",     Variant::Type::I32Vector},
    {"UI32Vector",    Variant::Type::UI32Vector},
    {"I64Vector",     Variant::Type::I64Vector},
    {"UI64Vector",    Variant::Type::UI64Vector},
    {"F32Vector",     Variant::Type::F32Vector},
    {"F64Vector",     Variant::Type::F64Vector},
    {"C32Vector",     Variant::Type::C32Vector},
    {"C64Vector",     Variant::Type::C64Vector},
    {"I8Matrix",      Variant::Type::I8Matrix},
    {"UI8Matrix",     Variant::Type::UI8Matrix},
    {"I16Matrix",     Variant::Type::I16Matrix},
    {"UI16Matrix",    Variant::Type::UI16Matrix},
    {"I32Matrix",     Variant::Type::I32Matrix},
    {"UI32Matrix",    Variant::Type::UI32Matrix},
    {"I64Matrix",     Variant::Type::I64Matrix},
    {"UI64Matrix",    Variant::Type::UI64Matrix},
    {"F32Matrix",     Variant::Type::F32Matrix},
    {"F64Matrix",     Variant::Type::F64Matrix},
    {"C32Matrix",     Variant::Type::C32Matrix},
    {"C64Matrix",     Variant::Type::C64Matrix},
};

static std::map<Variant::Type, const char*> idToType = {
    {Variant::Type::Monostate,     "Monostate"},
    {Variant::Type::Boolean,       "Boolean"},
    {Variant::Type::Int8,          "Int8"},
    {Variant::Type::UInt8,         "UInt8"},
    {Variant::Type::Int16,         "Int16"},
    {Variant::Type::UInt16,        "UInt16"},
    {Variant::Type::Int32,         "Int32"},
    {Variant::Type::UInt32,        "UInt32"},
    {Variant::Type::Int64,         "Int64"},
    {Variant::Type::UInt64,        "UInt64"},
    {Variant::Type::Float32,       "Float32"},
    {Variant::Type::Float64,       "Float64"},
    {Variant::Type::Complex32,     "Complex32"},
    {Variant::Type::Complex64,     "Complex64"},
    {Variant::Type::String,        "String"},
    {Variant::Type::TimePoint,     "TimePoint"},
    {Variant::Type::I8Vector,      "I8Vector"},
    {Variant::Type::UI8Vector,     "UI8Vector"},
    {Variant::Type::I16Vector,     "I16Vector"},
    {Variant::Type::UI16Vector,    "UI16Vector"},
    {Variant::Type::I32Vector,     "I32Vector"},
    {Variant::Type::UI32Vector,    "UI32Vector"},
    {Variant::Type::I64Vector,     "I64Vector"},
    {Variant::Type::UI64Vector,    "UI64Vector"},
    {Variant::Type::F32Vector,     "F32Vector"},
    {Variant::Type::F64Vector,     "F64Vector"},
    {Variant::Type::C32Vector,     "C32Vector"},
    {Variant::Type::C64Vector,     "C64Vector"},
    {Variant::Type::I8Matrix,      "I8Matrix"},
    {Variant::Type::UI8Matrix,     "UI8Matrix"},
    {Variant::Type::I16Matrix,     "I16Matrix"},
    {Variant::Type::UI16Matrix,    "UI16Matrix"},
    {Variant::Type::I32Matrix,     "I32Matrix"},
    {Variant::Type::UI32Matrix,    "UI32Matrix"},
    {Variant::Type::I64Matrix,     "I64Matrix"},
    {Variant::Type::UI64Matrix,    "UI64Matrix"},
    {Variant::Type::F32Matrix,     "I8Matrix"},
    {Variant::Type::F64Matrix,     "UI8Matrix"},
};

template<typename T>
T fromChars(const char *beg, const char *end)
{
    T val;
    std::from_chars(beg, end, val);
    return val;
}

template<typename T>
T fromCharsComplex(const char *beg, const char *end)
{
    T val = {0, 0};
    std::cmatch match;
    std::regex regex("\\(([^,]+),([^)]+)\\)");
    if(std::regex_match(beg, end, match, regex))
    {
        std::from_chars(match[1].first, match[1].second, val.real);
        std::from_chars(match[2].first, match[2].second, val.imag);
    }
    return val;
}

template<typename T>
void fromCharsVector(Variant &v, size_t len, const ByteArray &data)
{
    v = T();
    v.value<T>().resize(len);
    size_t size = len * sizeof(typename T::value_type);
    std::memcpy(&v.value<T>()[0], data.data(), size);
}

template<typename T>
void fromCharsMatrix(Variant &v, size_t rows, size_t cols, const ByteArray &data)
{
    v = T(rows, cols);
    size_t size = rows * cols * sizeof(typename T::value_type);
    std::memcpy(&v.value<T>()(0, 0), data.data(), size);
}

template<typename T>
void toChars(const Variant &v, char *beg, char *end)
{
    std::to_chars(beg, end, v.value<T>());
}

template<typename T>
void toCharsComplex(const Variant &v, std::string &str)
{
    T complex = v.value<T>();
    std::stringstream ss;
    ss.imbue(std::locale("C"));
    ss << "(" << complex.real << "," << complex.imag << ")";
    str = ss.str();
}


template<typename T>
void toCharsVector(const Variant &v, size_t &len, ByteArray &data)
{
    len = v.value<T>().size();
    size_t size = len * sizeof(typename T::value_type);
    data.resize(size);
    std::memcpy(data.data(), &v.value<T>()[0], size);
    data.encode_base64();
}

template<typename T>
void toCharsMatrix(const Variant &v, size_t &rows, size_t &cols, ByteArray &data)
{
    rows = v.value<T>().rows();
    cols = v.value<T>().cols();
    size_t size = rows * cols * sizeof(typename T::value_type);
    data.resize(size);
    std::memcpy(data.data(), &v.value<T>()(0, 0), size);
    data.encode_base64();
}

void deserializeVariant(const pugi::xml_node &node, Variant &variant, const ByteArray &data)
{
    std::string type = node.attribute("type").as_string();
    Variant::Type typeId = typeToId[type];

    if(typeId == Variant::Type::String && !node.attribute("location"))
    {
        variant.setValue(node.text().as_string());
    }
    else if(node.attribute("value"))
    {
        auto attr = node.attribute("value").value();
        switch(typeId)
        {
        case Variant::Type::Int8: variant.setValue(fromChars<Int8>(attr, attr + strlen(attr))); break;
        case Variant::Type::UInt8: variant.setValue(fromChars<UInt8>(attr, attr + strlen(attr))); break;
        case Variant::Type::Int16: variant.setValue(fromChars<Int16>(attr, attr + strlen(attr))); break;
        case Variant::Type::UInt16: variant.setValue(fromChars<UInt16>(attr, attr + strlen(attr))); break;
        case Variant::Type::Int32: variant.setValue(fromChars<Int32>(attr, attr + strlen(attr))); break;
        case Variant::Type::UInt32: variant.setValue(fromChars<UInt32>(attr, attr + strlen(attr))); break;
        case Variant::Type::Int64: variant.setValue(fromChars<Int64>(attr, attr + strlen(attr))); break;
        case Variant::Type::UInt64: variant.setValue(fromChars<UInt64>(attr, attr + strlen(attr))); break;
        case Variant::Type::Float32: variant.setValue(fromChars<Float32>(attr, attr + strlen(attr))); break;
        case Variant::Type::Float64: variant.setValue(fromChars<Float64>(attr, attr + strlen(attr))); break;
        case Variant::Type::Complex32: variant.setValue(fromCharsComplex<Complex32>(attr, attr + strlen(attr))); break;
        case Variant::Type::Complex64: variant.setValue(fromCharsComplex<Complex64>(attr, attr + strlen(attr))); break;
        case Variant::Type::TimePoint:
        {
            std::istringstream ss(node.attribute("value").value());
            std::tm tm = {};
            ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
            variant = tm;
            break;
        }
        case Variant::Type::Boolean:
            variant = node.attribute("value").as_bool();
            break;
        default: break;
        }
    }
    else if(typeId >= Variant::Type::I8Vector && typeId <= Variant::Type::C64Vector)
    {
        size_t len = node.attribute("length").as_ullong();
        switch(typeId)
        {
        case Variant::Type::I8Vector: fromCharsVector<I8Vector>(variant, len, data); break;
        case Variant::Type::UI8Vector: fromCharsVector<UI8Vector>(variant, len, data); break;
        case Variant::Type::I16Vector: fromCharsVector<I16Vector>(variant, len, data); break;
        case Variant::Type::UI16Vector: fromCharsVector<UI16Vector>(variant, len, data); break;
        case Variant::Type::I32Vector: fromCharsVector<I32Vector>(variant, len, data); break;
        case Variant::Type::UI32Vector: fromCharsVector<UI32Vector>(variant, len, data); break;
        case Variant::Type::I64Vector: fromCharsVector<I64Vector>(variant, len, data); break;
        case Variant::Type::UI64Vector: fromCharsVector<UI64Vector>(variant, len, data); break;
        case Variant::Type::F32Vector: fromCharsVector<F32Vector>(variant, len, data); break;
        case Variant::Type::F64Vector: fromCharsVector<F64Vector>(variant, len, data); break;
        case Variant::Type::C32Vector: fromCharsVector<C32Vector>(variant, len, data); break;
        case Variant::Type::C64Vector: fromCharsVector<C64Vector>(variant, len, data); break;
        default: break;
        }
    }
    else if(typeId >= Variant::Type::I8Matrix && typeId <= Variant::Type::C64Matrix)
    {
        size_t rows = node.attribute("rows").as_ullong();
        size_t cols = node.attribute("columns").as_ullong();
        switch(typeId)
        {
        case Variant::Type::I8Matrix: fromCharsMatrix<I8Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI8Matrix: fromCharsMatrix<UI8Matrix>(variant, rows, cols, data); break;
        case Variant::Type::I16Matrix: fromCharsMatrix<I16Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI16Matrix: fromCharsMatrix<UI16Matrix>(variant, rows, cols, data); break;
        case Variant::Type::I32Matrix: fromCharsMatrix<I32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI32Matrix: fromCharsMatrix<UI32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::I64Matrix: fromCharsMatrix<I64Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI64Matrix: fromCharsMatrix<UI64Matrix>(variant, rows, cols, data); break;
        case Variant::Type::F32Matrix: fromCharsMatrix<F32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::F64Matrix: fromCharsMatrix<F64Matrix>(variant, rows, cols, data); break;
        case Variant::Type::C32Matrix: fromCharsMatrix<C32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::C64Matrix: fromCharsMatrix<C64Matrix>(variant, rows, cols, data); break;
        default: break;
        }
    }
}

void serializeVariant(pugi::xml_node &node, const Variant &variant)
{
    char str[32] = {0};
    char *end = str + sizeof(str);

    node.append_attribute("type").set_value(variant.typeName());

    if(variant.type() == Variant::Type::String)
    {
        node.append_child(pugi::node_pcdata).set_value(variant.value<String>().c_str());
    }
    else if(variant.type() == Variant::Type::Boolean)
    {
        node.append_attribute("value").set_value(variant.value<Boolean>() ? 1 : 0);
    }
    else if(variant.type() >= Variant::Type::Int8 && variant.type() <= Variant::Type::Float64)
    {
        switch(variant.type())
        {
        case Variant::Type::Int8: toChars<Int8>(variant, str, end); break;
        case Variant::Type::UInt8: toChars<UInt8>(variant, str, end); break;
        case Variant::Type::Int16: toChars<Int16>(variant, str, end); break;
        case Variant::Type::UInt16: toChars<UInt16>(variant, str, end); break;
        case Variant::Type::Int32: toChars<Int32>(variant, str, end); break;
        case Variant::Type::UInt32: toChars<UInt32>(variant, str, end); break;
        case Variant::Type::Int64: toChars<Int64>(variant, str, end); break;
        case Variant::Type::UInt64: toChars<UInt64>(variant, str, end); break;
        case Variant::Type::Float32: toChars<Float32>(variant, str, end); break;
        case Variant::Type::Float64: toChars<Float64>(variant, str, end); break;
        default: break;
        }
        node.append_attribute("value").set_value(str);
    }
    else if(variant.type() == Variant::Type::Complex32 || variant.type() == Variant::Type::Complex64)
    {
        std::string str;
        if(variant.type() == Variant::Type::Complex32)
            toCharsComplex<Complex32>(variant, str);
        else
            toCharsComplex<Complex64>(variant, str);
        node.append_attribute("value").set_value(str.c_str());
    }
    else if(variant.type() >= Variant::Type::I8Vector && variant.type() <= Variant::Type::C64Vector)
    {
        size_t len = 0;
        ByteArray data;
        switch(variant.type())
        {
        case Variant::Type::I8Vector: toCharsVector<I8Vector>(variant, len, data); break;
        case Variant::Type::UI8Vector: toCharsVector<UI8Vector>(variant, len, data); break;
        case Variant::Type::I16Vector: toCharsVector<I16Vector>(variant, len, data); break;
        case Variant::Type::UI16Vector: toCharsVector<UI16Vector>(variant, len, data); break;
        case Variant::Type::I32Vector: toCharsVector<I32Vector>(variant, len, data); break;
        case Variant::Type::UI32Vector: toCharsVector<UI32Vector>(variant, len, data); break;
        case Variant::Type::I64Vector: toCharsVector<I64Vector>(variant, len, data); break;
        case Variant::Type::UI64Vector: toCharsVector<UI64Vector>(variant, len, data); break;
        case Variant::Type::F32Vector: toCharsVector<F32Vector>(variant, len, data); break;
        case Variant::Type::F64Vector: toCharsVector<F64Vector>(variant, len, data); break;
        case Variant::Type::C32Vector: toCharsVector<C32Vector>(variant, len, data); break;
        case Variant::Type::C64Vector: toCharsVector<C64Vector>(variant, len, data); break;
        default: break;
        }
        node.append_attribute("length").set_value(len);
        node.append_attribute("location").set_value("inline:base64");
        node.append_child(pugi::node_pcdata).set_value(data.constData(), data.size());
    }
    else if(variant.type() >= Variant::Type::I8Matrix && variant.type() <= Variant::Type::C64Matrix)
    {
        size_t rows = 0;
        size_t cols = 0;
        ByteArray data;
        switch(variant.type())
        {
        case Variant::Type::I8Matrix: toCharsMatrix<I8Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI8Matrix: toCharsMatrix<UI8Matrix>(variant, rows, cols, data); break;
        case Variant::Type::I16Matrix: toCharsMatrix<I16Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI16Matrix: toCharsMatrix<UI16Matrix>(variant, rows, cols, data); break;
        case Variant::Type::I32Matrix: toCharsMatrix<I32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI32Matrix: toCharsMatrix<UI32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::I64Matrix: toCharsMatrix<I64Matrix>(variant, rows, cols, data); break;
        case Variant::Type::UI64Matrix: toCharsMatrix<UI64Matrix>(variant, rows, cols, data); break;
        case Variant::Type::F32Matrix: toCharsMatrix<F32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::F64Matrix: toCharsMatrix<F64Matrix>(variant, rows, cols, data); break;
        case Variant::Type::C32Matrix: toCharsMatrix<C32Matrix>(variant, rows, cols, data); break;
        case Variant::Type::C64Matrix: toCharsMatrix<C64Matrix>(variant, rows, cols, data); break;
        default: break;
        }

        node.append_attribute("rows").set_value(rows);
        node.append_attribute("columns").set_value(cols);
        node.append_attribute("location").set_value("inline:base64");
        node.append_child(pugi::node_pcdata).set_value(data.constData(), data.size());
    }
    else if(variant.type() == Variant::Type::TimePoint)
    {
        std::ostringstream ss;
        ss << std::put_time(&variant.value<TimePoint>(), "%Y-%m-%dT%H:%M:%SZ");
        node.append_attribute("value").set_value(ss.str().c_str());
    }
}

void ByteArray::makeUnique()
{
    if(!_data.unique())
        _data = std::make_unique<PtrType>(_data->begin(), _data->end());
}

ByteArray::ByteArray(size_t size)
{
    _data = std::make_shared<std::vector<char>>();
    _data->resize(size);
}

ByteArray::ByteArray(const char *ptr) : ByteArray((size_t)0)
{
    size_t len = std::strlen(ptr);
    if(len)
    {
        _data->resize(len);
        std::memcpy(data(), ptr, len);
    }
}

ByteArray::ByteArray(const ByteArray &d)
{
    _data = d._data;
}

char& ByteArray::operator[](size_t i)
{
    makeUnique();
    return (*_data)[i];
}

const char& ByteArray::operator[](size_t i) const
{
    return (*_data)[i];
}

size_t ByteArray::size() const
{
    return _data->size();
}

void ByteArray::resize(size_t newsize)
{
    makeUnique();
    _data->resize(newsize);
}

void ByteArray::append(char c)
{
    _data->push_back(c);
}

void ByteArray::decode_base64()
{
    int i = 0;
    Ptr tmp = std::make_unique<PtrType>();

    uint8_t c4[4] = {0};
    for(uint8_t c : *_data)
    {
        if(c >= 'A' && c <= 'Z')c4[i++] = c - 'A';
        else if(c >= 'a' && c <= 'z')c4[i++] = c - 'a' + 26;
        else if(c >= '0' && c <= '9')c4[i++] = c - '0' + 52;
        else if(c == '+')c4[i++] = 62;
        else if(c == '/')c4[i++] = 63;

        if(i == 4)
        {
            tmp->push_back((c4[0] << 2) | (c4[1] >> 4));
            tmp->push_back((c4[1] << 4) | (c4[2] >> 2));
            tmp->push_back((c4[2] << 6) | c4[3]);
            i = 0;
        }
    }

    if(i > 1)tmp->push_back((c4[0] << 2) | (c4[1] >> 4));
    if(i > 2)tmp->push_back((c4[1] << 4) | (c4[2] >> 2));

    std::swap(_data, tmp);
}

void ByteArray::encode_base64()
{
    static const char *base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    Ptr tmp = std::make_unique<PtrType>();
    int i = 0;
    uint8_t sextet[4] = {0};
    for(uint8_t c : *_data)
    {
        switch(i)
        {
        case 0:
            sextet[0] |= c >> 2 & 0x3f;
            sextet[1] |= c << 4 & 0x3f;
            i++;
            break;
        case 1:
            sextet[1] |= c >> 4 & 0x3f;
            sextet[2] |= c << 2 & 0x3f;
            i++;
            break;
        case 2:
            sextet[2] |= c >> 6 & 0x3f;
            sextet[3] = c & 0x3f;
            i = 0;
            for(int o=0; o<4; o++)
                tmp->push_back(base64[sextet[o]]);
            std::memset(sextet, 0, sizeof(sextet));
            break;
        }
    }
    for(int o = 0; o <= i && i; o++)
        tmp->push_back(base64[sextet[o]]);

    if(tmp->size() % 4)
        tmp->resize(tmp->size() + 4 - (tmp->size() % 4), '=');
    std::swap(_data, tmp);
}


void ByteArray::encode_hex()
{
    static const char *hex = "0123456789abcdef";
    Ptr tmp = std::make_unique<PtrType>(_data->size() * 2);
    for(size_t i = 0; i< _data->size(); i++)
    {
        uint8_t t = static_cast<uint8_t>(_data->at(i));
        (*tmp)[2*i + 0] = hex[(t & 0xf0) >> 4];
        (*tmp)[2*i + 1] = hex[t & 0xf];
    }
    std::swap(_data, tmp);
}

void ByteArray::decode_hex()
{
    auto toByte = [](char c) -> char
    {
        if(c >= '0' && c <= '9')
            return c - '0';
        if(c >= 'A' && c <= 'F')
            return c - '7';
        if(c >= 'a' && c <= 'f')
            return c - 'W';
        return 0;
    };

    Ptr tmp = std::make_unique<PtrType>(size() / 2);
    for(size_t i = 0; i< tmp->size(); i++)
    {
        (*tmp)[i] = (toByte(_data->at(i*2)) << 4) | toByte(_data->at(i*2+1));
    }
    std::swap(_data, tmp);
}

ByteStream::ByteStream()
{
    //setg(_buffer.data(), _buffer.data(), _buffer.data() + _buffer.size());
}

ByteStream::~ByteStream()
{

}

const ByteArray &ByteStream::buffer() const
{
    return _buffer;
}

void ByteStream::stats()
{
    std::cout << "pbase " << (void*)pbase() << std::endl;
    std::cout << "pptr  " << (void*)pptr() << std::endl;
    std::cout << "epptr " << (void*)epptr() << std::endl;
    std::cout << "eback " << (void*)eback() << std::endl;
    std::cout << "gptr  " << (void*)gptr() << std::endl;
    std::cout << "egptr " << (void*)egptr() << std::endl;
}

ByteStream::pos_type ByteStream::seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode)
{
    return pos_type(off_type(-1));
}

ByteStream::pos_type ByteStream::seekpos(pos_type, std::ios_base::openmode)
{
    return pos_type(off_type(-1));
}

/*std::streamsize ByteStream::showmanyc()
{
    return _buffer.size() - ipos;
}

std::streamsize ByteStream::xsgetn(char_type *s, std::streamsize n)
{
    if((std::streamoff)_buffer.size() > ipos + n)
    {
        std::memcpy(s, _buffer.data() + ipos, n);
        return n;
    }
    return pos_type(off_type(-1));
}

ByteStream::int_type ByteStream::underflow()
{
    if(ipos >= (std::streamoff)_buffer.size())return traits_type::eof();
    return traits_type::to_int_type(_buffer[ipos]);
}*/

std::streamsize ByteStream::xsputn(const char_type *s, std::streamsize n)
{
    if(opos + n >= (std::streamsize)_buffer.size())
    {
        _buffer.resize(opos + n);
        setoptr();
    }

    std::memcpy(_buffer.data() + opos, s, n);
    opos += n;
    return n;
}

ByteStream::int_type ByteStream::overflow(int_type c)
{
    _buffer.append(c);
    return c;
}

void ByteStream::setoptr()
{
    size_t off = gptr() - eback();
    setg(_buffer.data(), _buffer.data() + off, _buffer.data() + _buffer.size());
}

Variant::Type Variant::type() const
{
    return (Variant::Type)_value.index();
}

const char* Variant::typeName() const
{
    if(idToType.count(type()))
        return idToType[type()];
    else
        return "";
}

}
