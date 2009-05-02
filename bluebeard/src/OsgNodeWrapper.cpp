

#include "OsgNodeWrapper.h"



#include <osg/MatrixTransform>


#include "UtilsOsg.h"
#include "SceneManager.h"
#include "BbmOsgConverter.h" /// XXX for RootNodeUserData, put someplace else?
#include "LodUpdater.h"



//------------------------------------------------------------------------------
PlainOsgNode::PlainOsgNode(osg::MatrixTransform * node) :
    node_(node),
    cur_lod_level_(-1)
{
}

//------------------------------------------------------------------------------
void PlainOsgNode::setLodLevel(unsigned lvl)
{
    assert(lvl <= NUM_LOD_LVLS);
    if (lvl == cur_lod_level_) return;

    // enable before disable or particle effects get disabled...
    if (lvl < NUM_LOD_LVLS)
    {
        EnableGroupVisitor v2(LOD_LVL_NAME[lvl], true);
        node_->accept(v2);
    }


    if (cur_lod_level_ < NUM_LOD_LVLS)
    {
        EnableGroupVisitor v1(LOD_LVL_NAME[cur_lod_level_], false);
        node_->accept(v1);
    } else
    {
        EnableGroupVisitor v1;
        node_->accept(v1);
    }
    
    cur_lod_level_ = lvl;
}

//------------------------------------------------------------------------------
unsigned PlainOsgNode::getLodLevel() const
{
    return cur_lod_level_;
}

//------------------------------------------------------------------------------
const std::vector<float> & PlainOsgNode::getLodDists() const
{
    assert(node_->getUserData());
    
    return ((RootNodeUserData*)node_->getUserData())->lod_dist_;
}

//------------------------------------------------------------------------------
void PlainOsgNode::setLodDists(const std::vector<float> & dist)
{
    assert(node_->getUserData());
    ((RootNodeUserData*)node_->getUserData())->lod_dist_ = dist;
}


//------------------------------------------------------------------------------
void PlainOsgNode::setTransform(const Matrix & m)
{
    node_->setMatrix(matGl2Osg(m));
}

//------------------------------------------------------------------------------
void PlainOsgNode::addToScene()
{
    s_scene_manager.addNode(node_.get());

    s_lod_updater.addLodNode(this);
}
    
//------------------------------------------------------------------------------
void PlainOsgNode::removeFromScene()
{
    DeleteNodeVisitor d(node_.get());
    s_scene_manager.getRootNode()->accept(d);

    s_lod_updater.removeLodNode(this);
}

//------------------------------------------------------------------------------
Vector PlainOsgNode::getPosition() const
{
    return Vector(node_->getMatrix().getTrans()[0],
                  node_->getMatrix().getTrans()[1],
                  node_->getMatrix().getTrans()[2]);
}

//------------------------------------------------------------------------------
void PlainOsgNode::setPosition(const Vector & v)
{
    osg::Matrix mat = node_->getMatrix();
    mat.setTrans(vecGl2Osg(v));
    node_->setMatrix(mat);
}


//------------------------------------------------------------------------------
float PlainOsgNode::getRadius() const
{
    return node_->getBound().radius();
}


//------------------------------------------------------------------------------
osg::MatrixTransform * PlainOsgNode::getOsgNode()
{
    return node_.get();
}
    
