
#ifndef BLUEBERAD_OSG_NODE_WRAPPER_INCLUDED
#define BLUEBERAD_OSG_NODE_WRAPPER_INCLUDED


#include <osg/ref_ptr>
#include <osg/Referenced>


#include "Vector.h"


namespace osg
{
    class Node;
    class MatrixTransform;
    class NodeCallback;
}


class Matrix;

//------------------------------------------------------------------------------
/**
 *  Wraps either an ordinary osg node or an (pseudo-)instanced object.
 */
class OsgNodeWrapper : public osg::Referenced
{
 public:
    OsgNodeWrapper() {}
    virtual ~OsgNodeWrapper() {}
    
    virtual void setLodLevel(unsigned lvl)                       = 0;
    virtual unsigned getLodLevel() const                         = 0;

    virtual const std::vector<float> & getLodDists() const       = 0;
    virtual void  setLodDists(const std::vector<float> & dist)   = 0;
    
    virtual void setTransform(const Matrix & m)                  = 0;
    virtual void addToScene()                                    = 0;
    virtual void removeFromScene()                               = 0;

    virtual Vector getPosition() const                           = 0;
    virtual void setPosition(const Vector & v)                   = 0;
    
    virtual float getRadius() const                              = 0;
    
    virtual osg::MatrixTransform * getOsgNode()                  = 0;
    
    static OsgNodeWrapper * create(osg::Node * imported_node);
};



//------------------------------------------------------------------------------
class PlainOsgNode : public OsgNodeWrapper
{
 public:
    PlainOsgNode(osg::MatrixTransform * node);

    virtual void setLodLevel(unsigned lvl);
    virtual unsigned getLodLevel() const;

    virtual const std::vector<float> & getLodDists() const;
    virtual void  setLodDists(const std::vector<float> & dist);
    
    virtual void setTransform(const Matrix & m);
    virtual void addToScene();
    virtual void removeFromScene();

    virtual Vector getPosition() const;
    virtual void setPosition(const Vector & v);
    
    virtual float getRadius() const;

    virtual osg::MatrixTransform * getOsgNode();
    
 protected:
    osg::ref_ptr<osg::MatrixTransform> node_;

    unsigned cur_lod_level_;
};




#endif
