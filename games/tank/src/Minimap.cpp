

#include "Minimap.h"


#include <osg/Geode>
#include <osg/NodeCallback>
#include <osg/MatrixTransform>
#include <osg/TexMat>
#include <osg/Projection>
#include <osg/Stencil>

#include "PuppetMasterClient.h"
#include "GameLogicClient.h"
#include "UtilsOsg.h"
#include "RigidBody.h"
#include "SceneManager.h"
#include "Observable.h"
#include "ParameterManager.h"
#include "Scheduler.h"
#include "LevelData.h"
#include "HudTextureElement.h"

const std::string MINIMAP_ICON_PATH = "data/textures/hud/minimap/";


//------------------------------------------------------------------------------
Minimap::Minimap(PuppetMasterClient * master) :
    master_(master),
    size_(1.0f)
{
    Vector2d pos = HudTextureElement::getScreenCoords("minimap");
    
    tex_matrix_ = new osg::TexMat;
    icon_group_ = new osg::Group;
    bg_group_   = new osg::Group;

    proj_ = new osg::Projection;
    proj_->setMatrix(osg::Matrix::ortho2D(0,1,0,1));
    proj_->setName("Minimap root group");
    s_scene_manager.addHudNode(proj_.get());

    // setup mask nodes
    MinimapCircle * mask = new MinimapCircle(0.5f, 0.5f,
                                             0.5f - 0.5f*s_params.get<float>("hud.minimap.icon_size"));
    osg::Geode * mask_geode = new osg::Geode;
    mask_geode->addDrawable(mask);
    mask_geode->setName("Stencil mask");

    // setup rendering the mask (circle) into the stencil buffer
    osg::StateSet * stateset_mask = mask_geode->getOrCreateStateSet();
    osg::Stencil* stencil = new osg::Stencil;
    stencil->setFunction(osg::Stencil::ALWAYS, 1, 0xffffffff);
    stencil->setOperation(osg::Stencil::REPLACE, osg::Stencil::REPLACE, osg::Stencil::REPLACE);
    stateset_mask->setAttributeAndModes(stencil,osg::StateAttribute::ON);
    stateset_mask->setRenderBinDetails(BN_HUD_MINIMAP_MASK, "RenderBin"); // draw mask before map
    proj_->addChild(mask_geode);


    icon_group_->setName("icon group");
    icon_group_->getOrCreateStateSet()->setRenderBinDetails(BN_HUD_MINIMAP_ICONS, "RenderBin"); // minimap after mask
    proj_->addChild(icon_group_.get());


    bg_group_->setName("bg group");
    bg_group_->getOrCreateStateSet()->setRenderBinDetails(BN_HUD_MINIMAP,       "RenderBin"); // minimap after mask
    osg::Stencil* stencil2 = new osg::Stencil;
    stencil2->setFunction(osg::Stencil::EQUAL, 1, 0xffffffff);
    stencil2->setOperation(osg::Stencil::KEEP, osg::Stencil::KEEP, osg::Stencil::KEEP);
    bg_group_->getOrCreateStateSet()->setAttributeAndModes(stencil2,osg::StateAttribute::ON);
    proj_->addChild(bg_group_.get());

    
    s_scene_manager.addObserver(ObserverCallbackFun0(this, &Minimap::onWindowResized),
                                SOE_RESOLUTION_CHANGED,
                                &fp_group_);

    // set minimap viewport
    onWindowResized();
}



//------------------------------------------------------------------------------
Minimap::~Minimap()
{
    DeleteNodeVisitor del(proj_.get());
    s_scene_manager.getRootNode()->accept(del);

    clear();    
}

//------------------------------------------------------------------------------
void Minimap::setupBackground(const std::string map_file, float level_size)
{
    map_center_ = Vector2d(level_size * 0.5, level_size * 0.5);
    size_       = level_size;

    // Remove old minimap background texture
    if (map_texture_.get())
    {
        assert(map_texture_->getNumParents() == 1);
        osg::Node * geode = map_texture_->getParent(0);
        bool r = bg_group_->removeChild(geode);
        UNUSED_VARIABLE(r);
        assert(r);
    }

    map_texture_ = new HudTextureElement( "minimap_background", map_file);
    map_texture_->getOrCreateStateSet()->setTextureAttribute(0, tex_matrix_.get());

    

    osg::Geode * geode = new osg::Geode;


    geode->addDrawable(map_texture_.get());
    geode->setName("level minimap");
    bg_group_->addChild(geode);
}


