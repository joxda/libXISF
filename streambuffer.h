#ifndef STREAMBUFFER_H
#define STREAMBUFFER_H

#include <streambuf>
#include "libxisf.h"

namespace LibXISF
{

class StreamBuffer : public std::streambuf
{
public:
    StreamBuffer();
    StreamBuffer(const ByteArray &byteArray);
    ByteArray byteArray();
protected:
    pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) override;
    pos_type seekpos(pos_type pos, std::ios_base::openmode mode = std::ios_base::in | std::ios_base::out) override;

    std::streamsize xsgetn(char_type *s, std::streamsize n) override;
    int_type underflow() override;

    std::streamsize xsputn(const char_type *s, std::streamsize n) override;
    int_type overflow(int_type c = traits_type::eof()) override;
private:
    void update_ptrs();
    off_type _size = 0;
    ByteArray _byteArray;
};

}

#endif // STREAMBUFFER_H
