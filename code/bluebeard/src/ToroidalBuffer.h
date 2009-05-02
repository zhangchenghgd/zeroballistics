
#ifndef RACING_TOROIDAL_BUFFER_INCLUDED
#define RACING_TOROIDAL_BUFFER_INCLUDED

#include <loki/Functor.h>


namespace terrain
{

class ToroidalBuffer;

//------------------------------------------------------------------------------
class ToroidalIterator
{
    friend class ToroidalBuffer;
 public:
    ToroidalIterator();
       
    void incXRange(unsigned range,
                   unsigned & start1, unsigned & stop1,
                   unsigned & start2, unsigned & stop2);

    void incX();
    void incY();
    void decX();            
    void decY();
     

    unsigned operator*() const;
      

 protected:

    ToroidalIterator(const ToroidalBuffer * buffer, unsigned x, unsigned y);            
            
    unsigned cur_x_;     ///< The physical x-index of the iterator position
    unsigned cur_y_;     ///< The physical y-index of the iterator position
    unsigned cur_index_; ///< The physical index of the iterator
                         ///position (cur_x_ + buffer_->width_*cur_y_)

    const ToroidalBuffer * buffer_;
};

class ToroidalBuffer;

//------------------------------------------------------------------------------
enum BUFFERFILL_CALLBACK_TYPE
{
    BCT_LOCK,
    BCT_UNLOCK,
    BCT_FILL
};

typedef Loki::Functor<void, LOKI_TYPELIST_8(BUFFERFILL_CALLBACK_TYPE,
                                            unsigned /* dest_x */, unsigned /* dest_y */,
                                            int /* sourcex */, int /* sourcey */,
                                            unsigned /* num_x */, unsigned /* num_y */,
                                            const ToroidalBuffer*) > BufferFillCallback;


//------------------------------------------------------------------------------
/**
 *  Assists in indexing a toroidal 2d-buffer stored in a 1d-array.
 *  Maps logical to physical x,y indices. The logical origin can be
 *  shifted by arbitrary amounts.
 */
class ToroidalBuffer
{
 public:
    
    ToroidalBuffer();
    ToroidalBuffer(unsigned width, unsigned height);
    virtual ~ToroidalBuffer();
    
    void setBufferFillCallback(BufferFillCallback cb);

    void setPos(int x, int y);
    void getPos(int & x, int & y) const;

    void fillArea(unsigned x, unsigned y, unsigned width, unsigned height);
    
    void setSize(unsigned width, unsigned height);
    
    unsigned getWidth() const;
    unsigned getHeight() const;

    void shiftOrigin(int dx, int dy);
    void getOrigin(unsigned & x, unsigned & y) const;

    unsigned getIndex(unsigned x, unsigned y) const;
    unsigned log2Phys(unsigned & x, unsigned & y) const;
    
    void getRangeIndices(bool horz,
                         unsigned & start, unsigned & width, unsigned & width2) const;
    
    ToroidalIterator getIterator(int x, int y) const;
    
 protected:

    void dummyBufferFill(BUFFERFILL_CALLBACK_TYPE type,
                         unsigned dest_x, unsigned dest_y,
                         int source_x, int source_y,
                         unsigned width, unsigned height,
                         const ToroidalBuffer*);

    void fillRectangle(unsigned dest_x, unsigned dest_y,
                       int source_x, int source_y,
                       unsigned width, unsigned height) ;
    
    unsigned width_;  ///< The width of the physical buffer.
    unsigned height_; ///< The height of the physical buffer.

    unsigned origin_x_; ///< The physical x-index of the logical origin.
    unsigned origin_y_; ///< The physical y-index of the logical origin.

    int pos_x_;
    int pos_y_;
    
    BufferFillCallback fill_buffer_;
};


} // namespace terrain

#endif // #ifndef RACING_TOROIDAL_BUFFER_INCLUDED