//------------------------------------------------------------------------------
void Minimap::update()
{
    const Matrix & camera_transform = master_->getGameLogic()->getCameraPosition();            
    float view_angle = atan2f(camera_transform.getZ().x_,
                              camera_transform.getZ().z_);
    Vector2d position = camera_transform.getTranslation();


    // Minimap itself: update texture matrix to show proper portion of
    // texture

    // trans * scale * rot * pos_offset


    osg::Matrix trans_neg;
    trans_neg.makeTranslate(osg::Vec3(-0.5, -0.5, 0.0));


    osg::Matrix map_scale;
    float map_extents = s_params.get<float>("hud.minimap.map_extents");
    float icon_size = s_params.get<float>("hud.minimap.icon_size");
    float corrected_scale = map_extents / size_;
    map_scale.makeScale(osg::Vec3(corrected_scale, corrected_scale, 1.0f));

    osg::Matrix view_rotate;
    view_rotate.makeRotate(view_angle, osg::Vec3(0,0,1));

    osg::Matrix pos_offset;
    pos_offset.makeTranslate( 0.5f + (position.x_ - map_center_.x_) / size_,
                              0.5f - (position.y_ - map_center_.y_) / size_,
                              0.0f);

    tex_matrix_->setMatrix(trans_neg * map_scale * view_rotate * pos_offset);

    
    // Minimap icons
    // icon_scale * object_rot * offset * view_rot * trans
    osg::Matrix icon_scale;
    icon_scale.makeScale(osg::Vec3(icon_size, icon_size, 1.0f));

    osg::Matrix view_rot;
    view_rot.makeRotate(-view_angle, osg::Vec3(0,0,1));

    // This is neccessary because [0,1]x[0,1] is mapped to the minimap
    osg::Matrix trans;
    trans.makeTranslate(osg::Vec3(0.5, 0.5, 0.0));

    osg::Matrix m2 = view_rot * trans;
    
    for (BodyIconContainer::iterator it = icon_.begin();
         it != icon_.end();
         ++it)
    {
        osg::Matrix t = getIconTransform(position, it->first->getTransform(),
                                         map_extents, icon_size);
        it->second.transform_->setMatrix(icon_scale  * t * m2 );
    }
    for (StaticIconContainer::iterator it = static_icon_.begin();
         it != static_icon_.end();
         ++it)
    {
        osg::Matrix t = getIconTransform(position, it->first,
                                         map_extents, icon_size);
        it->second->setMatrix(icon_scale  * t * m2 );
    }



    // Nodes not positioned by rigidbody

    map_scale.makeScale(osg::Vec3(1.0f/map_extents, 1.0f/map_extents, 1.0f));
    pos_offset.makeTranslate(-position.x_ / map_extents,
                             position.y_ / map_extents,
                             0.0);
    for (unsigned i=0; i<node_.size(); ++i)
    {
        node_[i]->setMatrix(map_scale * pos_offset * view_rot * trans);
    }    
}

//------------------------------------------------------------------------------
/**
 *  Removes all icons from the minimap.
 */
void Minimap::clear()
{

    for (NodeContainer::iterator it = node_.begin();
         it != node_.end();
         ++it)
    {
        DeleteNodeVisitor del(it->get());
        bg_group_->accept(del);        
    }
    node_.clear();
    
    for (BodyIconContainer::iterator it = icon_.begin();
         it != icon_.end();
         ++it)
    {
        // We don't want to be notified of deletion anymore
        fp_group_.deregister(ObserverFp(&fp_group_, it->first, GOE_SCHEDULED_FOR_DELETION));

        DeleteNodeVisitor del(it->second.transform_.get());
        icon_group_->accept(del);
    }
    icon_.clear();

    icon_group_->removeChildren(0, icon_group_->getNumChildren());
}


//------------------------------------------------------------------------------
/**
 *  Adds an osg node not attached to a rigidbody to the
 *  minimap. Currently used for boundary outline.
 */
void Minimap::addNode(osg::Node * node)
{
    osg::MatrixTransform * transform = new osg::MatrixTransform;
    transform->addChild(node);

    node_.push_back(transform);

    bg_group_->addChild(transform);
}


//------------------------------------------------------------------------------
void Minimap::addIcon(RigidBody * body, const std::string & icon)
{
    assert(body);
    
    HudTextureElement * tex = new HudTextureElement("minimap_icons", MINIMAP_ICON_PATH + icon);

    osg::Geode * geode = new osg::Geode;
    geode->setName(icon);
    geode->addDrawable(tex);
    
    osg::MatrixTransform * transform = new osg::MatrixTransform;
    transform->addChild(geode);

    assert(icon_.find(body) == icon_.end());
    icon_.insert(std::make_pair(body, MinimapIcon(transform)));

    icon_group_->addChild(transform);

    body->addObserver(ObserverCallbackFun2(this, &Minimap::onBodyScheduledForDeletion),
                      GOE_SCHEDULED_FOR_DELETION,
                      &fp_group_);
}


//------------------------------------------------------------------------------
void Minimap::addIcon(const Matrix & offset, const std::string & icon)
{
    HudTextureElement * tex = new HudTextureElement("minimap_icons", MINIMAP_ICON_PATH + icon);

    osg::Geode * geode = new osg::Geode;
    geode->setName(icon);
    geode->addDrawable(tex);
    
    osg::MatrixTransform * transform = new osg::MatrixTransform;
    transform->addChild(geode);

    static_icon_.push_back(std::make_pair(offset, transform));

    icon_group_->addChild(transform);
}


//------------------------------------------------------------------------------
bool Minimap::hasIcon(RigidBody * body) const
{
    return icon_.find(body) != icon_.end();
}


