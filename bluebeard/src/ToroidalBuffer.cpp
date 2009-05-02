
#include "ToroidalBuffer.h"

#include "Utils.h"

#undef min
#undef max

namespace terrain
{

//------------------------------------------------------------------------------
ToroidalIterator::ToroidalIterator() :
    cur_x_(0), cur_y_(0),
    cur_index_(0),
    buffer_(NULL)
{
}

//------------------------------------------------------------------------------
/**
 *  Increments the iterator's x-position by range and
 *  returns the physical indices spanned by the range
 *  (split into two intervals in case the physical buffer
 *  border is traversed). This is implemented purely for
 *  efficiency reasons to avoid multiple calls to incX().
 */
void ToroidalIterator::incXRange(unsigned range,
                                 unsigned & start1, unsigned & stop1,
                                 unsigned & start2, unsigned & stop2)
{
    assert(range <= buffer_->getWidth());
                    
    start1 = cur_index_;
                    
    if (cur_x_ + range > buffer_->getWidth())
    {
        stop1  = cur_index_ + buffer_->getWidth() - cur_x_;
        start2 = cur_index_ - cur_x_;
        stop2 = start2 + range-(buffer_->getWidth() - cur_x_);

        cur_x_     += range - buffer_->getWidth();
        cur_index_ += range - buffer_->getWidth();
    } else
    {
        stop1 = cur_index_ + range;
        start2 = stop2 = 0;

        cur_x_     += range;
        cur_index_ += range;
    }
}

            
//------------------------------------------------------------------------------
void ToroidalIterator::incX()
{
    if (++cur_x_ == buffer_->getWidth())
    {
        cur_index_ -= buffer_->getWidth()-1;
        cur_x_ = 0;
    } else ++cur_index_;
}
            
//------------------------------------------------------------------------------
void ToroidalIterator::incY()
{
    if (++cur_y_ == buffer_->getHeight())
    {
        cur_index_ -= (buffer_->getHeight()-1)*buffer_->getWidth();
        cur_y_ = 0;
    } else cur_index_ += buffer_->getWidth();
}
            
//------------------------------------------------------------------------------
void ToroidalIterator::decX()
{
    if (cur_x_-- == 0)
    {
        cur_index_ += buffer_->getWidth()-1;
        cur_x_ = buffer_->getWidth()-1;
    } else --cur_index_;
}
            
//------------------------------------------------------------------------------
void ToroidalIterator::decY()
{
    if (cur_y_-- == 0)
    {
        cur_index_ += (buffer_->getHeight()-1)*buffer_->getWidth();
        cur_y_ = buffer_->getHeight()-1;
    } else cur_index_ -= buffer_->getWidth();
}
            
//------------------------------------------------------------------------------
unsigned ToroidalIterator::operator*() const
{
    return cur_index_;
}

//------------------------------------------------------------------------------
ToroidalIterator::ToroidalIterator(const ToroidalBuffer * buffer,
                                   unsigned x, unsigned y):
    cur_x_(x),
    cur_y_(y),
    cur_index_(buffer->log2Phys(cur_x_, cur_y_)),
    buffer_(buffer)
{
}





//------------------------------------------------------------------------------
ToroidalBuffer::ToroidalBuffer() :
    width_(0), height_(0),
    origin_x_(0), origin_y_(0),
    pos_x_(0), pos_y_(0)
{
	fill_buffer_ = BufferFillCallback(this, &ToroidalBuffer::dummyBufferFill);
}


//------------------------------------------------------------------------------
ToroidalBuffer::ToroidalBuffer(unsigned width, unsigned height) :
    width_(width), height_(height),
    origin_x_(0), origin_y_(0),
    pos_x_(0), pos_y_(0)
{
    fill_buffer_ = BufferFillCallback(this, &ToroidalBuffer::dummyBufferFill);
}


//------------------------------------------------------------------------------
ToroidalBuffer::~ToroidalBuffer()
{
}

//------------------------------------------------------------------------------
/**
 *  Sets the callback function that takes responsibility for refilling
 *  the buffer when the toroidalbuffer is shifted or set to a
 *  position.
 *
 *  To avoid multiple locks & frees of a non-local buffer, first the
 *  callback function is called with BCT_LOCK, then up to 4 times with
 *  BCT_FILL, finally with BCT_UNLOCK.
 */
void ToroidalBuffer::setBufferFillCallback(BufferFillCallback cb)
{
    fill_buffer_ = cb;
}

//------------------------------------------------------------------------------
void ToroidalBuffer::setPos(int x, int y)
{
    pos_x_ = x;
    pos_y_ = y;

    origin_x_ = 0;
    origin_y_ = 0;

    fill_buffer_(BCT_LOCK, 0, 0, 0, 0, 0, 0, this);
    fill_buffer_(BCT_FILL,
                 0, 0,
                 pos_x_, pos_y_,
                 width_, height_, this);
    fill_buffer_(BCT_UNLOCK, 0, 0, 0, 0, 0, 0, this);
}


//------------------------------------------------------------------------------
void ToroidalBuffer::getPos(int & x, int & y) const
{
    x = pos_x_;
    y = pos_y_;
}

//------------------------------------------------------------------------------
void ToroidalBuffer::fillArea(unsigned x, unsigned y,
                              unsigned width, unsigned height)
{
    fill_buffer_(BCT_LOCK, 0, 0, 0, 0, 0, 0, this);
    
    fillRectangle(x, y,
                  pos_x_+x, pos_y_+y,
                  width, height);
    
    fill_buffer_(BCT_UNLOCK, 0, 0, 0, 0, 0, 0, this);
}


//------------------------------------------------------------------------------
void ToroidalBuffer::setSize(unsigned width, unsigned height)
{
    width_  = width;
    height_ = height;

    origin_x_ = 0;
    origin_y_ = 0;
}


//------------------------------------------------------------------------------
unsigned ToroidalBuffer::getWidth() const
{
    return width_;
}


//------------------------------------------------------------------------------
unsigned ToroidalBuffer::getHeight() const
{
    return height_;
}


//------------------------------------------------------------------------------
/**
 *  Shifts the buffer origin by an arbitrary amount.
 */
void ToroidalBuffer::shiftOrigin(int dx, int dy)
{
    assert(width_  != 0);
    assert(height_ != 0);

    if (dx == 0 && dy == 0) return;

    if ((unsigned)abs(dx) >= width_ ||
        (unsigned)abs(dy) >= height_)
    {
        setPos(pos_x_+dx, pos_y_+dy);
        return;
    }

    int tmp_x = (dx+(int)origin_x_)%(int)width_;
    int tmp_y = (dy+(int)origin_y_)%(int)height_;
    
    origin_x_ = tmp_x < 0 ? tmp_x + width_  : tmp_x;
    origin_y_ = tmp_y < 0 ? tmp_y + height_ : tmp_y;

    pos_x_ += dx;
    pos_y_ += dy;




    fill_buffer_(BCT_LOCK, 0, 0, 0, 0, 0, 0, this);
    
    unsigned offset_y = 0; // Don't double-fill corner of L
    unsigned num_y = height_;
    if (dy > 0)
    {
        fillRectangle(0, height_-dy,
                      pos_x_, pos_y_+height_-dy,
                      width_, dy);

        num_y -= dy;
    } else if (dy < 0)
    {
        fillRectangle(0, 0,
                      pos_x_, pos_y_,
                      width_, -dy);

        num_y += dy;
        offset_y = -dy;
    }
    
    if (dx > 0)
    {
        fillRectangle(width_-dx, offset_y,
                      pos_x_+width_-dx, pos_y_+offset_y,
                      dx, num_y);
    } else if (dx < 0)
    {
        fillRectangle(0, offset_y,
                      pos_x_, pos_y_+offset_y,
                      -dx, num_y);
    }

    fill_buffer_(BCT_UNLOCK, 0, 0, 0, 0, 0, 0, this);    
}

//------------------------------------------------------------------------------
void ToroidalBuffer::getOrigin(unsigned & x, unsigned & y) const
{
    x = origin_x_;
    y = origin_y_;
}

//------------------------------------------------------------------------------
unsigned ToroidalBuffer::getIndex(unsigned x, unsigned y) const
{
    return log2Phys(x,y);
}


//------------------------------------------------------------------------------
/**
 *  Converts the logical x,z-indices to physical indices by adding the
 *  origin coordinates and performing modulo with the buffer dimensions.
 */
unsigned ToroidalBuffer::log2Phys(unsigned & x, unsigned & y) const
{
    assert(x < width_ && y < height_);

    x += origin_x_;
    if (x >= width_) x-=width_;

    y += origin_y_;
    if (y >= height_) y-=height_;

    return x + width_*y;
}


//------------------------------------------------------------------------------
/**
 *  \param horz True if the toroidal buffer's width should be used,
 *  flase otherwise.
 *
 *  \param start [in,out] Start of the range. Will be set to the
 *  physical start.
 *
 *  \param width [in,out] The width of the range. Will be set to
 *  the width of the first subrange.
 *
 *  \param width2 [out] Will be set to the width of the second
 *  subrange (starting from physical index 0).
 */
void ToroidalBuffer::getRangeIndices(bool horz,
                                        unsigned & start,
                                        unsigned & width, unsigned & width2) const
{
    unsigned limit = horz ? width_ : height_;

    assert(start < limit);

    // First convert start to physical index
    start += horz ? origin_x_ : origin_y_;
    if (start >= limit) start-=limit;

    // Check whether boundary is exceeded
    int diff = std::max((int)start + (int)width - (int)limit,0);

    width2 = diff;
    width  -=  diff;          
}

//------------------------------------------------------------------------------
/**
 *  Returns an iterator which can be used to retrive the physical
 *  index of a position specified in "world units".
 */
ToroidalIterator ToroidalBuffer::getIterator(int x, int y) const
{
    assert(x-pos_x_ >= 0 &&
           y-pos_y_ >= 0);
    
    return ToroidalIterator(this, x-pos_x_, y-pos_y_);
}


//------------------------------------------------------------------------------
void ToroidalBuffer::dummyBufferFill(BUFFERFILL_CALLBACK_TYPE,
                                     unsigned, unsigned,
                                     int, int,
                                     unsigned, unsigned,
                                     const ToroidalBuffer*)
{
}


//------------------------------------------------------------------------------
void ToroidalBuffer::fillRectangle(unsigned dest_x, unsigned dest_y,
                                   int source_x, int source_y,
                                   unsigned width, unsigned height)
{
    unsigned width2;
    unsigned height2;

    getRangeIndices(true,  dest_x, width,  width2);
    getRangeIndices(false, dest_y, height, height2);
    
    fill_buffer_(BCT_FILL,
                 dest_x, dest_y,
                 source_x, source_y,
                 width, height, this);

        
    if (width2)
    {

        fill_buffer_(BCT_FILL,
                     0, dest_y,
                     source_x+width, source_y,
                     width2, height, this);
    }
    if (height2)
    {
        fill_buffer_(BCT_FILL,
                     dest_x, 0,
                     source_x, source_y+height,
                     width, height2, this);
    }
    if (width2 && height2)
    {
        fill_buffer_(BCT_FILL,
                     0, 0,
                     source_x+width, source_y+height,
                     width2, height2, this);
    }
}

} // namespace terrain
