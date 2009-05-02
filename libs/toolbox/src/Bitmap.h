/*******************************************************************************
 *
 *  filename            : Bitmap.h
 *  author              : Muschick Christian
 *  date of creation    : 07.02.2003
 *  date of last change : 15.11.2003
 *
 *******************************************************************************/

#ifndef LIB_BITMAP_INCLUDED
#define LIB_BITMAP_INCLUDED




#include <fstream>

#include "Datatypes.h"
#include "Utils.h"
#include "Exception.h"
#include "Serializer.h"

#ifdef _WIN32
#pragma pack(1)
#endif

//------------------------------------------------------------------------------
struct BitmapFileHeader
{
    uint16_t  type_;
    uint32_t size_;
    uint16_t  reserved1_;
    uint16_t  reserved2_;
    uint32_t off_bits_;
}
#ifndef _WIN32
__attribute__((packed))
#endif
;


//------------------------------------------------------------------------------
struct BitmapInfoHeader
{
    uint32_t size_;
    int32_t width_;
    int32_t height_;
    uint16_t  planes_;
    uint16_t  bit_count_;
    uint32_t compression_;
    uint32_t size_image_;
    int32_t x_pixels_per_meter_;
    int32_t y_pixels_per_meter_;
    uint32_t clr_used_;
    uint32_t clr_important_;
}
#ifndef _WIN32
__attribute__((packed))
#endif
;

#ifdef _WIN32
#pragma pack()
#endif


//------------------------------------------------------------------------------
/**
 *  Provides functionality to save a bitmap to disk. Reading from disk
 *  is not implemented as the SDL can be used for this.
 */
class Bitmap
{
 public:
    Bitmap();
    Bitmap(int width, int height, int bytes_per_pixel, const void * bits = NULL);
    Bitmap(const Bitmap & bmp);
    ~Bitmap();

    void create(int width, int height, int bytes_per_pixel, const void * bits);
    void reset();

    int    getWidth()    const;
    int    getHeight()   const;
    int    getBytesPerPixel() const;
    void * getBits() const;
    void * getBits();

    int getPaddedWidth() const;
 private:
    int width_;
    int height_;
    int bpp_; ///< Bytes per pixel
    uint8_t * bits_;
};

namespace serializer
{
bool putInto(Serializer & s, const Bitmap & bmp);
bool putInto(Serializer & s, const BitmapFileHeader & bfh);
bool putInto(Serializer & s, const BitmapInfoHeader & bih);

}

#endif // #ifndef LIB_BITMAP_INCLUDED


