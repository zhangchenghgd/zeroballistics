

#ifndef BLUEBEARD_LOD_UPDATER_INCLUDED
#define BLUEBEARD_LOD_UPDATER_INCLUDED


#include <stack>

#include "Singleton.h"
#include "Datatypes.h"
#include "RegisteredFpGroup.h"
#include "Scheduler.h"
#include "Vector.h"


class Vector;
class GameState;
class OsgNodeWrapper;




#define s_lod_updater Loki::SingletonHolder<LodUpdater, Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()
//------------------------------------------------------------------------------
class LodUpdater
{
    DECLARE_SINGLETON(LodUpdater);
 public:

    void reset();
    
    void update(float dt);
    void updateAll();

    void enable(bool v);
    bool isEnabled() const;
    
    void addLodNode(OsgNodeWrapper * node);
    void removeLodNode(const OsgNodeWrapper * node);

    void updateNode(OsgNodeWrapper * node);
    
 protected:


    RegisteredFpGroup fp_group_;

    unsigned next_to_update_;

    float * lod_scale_;       ///< cached parameter.
    std::vector<OsgNodeWrapper*> osg_lod_nodes_; ///< holds PlainOsgNodes
                                                 ///< and InstancedProxies

    hTask updater_enabled_;

    Vector last_camera_pos_;
};


#endif
