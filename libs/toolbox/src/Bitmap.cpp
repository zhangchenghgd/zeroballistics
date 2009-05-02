/*******************************************************************************
 *
 *  filename            : Bitmap.cpp
 *  author              : Muschick Christian
 *  date of creation    : 07.02.2003
 *  date of last change : 15.11.2003
 *
 *  Implementation file for the class Bitmap.
 *
 *******************************************************************************/

#include "Bitmap.h"

//------------------------------------------------------------------------------
Bitmap::Bitmap() : bits_(NULL)
{
}


//------------------------------------------------------------------------------
/**
 *  Creates a new bitmap object with the specified
 *  dimensions.
 */
Bitmap::Bitmap(int width, int height, int bytes_per_pixel, const void * bits) : bits_(NULL)
{
    create(width, height, bytes_per_pixel, bits);
}

//------------------------------------------------------------------------------
Bitmap::Bitmap(const Bitmap & bmp) : bits_(NULL)
{
    create(bmp.width_, bmp.height_, bmp.bpp_, bmp.bits_);
}

//------------------------------------------------------------------------------
Bitmap::~Bitmap()
{
    reset();
}

//------------------------------------------------------------------------------
void Bitmap::create(int width, int height, int bytes_per_pixel, const void * bits)
{
    assert(bytes_per_pixel == 3);

    reset();
    
    width_  = width;
    height_ = height;
    bpp_    = bytes_per_pixel;

    int num_bytes = getPaddedWidth() * height;
    bits_ = new uint8_t[num_bytes];
    if (!bits_) throw Exception("Unable to allocate bitmap bits");

    if (bits)
    {
        if (getPaddedWidth() == width_*bpp_)
        {
            memcpy(bits_, bits, num_bytes);
        } else
        {
            memset(bits_, 0, num_bytes);
            for (int r=0; r<height; ++r)
            {
                memcpy(&bits_[r*width_*bpp_], &((uint8_t*)bits)[r*width_*bpp_], width_*bpp_);
            }
        }
    }
}


//------------------------------------------------------------------------------
void Bitmap::reset()
{
    DELNULLARRAY(bits_);
}

//------------------------------------------------------------------------------
int Bitmap::getWidth() const
{
    return width_;
}

//------------------------------------------------------------------------------
int Bitmap::getHeight() const
{
    return height_;
}

//------------------------------------------------------------------------------
int Bitmap::getBytesPerPixel() const
{
    return bpp_;
}

//------------------------------------------------------------------------------
void * Bitmap::getBits() const
{
    return bits_;
}

//------------------------------------------------------------------------------
void * Bitmap::getBits()
{
    return bits_;
}

//------------------------------------------------------------------------------
/**
 *  Returns the padded width of the bitmap in bytes.
 */
int Bitmap::getPaddedWidth() const
{
    int res = width_ * bpp_;
    if (res & 3) return (res & ~3) + 4;
    else return res;
}


namespace serializer
{

//------------------------------------------------------------------------------
bool putInto(Serializer & out, const Bitmap & bmp)
{
   assert(!out.isTaggingEnabled());
  
   if (!bmp.getBits()) throw Exception("No bitmap in object");

   BitmapFileHeader file_header;
   BitmapInfoHeader info_header;
   memset(&file_header, 0, sizeof(file_header));
   memset(&info_header, 0, sizeof(info_header));

   int num_bytes = bmp.getPaddedWidth() * bmp.getHeight();
   file_header.type_ = ('M' << 8) | 'B';
   file_header.size_ = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader) + num_bytes;
   file_header.off_bits_ = sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader);

   info_header.size_ = sizeof(BitmapInfoHeader);
   info_header.width_ = bmp.getWidth();
   info_header.height_ = bmp.getHeight();
   info_header.planes_ = 1;
   info_header.bit_count_ = bmp.getBytesPerPixel() << 3;

   out.put(file_header);
   out.put(info_header);
   out.putRaw(bmp.getBits(), num_bytes);
   return true;
}

//------------------------------------------------------------------------------
bool putInto(Serializer & out, const BitmapFileHeader & bfh)
{
    out.put(bfh.type_);
    out.put(bfh.size_);
    out.put(bfh.reserved1_);
    out.put(bfh.reserved2_);
    out.put(bfh.off_bits_);
    return true;
}


//------------------------------------------------------------------------------
bool putInto(Serializer & out, const BitmapInfoHeader & bih)
{
    out.put(bih.size_);
    out.put(bih.width_);
    out.put(bih.height_);
    out.put(bih.planes_);
    out.put(bih.bit_count_);
    out.put(bih.compression_);
    out.put(bih.size_image_);
    out.put(bih.x_pixels_per_meter_);
    out.put(bih.y_pixels_per_meter_);
    out.put(bih.clr_used_);
    out.put(bih.clr_important_);

    return true;
}


} //namespace serializer
