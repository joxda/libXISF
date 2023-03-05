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

#ifndef PROPERTYVARIANT_H
#define PROPERTYVARIANT_H

#include <variant>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <cstring>
#include <ctime>
#include <sstream>

namespace LibXISF
{

class ByteArray
{
    using PtrType = std::vector<char>;
    using Ptr = std::shared_ptr<PtrType>;
    Ptr _data;
    void makeUnique();
public:
    ByteArray() : ByteArray((size_t)0) {}
    explicit ByteArray(size_t size);
    explicit ByteArray(const char *ptr);
    ByteArray(const char *ptr, size_t size)
    {
        _data = std::make_shared<std::vector<char>>();
        _data->resize(size);
        std::memcpy(data(), ptr, size);
    }
    ByteArray(const ByteArray &d);
    char& operator[](size_t i);
    const char& operator[](size_t i) const;
    char* data() { return &_data->at(0); }
    const char* data() const { return &_data->at(0); }
    const char* constData() const { return &_data->at(0); }
    size_t size() const;
    void resize(size_t newsize);
    void append(char c);
    void decode_base64();
    void encode_base64();
    void encode_hex();
    void decode_hex();
};

class ByteStream : public std::basic_streambuf<char>
{
    ByteArray _buffer;
    pos_type opos = 0;
    pos_type ipos = 0;
public:
    ByteStream();
    ~ByteStream();
    const ByteArray& buffer() const;
    void stats();
protected:
    pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;
    pos_type seekpos(pos_type, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override;

    /*std::streamsize showmanyc() override;
    std::streamsize xsgetn(char_type *s, std::streamsize n) override;
    int_type underflow() override;*/

    std::streamsize xsputn(const char_type *s, std::streamsize n) override;
    int_type overflow(int_type c = traits_type::eof()) override;

    void setoptr();
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
    using value_type = T;

    Matrix() = default;
    Matrix(int rows, int cols) : _rows(rows), _cols(cols), _elem(rows * cols) {}
    void resize(int rows, int cols) { _rows = rows; _cols = cols; _elem.resize(rows * cols); }
    T& operator()(int row, int col) { return _elem[row * _cols + col]; }
    const T& operator()(int row, int col) const { return _elem[row * _cols + col]; }
    int rows() const { return _rows; }
    int cols() const { return _cols; }
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
typedef std::string String;
typedef std::tm TimePoint;
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

class Variant
{
    using StdVariant = std::variant<std::monostate, Boolean, Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64, Float32, Float64,
        Complex32, Complex64, String, TimePoint,
        I8Vector, UI8Vector, I16Vector, UI16Vector, I32Vector, UI32Vector, I64Vector, UI64Vector, F32Vector, F64Vector, C32Vector, C64Vector,
        I8Matrix, UI8Matrix, I16Matrix, UI16Matrix, I32Matrix, UI32Matrix, I64Matrix, UI64Matrix, F32Matrix, F64Matrix, C32Matrix, C64Matrix>;
    StdVariant _value;
public:
    enum class Type
    {
        Monostate, Boolean, Int8, UInt8, Int16, UInt16, Int32, UInt32, Int64, UInt64, Float32, Float64,
        Complex32, Complex64, String, TimePoint,
        I8Vector, UI8Vector, I16Vector, UI16Vector, I32Vector, UI32Vector, I64Vector, UI64Vector, F32Vector, F64Vector, C32Vector, C64Vector,
        I8Matrix, UI8Matrix, I16Matrix, UI16Matrix, I32Matrix, UI32Matrix, I64Matrix, UI64Matrix, F32Matrix, F64Matrix, C32Matrix, C64Matrix
    };

    Variant() = default;
    template<typename T>
    Variant(const T &t) : _value(t) {}
    Type type() const;
    const char *typeName() const;
    template<typename T>
    T& value() { return std::get<T>(_value); }
    template<typename T>
    const T& value() const { return std::get<T>(_value); }
    template<typename T>
    void setValue(const T& val) { _value = val; }
};

}

#endif // PROPERTYVARIANT_H