//------------------------------------------------------------------------------
/**
 *  \return Whether an icon for the specified body existed and was deleted.
 */
bool Minimap::removeIcon(RigidBody * body)
{
    BodyIconContainer::iterator it = icon_.find(body);
    if (it==icon_.end()) return false;

    fp_group_.deregister(ObserverFp(&fp_group_, body, GOE_SCHEDULED_FOR_DELETION));
    
    DeleteNodeVisitor del(it->second.transform_.get());
    icon_group_->accept(del);
    
    icon_.erase(it);

    return true;
}


//------------------------------------------------------------------------------
void Minimap::enableFlash(RigidBody * body, bool b)
{
    BodyIconContainer::iterator it = icon_.find(body);
    if (it == icon_.end())
    {
        s_log << Log::warning
              << "Cannot flash "
              << *body
              << " because there is no icon for it.\n";
        return;
    }

    if (b)
    {
        flashIcon(&it->second);
    } else
    {
        // Clear flash event
        it->second.fp_group_.deregisterAllOfType(TaskFp());
        it->second.transform_->setNodeMask(NODE_MASK_VISIBLE);
    }
}

//------------------------------------------------------------------------------
void Minimap::enableFlash(RigidBody * body, float duration)
{
    BodyIconContainer::iterator it = icon_.find(body);
    if (it == icon_.end())
    {
        s_log << Log::warning
              << "Cannot flash "
              << *body
              << " for "
              << duration
              << " secs because there is no icon for it.\n";
        return;
    }

    flashIcon(&it->second);

    s_scheduler.addEvent(SingleEventCallback(this, &Minimap::stopFlashingIcon),
                         duration,
                         &it->second,
                         "stop flashing Icon",
                         &(it->second.fp_group_));  
    
}

//------------------------------------------------------------------------------
void Minimap::onWindowResized()
{
    unsigned width, height;
    s_scene_manager.getWindowSize(width, height);

    Vector2d pos = HudTextureElement::getScreenCoords("minimap");
    float size   = HudTextureElement::alignAndGetSize("minimap", pos);
    
    pos  *= (float)height;
    size *= (float)height;

    pos.x_ += (float)(width-height)*0.5f;

    proj_->getOrCreateStateSet()->setAttribute(new osg::Viewport(pos.x_, pos.y_, size, size));
}

//------------------------------------------------------------------------------
void Minimap::enable(bool e)
{
    proj_->setNodeMask(e ? NODE_MASK_VISIBLE : NODE_MASK_INVISIBLE);
}



//------------------------------------------------------------------------------
/**
 *  Remove icon from osg and delete it from our icon map.
 */
void Minimap::onBodyScheduledForDeletion(Observable * body, unsigned event)
{
    bool b = removeIcon((RigidBody*)body);
    UNUSED_VARIABLE(b);
    assert(b);
}


//------------------------------------------------------------------------------
void Minimap::flashIcon(void * i)
{
    MinimapIcon * icon = (MinimapIcon*)i;

    osg::Node::NodeMask mask = icon->transform_->getNodeMask();

    float icon_flash_period     = s_params.get<float>("hud.minimap.icon_flash_period");
    float icon_flash_percentage = s_params.get<float>("hud.minimap.icon_flash_percentage");
    
    float delay;
    if (mask == NODE_MASK_INVISIBLE)
    {
        delay = icon_flash_period * icon_flash_percentage;
        icon->transform_->setNodeMask(NODE_MASK_VISIBLE);
    } else
    {
        delay = icon_flash_period * (1-icon_flash_percentage);
        icon->transform_->setNodeMask(NODE_MASK_INVISIBLE);
    }

    s_scheduler.addEvent(SingleEventCallback(this, &Minimap::flashIcon),
                         delay,
                         icon,
                         "Icon Flash",
                         &icon->fp_group_);   
}

//------------------------------------------------------------------------------
void Minimap::stopFlashingIcon(void * i)
{
    MinimapIcon * icon = (MinimapIcon*)i;

    // Clear flash event
    icon->fp_group_.deregisterAllOfType(TaskFp());
    icon->transform_->setNodeMask(NODE_MASK_VISIBLE); 
}

//------------------------------------------------------------------------------
osg::Matrix Minimap::getIconTransform(const Vector & viewpos, const Matrix & icon_pos,
                                      float map_extents, float icon_size) const
{
    float object_angle = atan2f(icon_pos.getZ().x_,
                                icon_pos.getZ().z_);
    Vector2d object_position = icon_pos.getTranslation();

    Vector2d delta = (viewpos-object_position) / map_extents;
    float dist = delta.length();
    if (dist > 0.5f - icon_size*0.5f)
    {
        delta /= dist;
        delta *= 0.5f - icon_size*0.5f;
    }
        
    osg::Matrix offset;
    offset.makeTranslate(-delta.x_,
                         delta.y_,
                         0.0f );

    osg::Matrix object_rot;
    object_rot.makeRotate(object_angle, osg::Vec3(0,0,1));

    return object_rot * offset;
}
