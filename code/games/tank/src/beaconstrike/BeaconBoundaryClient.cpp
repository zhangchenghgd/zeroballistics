

#include "BeaconBoundaryClient.h"




#include <osg/State>
#include <osg/CullFace>
#include <osg/BlendFunc>
#include <osg/Depth>
#include <osg/AlphaFunc>
#include <osg/Texture2D>
#include <osg/TexEnv>
#include <osg/Geometry>
#include <osg/TexMat>
#include <osg/NodeCallback>
#include <osg/Group>

#include <osg/LineWidth>



#include "physics/OdeCollisionSpace.h"


#include "Log.h"
#include "Vector.h"
#include "BeaconBoundary.h"
#include "Beacon.h"
#include "Profiler.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "UtilsOsg.h"
#include "ReaderWriterBbm.h"
#include "OsgNodeWrapper.h"


#undef min
#undef max


const float RAY_LENGTH = 1000;

const float VERTS_PER_UNIT = 1.0;

const float SEGMENT_NORMAL_THRESHOLD = 0.15;

const std::string ENERGY_RAY_MODEL = "energy_ray";

const Color BOUNDARY_OUTLINE_COLOR = Color(1,1,0,0.5);

///< Segments spanning a greater angle than indicated here are split
///in order to reduce z-sorting issues.
const float MAXIMUM_INTERVAL_SIZE = PI/4.0f;

//------------------------------------------------------------------------------
/**
 *  We need to know for every segment whether it has a neighboring
 *  segment from a different beacon or not to determine whether to
 *  alpha-blend its ends.
 */
struct BoundarySegment
{
    BoundarySegment() : blend_start(true), blend_end(true) {}
    
    std::vector<WorldIntersectionPoint> intersection_point_;
    bool blend_start;
    bool blend_end;
};


//------------------------------------------------------------------------------
/**
 *  Updates the tex_scroll uniform.
 */
class BeaconBoundaryClientUpdater : public osg::NodeCallback
{
public:

    BeaconBoundaryClientUpdater(osg::Uniform * tex_scroll_uniform) :
        tex_scroll_uniform_(tex_scroll_uniform) {}
    
    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv)
        {
            traverse(node, nv);
            tex_scroll_uniform_->set((float)nv->getFrameStamp()->getSimulationTime() *
                                     s_params.get<float>("client.border.border_pan_speed"));
        }

protected:
    osg::ref_ptr<osg::Uniform> tex_scroll_uniform_;
};



//------------------------------------------------------------------------------
/**
 *  Updates the tex matrix.
 */
class BeaconConnectionUpdater : public osg::NodeCallback
{
public:

    BeaconConnectionUpdater() :
        last_time_(0) {}
    
    virtual void operator() (osg::Node *node, osg::NodeVisitor *nv)
        {
            traverse(node, nv);
            
            osg::TexMat * tex_mat_attrib =
                (osg::TexMat*)node->getOrCreateStateSet()->getTextureAttribute(0, osg::StateAttribute::TEXMAT);
            assert(tex_mat_attrib);


            double cur_time = nv->getFrameStamp()->getSimulationTime();
            double delta_time = cur_time - last_time_;

            double & cur_val = tex_mat_attrib->getMatrix().ptr()[3*4];
//            double & scale = tex_mat_attrib->getMatrix().ptr()[0];
            cur_val += delta_time * s_params.get<float>("client.border.connection_pan_speed");// * scale;

            last_time_ = cur_time;
        }

protected:
    float last_time_;
};



//------------------------------------------------------------------------------
BeaconBoundaryClient::BeaconBoundaryClient(physics::OdeCollisionSpace * world_space,
                                           const std::string & tex_file) :
    BeaconBoundary(world_space),
    update_outline_(true),
    draw_los_hints_(false)
{
    s_log << Log::debug('i')
          << "BeaconBoundaryClient constructor\n";

    geode_ = new osg::Geode();    
    geode_->setName("Beacon Boundary");
    
    connection_group_ = new osg::Group();
    connection_group_->setName("Beacon Connections");




    // Minimap Outline Stuff
    outline_geode_ = new osg::Geode();
    outline_geode_->setName("Beacon Boundary Outline");


    
    initOsgStates(tex_file);
    
    s_scene_manager.addNode(geode_.get());
    s_scene_manager.addNode(connection_group_.get());
}


//------------------------------------------------------------------------------
BeaconBoundaryClient::~BeaconBoundaryClient()
{
    s_log << Log::debug('d')
          << "BeaconBoundaryClient destructor\n";

    DeleteNodeVisitor delete_visitor(geode_.get());
    s_scene_manager.getRootNode()->accept(delete_visitor);

    DeleteNodeVisitor delete_visitor2(connection_group_.get());
    s_scene_manager.getRootNode()->accept(delete_visitor2);
}


//------------------------------------------------------------------------------
/**
 *  Whether to colorize differntly portions of boundary which are not
 *  in LOS from beacon.
 */
void BeaconBoundaryClient::setDrawLosHints(bool b)
{
    draw_los_hints_ = b;
}

//------------------------------------------------------------------------------
void BeaconBoundaryClient::setUpdateOutline(bool b)
{
    if (b == update_outline_) return;
    
    update_outline_ = b;

    // dirty all deployed beacons so outline will be updated
    for (BeaconContainer::iterator it =  beacon_.begin();
         it != beacon_.end();
         ++it)
    {
        if ((*it)->isDeployed()) newly_deployed_beacon_.insert(*it);
    }
    update();
}


//------------------------------------------------------------------------------
void BeaconBoundaryClient::update()
{
    PROFILE(BeaconBoundaryClient::update);
    
    if (newly_deployed_beacon_.empty() &&
        deactivated_beacon_.empty()) return;
    
    s_log << Log::debug('n')
          << "BeaconBoundaryClient::update\n";

    // update neighborhood information for newly deployed beacons
    for (std::set<Beacon*>::iterator it = newly_deployed_beacon_.begin();
         it != newly_deployed_beacon_.end();
         ++it)
    {
        beacon_space_->collide((*it)->getRadiusGeom(),
                               physics::CollisionCallback(this, &BeaconBoundaryClient::beaconRadiusCollisionCallback));
    }

    
    s_log << Log::debug('B')
          << "removing deleted\n";

    // remove geometry for deleted beacons, mark their neighbors for
    // update
    for (unsigned i=0; i<deactivated_beacon_.size(); ++i)
    {
        clearGeometry(deactivated_beacon_[i]);

        for (unsigned n=0; n<deactivated_beacon_[i].neighbor_.size(); ++n)
        {
            dirty_beacon_.insert(deactivated_beacon_[i].neighbor_[n]);
        }
    }
    deactivated_beacon_.clear();


    // mark newly deployed beacons as dirty
    for (std::set<Beacon*>::iterator it = newly_deployed_beacon_.begin();
         it != newly_deployed_beacon_.end();
         ++it)
    {
        dirty_beacon_.insert(*it);
    }
    
    s_log << Log::debug('B')
          << "updating dirty\n";

    // Traverse beacons in need of new boundary segments
    for (std::set<Beacon*>::iterator it = dirty_beacon_.begin();
         it != dirty_beacon_.end();
         ++it)
    {
        if (!(*it)->isDeployed()) continue;
        
        s_log << Log::debug('B')
              << "updating "
              << **it
              << ". Neighbors: ";
        std::vector<Beacon*> & neighs = beacon_info_[*it].neighbor_;
        for (unsigned n=0; n<neighs.size(); ++n)
        {
            s_log << *neighs[n]
                  << "; ";
        }
        s_log << "\n";
        

        std::vector<std::pair<float,float> > intervals;
        calculateIntervals(*it, intervals);
        
        std::vector<BoundarySegment> segments;
        std::vector<std::vector<Vector2d> > outline;
        createSegments(*it, intervals, segments, outline);
        createDrawables(*it, segments, outline);
    }
    newly_deployed_beacon_.clear();
    dirty_beacon_.clear();
}

//------------------------------------------------------------------------------
void BeaconBoundaryClient::updateBeaconConnections(std::vector<std::pair<Beacon*, Beacon*> > & connections)
{
    connection_group_->removeChildren(0, connection_group_->getNumChildren());

    
    for (unsigned i=0; i<connections.size(); ++i)
    {
        osg::ref_ptr<osg::Node> ray_model = ReaderWriterBbm::loadModel(ENERGY_RAY_MODEL)->getOsgNode();
        
        if (!ray_model.get())
        {
            s_log << Log::error
                  << "Couldn't load beacon connection model.\n";
            break;
        }

        ray_model->setUserData(NULL); // ignore root node data for now...
        
        assert(ray_model->asTransform());
        osg::ref_ptr<osg::MatrixTransform> transform = ray_model->asTransform()->asMatrixTransform();
        assert(transform.get());

        
        Beacon * b1 = connections[i].first;
        Beacon * b2 = connections[i].second;

        Vector dir = b2->getPosition() - b1->getPosition();
        float len = dir.length();
        if (equalsZero(len)) continue;
    
        Vector pos = (b1->getPosition() + b2->getPosition()) * 0.5;


        Matrix mat(true);
        mat.loadOrientation(dir, Vector(0,1,0));
        mat.getTranslation() = pos;        

        Matrix scale(true);
        scale.scale(1,1,len);
        
        transform->setMatrix(matGl2Osg(mat * scale));


        // ---------- Texture Matrix ----------
        osg::ref_ptr<osg::TexMat> tex_mat = new osg::TexMat;
        tex_mat->getMatrix().makeScale(len,1,1);        
        ray_model->getOrCreateStateSet()->setTextureAttribute(0, tex_mat.get());


        ray_model->setUpdateCallback(new BeaconConnectionUpdater);
        
        connection_group_->addChild(ray_model.get());
    }
}


//------------------------------------------------------------------------------
osg::Node * BeaconBoundaryClient::getOutlineNode() const
{
    return outline_geode_.get();
}


//------------------------------------------------------------------------------
void BeaconBoundaryClient::onBeaconAdded(Beacon * beacon)
{
    beacon->addObserver(ObserverCallbackFun2(this, &BeaconBoundaryClient::onBeaconDeployedChanged),
                        BE_DEPLOYED_CHANGED, &fp_group_);
    
    beacon_info_[beacon] = BeaconInfo();

    // Fixed beacons are deployed on addition...
    if (beacon->isDeployed())
    {
        newly_deployed_beacon_.insert(beacon);
    }
}

//------------------------------------------------------------------------------
/**
 *  Transfers the beacon info to the "deleted" list
 */
void BeaconBoundaryClient::onBeaconDeleted(Beacon * beacon)
{
    assert(beacon_info_.find(beacon) != beacon_info_.end());

    fp_group_.deregister(ObserverFp(&fp_group_, beacon, BE_DEPLOYED_CHANGED));
    
    removeBeacon(beacon);
    
    deactivated_beacon_.push_back(beacon_info_[beacon]);
    beacon_info_.erase(beacon_info_.find(beacon));

    
}


//------------------------------------------------------------------------------
void BeaconBoundaryClient::onBeaconDeployedChanged(Observable* observable, unsigned event)
{    
    Beacon * beacon = (Beacon*)observable;
    
    if (beacon->isDeployed())
    {
        newly_deployed_beacon_.insert(beacon);
    } else
    {
        std::map<const Beacon*, BeaconInfo>::iterator it = beacon_info_.find(beacon);
        assert(it != beacon_info_.end());

        removeBeacon(beacon);
        
        deactivated_beacon_.push_back(it->second);
        it->second.geometry_.clear();
        it->second.outline_geometry_.clear();
        it->second.neighbor_.clear();
    }
}




//------------------------------------------------------------------------------
void BeaconBoundaryClient::initOsgStates(const std::string & tex_file)
{
    // -------------------- set boundary states --------------------    
    osg::StateSet * state_set = geode_->getOrCreateStateSet();
    
    state_set->setAttributeAndModes(new osg::CullFace(osg::CullFace::BACK),
                                    osg::StateAttribute::OFF);

    state_set->setAttribute(s_scene_manager.getCachedProgram("border"));
    state_set->setMode(GL_BLEND, osg::StateAttribute::ON);
    state_set->setAttribute(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    state_set->setRenderBinDetails(BN_TRANSPARENT, "DepthSortedBin");
    state_set->setAttribute(new osg::Depth(osg::Depth::LESS, 0,1,false)); // Disable z-buffer writing

    Texture * tex_obj = s_texturemanager.getResource(tex_file);
    osg::Texture2D * texture;
    if (tex_obj)
    {
        texture = tex_obj->getOsgTexture();

        texture->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::LINEAR_MIPMAP_LINEAR);
        texture->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::LINEAR);
        texture->setWrap(osg::Texture2D::WRAP_S, osg::Texture2D::REPEAT);
        texture->setWrap(osg::Texture2D::WRAP_T, osg::Texture2D::CLAMP);        
        state_set->setTextureAttribute(0, texture);
    }

    osg::Uniform * tex_scroll_uniform = new osg::Uniform("tex_scroll", 0.0f);
    state_set->addUniform(tex_scroll_uniform);
    geode_->setUpdateCallback(new BeaconBoundaryClientUpdater(tex_scroll_uniform));

    
    // -------------------- Set outline states --------------------
    state_set = outline_geode_->getOrCreateStateSet();
    state_set->setAttribute(new osg::LineWidth(3));
}



//------------------------------------------------------------------------------
/**
 *  Traverse neighbors, find areas that are covered by those
 *  neighbors. Finally add intervals not covered to the interval list.
 */
void BeaconBoundaryClient::calculateIntervals(const Beacon * beacon,
                                              std::vector<std::pair<float,float> > & intervals)
{
    assert(intervals.empty());
    assert(beacon_info_.find(beacon) != beacon_info_.end());

    const BeaconInfo & info = beacon_info_[beacon];

    std::vector<std::pair<float,float> > covered;

    
    for (unsigned n=0; n<info.neighbor_.size(); ++n)
    {
        const Beacon * cur_neighbor = info.neighbor_[n];
        
        assert(cur_neighbor->isDeployed());
        
        float dist = ((Vector2d)cur_neighbor->getPosition() - (Vector2d)beacon->getPosition()).length();
        assert(dist <= 2*beacon->getRadius());
        assert(dist >= 0.0f);
        float phi = 2.0f*acosf(0.5f * dist / beacon->getRadius());

        float alpha = atan2f(cur_neighbor->getPosition().x_ - beacon->getPosition().x_,
                             cur_neighbor->getPosition().z_ - beacon->getPosition().z_);

        covered.push_back(std::make_pair(alpha-phi*0.5f, alpha+phi*0.5f));

        if (alpha+phi*0.5f > PI)
        {
            covered.push_back(std::make_pair(alpha-phi*0.5f-2.0f*PI, alpha+phi*0.5f-2.0f*PI));
        }
        if (alpha-phi*0.5f < -PI)
        {
            covered.push_back(std::make_pair(alpha-phi*0.5f+2.0f*PI, alpha+phi*0.5f+2.0f*PI));
        }

        sort(covered.begin(), covered.end());
    }
    
    s_log << Log::debug('B')
          << " covered intervals for "
          << *beacon
          << ": \n";
    for (unsigned i=0; i<covered.size(); ++i)
    {
        s_log << Log::debug('B')
              << covered[i].first / PI * 180
              << " - "
              << covered[i].second / PI * 180
              << "\n";
    }

    if (covered.empty())
    {
        intervals.push_back(std::make_pair(0.0f, 2.0f*PI));
    } else
    {
        // Find uncovered intervals. Start with the end of first
        // interval.
        unsigned cur_covered = 1;
        float cur_start = covered[0].second;
        while(true)
        {
            while (cur_covered < covered.size() &&
                   covered[cur_covered].first < cur_start)
            {
                if (covered[cur_covered].second > cur_start)
                {
                    cur_start = covered[cur_covered].second;
                }
                ++cur_covered;
            }

            if (cur_covered < covered.size())
            {
                intervals.push_back(std::make_pair(cur_start, covered[cur_covered].first));
                cur_start = covered[cur_covered].second;
                ++cur_covered;
            } else
            {
                break;
            }
        }

        // handle wraparound
        if (covered.begin()->first > -PI)
        {
            intervals.push_back(std::make_pair(cur_start, covered.begin()->first + 2*PI));
        }
    }


    s_log << Log::debug('B')
          << " free intervals for "
          << *beacon
          << ": \n";
    for (unsigned i=0; i<intervals.size(); ++i)
    {
        s_log << Log::debug('B')
              << intervals[i].first / PI * 180
              << " - "
              << intervals[i].second / PI * 180
              << "\n";
    }    
}


//------------------------------------------------------------------------------
/**
 *  Calculates the boundary segments for the specified beacon.
 *
 *  \param beacon The beacon to handle.
 *  \param intervals [in] The intervals around the beacon not covered by a different beacon.
 *  \param segments [out]
 *  \param outline [out]
 */
void BeaconBoundaryClient::createSegments(const Beacon * beacon,
                                          std::vector<std::pair<float,float> > & intervals,
                                          std::vector<BoundarySegment> & segments,
                                          std::vector<std::vector<Vector2d> > & outline)
{
    physics::OdeRayGeom ray(RAY_LENGTH);


    while (!intervals.empty())
    {
        std::pair<float,float> cur_interval = intervals.back();
        intervals.pop_back();

        float interval_size = cur_interval.second - cur_interval.first;
        assert(interval_size >= 0.0f);


        // Split segment if too long to reduce z-sorting issues
        if (interval_size > MAXIMUM_INTERVAL_SIZE)
        {
            unsigned num_new_intervals = (unsigned)(interval_size / MAXIMUM_INTERVAL_SIZE) + 1;
            float new_interval_size = interval_size / (float)num_new_intervals;
            assert(new_interval_size <= MAXIMUM_INTERVAL_SIZE);
            for (unsigned i=0; i<num_new_intervals; ++i)
            {
                intervals.push_back(std::make_pair(cur_interval.first + new_interval_size* i,
                                                   cur_interval.first + new_interval_size*(i+1)));
            }
            continue;
        }
        
        
        float arc_length = beacon->getRadius() * interval_size;
        unsigned num_verts = std::max((unsigned)ceilf(arc_length * VERTS_PER_UNIT), (unsigned)2);
        float angle_increment = interval_size / (num_verts-1);
        
        std::vector<unsigned> active_segments;

        if (update_outline_)
        {
            outline.push_back(std::vector<Vector2d>());
        }

        float cur_angle = cur_interval.first;
        for (unsigned v=0; v<num_verts; ++v)        
        {
            Vector r_pos;        
            r_pos = beacon->getPosition();
            r_pos.x_ += sinf(cur_angle)*beacon->getRadius();
            r_pos.y_ += RAY_LENGTH * 0.5;
            r_pos.z_ += cosf(cur_angle)*beacon->getRadius();
            ray.set(r_pos, Vector(0,-1,0));

            world_intersection_points_.clear();
            world_space_->collideRayMultiple(&ray, physics::CollisionCallback(
                                                 this, &BeaconBoundaryClient::worldRayCollisionCallback));
        
            updateSegments(active_segments, segments);

            // Mark start/stop of interval as not to be blended
            if (v==0)
            {
                for (unsigned s=0; s<segments.size(); ++s)
                {
                    segments[s].blend_start = false;
                }
            } else if (v==num_verts-1)
            {
                for (unsigned s=0; s<active_segments.size(); ++s)
                {
                    segments[active_segments[s]].blend_end = false;
                }
            }

            if (update_outline_)
            {
                outline.back().push_back(Vector2d(r_pos.x_, r_pos.z_));
            }

            cur_angle += angle_increment;
        }
    }
}




//------------------------------------------------------------------------------
/**
 *  Appends the points from the world_intersection_points_ (all at the
 *  same position but at different heights) to appropriate segments.
 */
void BeaconBoundaryClient::updateSegments(std::vector<unsigned> & active_segments,
                                          std::vector<BoundarySegment> & segments)
{
    unsigned cur_segment = 0;
    unsigned cur_point   = 0;
    std::vector<unsigned> next_active;

    while(cur_segment != active_segments.size() ||
          cur_point   != world_intersection_points_.size())
    {
        if (cur_segment == active_segments.size())
        {
            // No more active segments to put points into - create new segment
            segments.push_back(BoundarySegment());
            segments.back().intersection_point_.push_back(world_intersection_points_[cur_point]);            
            next_active.push_back(segments.size() -1);
            ++cur_point;
            
        } else if (cur_point == world_intersection_points_.size())
        {
            // no intersection points left - don't re-add segment to active list
            ++cur_segment;
            
        } else if (cur_point > 0 &&
                   (world_intersection_points_[cur_point-1].pos_.y_ -
                    world_intersection_points_[cur_point].pos_.y_) < s_params.get<float>("client.border.height"))
        {
            // Drop points too close to neighbor in vertical direction
            ++cur_point;
        } else
        {
            const WorldIntersectionPoint & last_intersection =
                segments[active_segments[cur_segment]].intersection_point_.back();
            const WorldIntersectionPoint & cur_intersection =
                world_intersection_points_[cur_point];

            Vector p1p2 = last_intersection.pos_ - cur_intersection.pos_;
            
            if (abs(vecDot(&p1p2,  &cur_intersection.n_)) < SEGMENT_NORMAL_THRESHOLD &&
                abs(vecDot(&p1p2, &last_intersection.n_)) < SEGMENT_NORMAL_THRESHOLD)
            {
                // intersection point within threshold - add to current segment
                segments[active_segments[cur_segment]].intersection_point_.push_back(cur_intersection);
                next_active.push_back(active_segments[cur_segment]);
                ++cur_segment;
                ++cur_point;
            } else if (last_intersection.pos_.y_ > cur_intersection.pos_.y_)
            {
                // drop segment
                ++cur_segment;
            } else
            {
                // create new segment with point
                segments.push_back(BoundarySegment());
                segments.back().intersection_point_.push_back(cur_intersection);
                next_active.push_back(segments.size() -1);
                ++cur_point;            
            }
        }
    }
    
    next_active.swap(active_segments);
}


//------------------------------------------------------------------------------
void BeaconBoundaryClient::createDrawables(const Beacon * beacon,
                                           const std::vector<BoundarySegment> & segments,
                                           const std::vector<std::vector<Vector2d> > & outline)
{
    assert(beacon_info_.find(beacon) != beacon_info_.end());
    
    BeaconInfo & cur_info = beacon_info_[beacon];
    
    clearGeometry(cur_info);
    
    for (unsigned s=0; s<segments.size(); ++s)
    {
        const std::vector<WorldIntersectionPoint> & cur_points = segments[s].intersection_point_;
        
        if (cur_points.size() < 2) continue;
        
        osg::Geometry * cur_geometry = new osg::Geometry;
        geode_->addDrawable(cur_geometry);
        cur_info.geometry_.push_back(cur_geometry);

        cur_geometry->addPrimitiveSet(new osg::DrawArrays(GL_QUAD_STRIP,0,cur_points.size()*2));

        osg::Vec3Array * vertex = new osg::Vec3Array(cur_points.size()*2);
        cur_geometry->setVertexArray(vertex);

        osg::Vec2Array* texcoord = new osg::Vec2Array(cur_points.size()*2);
        cur_geometry->setTexCoordArray(0,texcoord);
        
        osg::Vec4Array * color = new osg::Vec4Array(cur_points.size()*2);
        cur_geometry->setColorArray(color);
        cur_geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);


        // In order to always have an integer multiple of symbols, we
        // need to know the total length of the segment.
        float total_length = 0.0f;
        for (unsigned v=0; v < cur_points.size()-1; ++v)
        {
            total_length += (cur_points[v+1].pos_ - cur_points[v].pos_).length();
        }
        unsigned num_symbols = (unsigned)ceilf(total_length/s_params.get<float>("client.border.height"));
        float tex_u_increment = (float)num_symbols / total_length;
    
        float tex_u = 0.0f;
        
        for (unsigned v=0; v < cur_points.size(); ++v)
        {
            (*texcoord)[2*v]  .set(tex_u, 1.0f);
            (*texcoord)[2*v+1].set(tex_u, 0.0f);


            // Check whether beacon sees boundary
            float r=1.0f;
            float g=0.0f;
            float b=0.0f;

            Vector check_pos = cur_points[v].pos_;
                
            if (!draw_los_hints_ || checkLos(beacon, check_pos))
            {
                r=1.0f;
                g=1.0f;
                b=1.0f;
            } else
            {
                // check neighbors for los
                for (unsigned n=0; n<cur_info.neighbor_.size(); ++n)
                {
                    assert(cur_info.neighbor_[n]->isDeployed());

                    float dist = ((Vector2d)cur_info.neighbor_[n]->getPosition() - (Vector2d)check_pos).length();

                    // point can lie exactly at intersection point of circumferences -> tolerate EPSILON
                    if ((dist - EPSILON <= beacon->getRadius()) &&
                        checkLos(cur_info.neighbor_[n], check_pos))
                    {
                        r=1.0f;
                        g=1.0f;
                        b=1.0f;
                        break;
                    }
                }
            }

            if ((v==0                   && segments[s].blend_start) ||
                (v==cur_points.size()-1 && segments[s].blend_end))
            {
                (*color)[2*v]  .set(r, g, b, 0);
                (*color)[2*v+1].set(r, g, b, 0);
            } else if ((v==1                   && segments[s].blend_start) ||
                       (v==cur_points.size()-2 && segments[s].blend_end))
            {
                (*color)[2*v]  .set(r, g, b, 0.5f);
                (*color)[2*v+1].set(r, g, b, 1.0f);
            } else
            {
                (*color)[2*v]  .set(r, g, b, 1.0f);
                (*color)[2*v+1].set(r, g, b, 1.0f);
            }
            

            // Find normal pointing upwards and away from terrain
            Vector p1p2;

            if (v==0)
            {
                p1p2 = cur_points[v+1].pos_ - cur_points[v].pos_;
            } else if (v==cur_points.size()-1)
            {
                p1p2 = cur_points[v].pos_ - cur_points[v-1].pos_;
            } else
            {
                p1p2 = cur_points[v+1].pos_ - cur_points[v-1].pos_;
            }

            p1p2.normalize();
            
            Vector axis;

            // - (y x axis)
            axis.x_ = p1p2.z_;
            axis.y_ = 0.0f;
            axis.z_ = -p1p2.x_;

            Matrix rot;
            rot.loadRotationVector(PI*0.5, axis);

            Vector n = rot.transformVector(p1p2);

            float h = s_params.get<float>("client.border.height");
            (*vertex)[2*v]  .set(cur_points[v].pos_.x_ + n.x_ * h,
                                 cur_points[v].pos_.y_ + n.y_ * h,
                                 cur_points[v].pos_.z_ + n.z_ * h);

            (*vertex)[2*v+1].set(cur_points[v].pos_.x_,
                                 cur_points[v].pos_.y_,
                                 cur_points[v].pos_.z_);

            
            if (v != cur_points.size()-1)
            {
                tex_u += (cur_points[v+1].pos_ - cur_points[v].pos_).length() * tex_u_increment;
            }
        }
    }


    // Create outline drawables
    if (update_outline_)
    {
        for (unsigned o=0; o<outline.size(); ++o)
        {
            osg::Geometry * outline_geometry = new osg::Geometry;
            outline_geode_->addDrawable(outline_geometry);
            cur_info.outline_geometry_.push_back(outline_geometry);

            outline_geometry->addPrimitiveSet(new osg::DrawArrays(GL_LINE_STRIP,0,outline[o].size()));

            osg::Vec3Array * vertex = new osg::Vec3Array(outline[o].size());
            outline_geometry->setVertexArray(vertex);

        
            osg::Vec4Array * color = new osg::Vec4Array(1);
            outline_geometry->setColorArray(color);
            outline_geometry->setColorBinding(osg::Geometry::BIND_OVERALL);

            for (unsigned v=0; v<outline[o].size(); ++v)
            {
                (*vertex)[v].set( outline[o][v].x_,
                                 -outline[o][v].y_,
                                  0.0f);
            }

            (*color)[0].set(BOUNDARY_OUTLINE_COLOR.r_,
                            BOUNDARY_OUTLINE_COLOR.g_,
                            BOUNDARY_OUTLINE_COLOR.b_,
                            BOUNDARY_OUTLINE_COLOR.a_);
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  Used to detect contour in world which boundary should follow.
 */
bool BeaconBoundaryClient::worldRayCollisionCallback(const physics::CollisionInfo & info)
{
    assert(info.type_ == physics::CT_SINGLE);

    if (!info.other_geom_->isStatic())         return false;
    if (info.other_geom_->getName() == "void") return false;

    world_intersection_points_.push_back(WorldIntersectionPoint(info.pos_,
                                                                info.n_));
    
    return false;
}

//------------------------------------------------------------------------------
/**
 *  Used to detect neighboring relations between beacons. Beacons are
 *  neighbors if their radii touch, so they potentially affect each
 *  other's boundary segments.
 */
bool BeaconBoundaryClient::beaconRadiusCollisionCallback(const physics::CollisionInfo & info)
{
    Beacon * beacon       = (Beacon*)info.this_geom_ ->getUserData();
    Beacon * other_beacon = (Beacon*)info.other_geom_->getUserData();

    if (!other_beacon->isDeployed()) return false;
    if (other_beacon->getRadiusGeom() != info.other_geom_) return false;

    // Make sure that beacon info exists
    assert(beacon_info_.find(beacon)       != beacon_info_.end());
    assert(beacon_info_.find(other_beacon) != beacon_info_.end());


    // If beacons are already neighboring, bail
    std::vector<Beacon*> & neighbors       = beacon_info_[beacon      ].neighbor_;
    std::vector<Beacon*> & other_neighbors = beacon_info_[other_beacon].neighbor_;
    UNUSED_VARIABLE(other_neighbors);

    // Bail if neighbor relation already exists
    if (find(neighbors.begin(), neighbors.end(), other_beacon) != neighbors.end())
    {
        // make sure relation is symmetrical
        assert(find(other_neighbors.begin(), other_neighbors.end(), beacon) != other_neighbors.end());
        return false;
    } else assert(find(other_neighbors.begin(), other_neighbors.end(), beacon) == other_neighbors.end());

    
    // update neighbor information
    beacon_info_[beacon].      neighbor_.push_back(other_beacon);
    beacon_info_[other_beacon].neighbor_.push_back(beacon);

    dirty_beacon_.insert(other_beacon);
    
    s_log << Log::debug('B')
          << " "
          << *beacon
          << " now is neighbor of "
          << *other_beacon
          << "\n";
    
    return false;
}


//------------------------------------------------------------------------------
/**
 *  Convenience function. Removes b from all its neighbor's neighbor
 *  lists, from newly_deployed_beacon_ list and from any neighbor list
 *  in deactivated_beacon_.
 */
void BeaconBoundaryClient::removeBeacon(Beacon * b)
{
    assert(beacon_info_.find(b) != beacon_info_.end());
    
    BeaconInfo & info = beacon_info_[b];
    for (unsigned n=0; n<info.neighbor_.size(); ++n)
    {
        Beacon * neighbor = info.neighbor_[n];
        assert(beacon_info_.find(neighbor) != beacon_info_.end());
    
        BeaconInfo & neighbors_info = beacon_info_[neighbor];

        std::vector<Beacon*>::iterator it = find(neighbors_info.neighbor_.begin(),
                                                 neighbors_info.neighbor_.end(),
                                                 b);
        
        assert(it != neighbors_info.neighbor_.end());
        neighbors_info.neighbor_.erase(it);

        s_log << Log::debug('B')
              << " removed "
              << *b
              << " from "
              << *neighbor
              << "'s neighbor list.\n";
    }

    // remove from deactivated beacons as well
    for (unsigned d=0; d<deactivated_beacon_.size(); ++d)
    {
        std::vector<Beacon*>::iterator it = find(deactivated_beacon_[d].neighbor_.begin(),
                                                 deactivated_beacon_[d].neighbor_.end(),
                                                 b);

        if (it != deactivated_beacon_[d].neighbor_.end())
        {
            deactivated_beacon_[d].neighbor_.erase(it);
        }
    }

    // Finally, remove from newly_deployed_beacon_
    std::set<Beacon*>::iterator it = newly_deployed_beacon_.find(b);
    if (it != newly_deployed_beacon_.end()) newly_deployed_beacon_.erase(it);
}

//------------------------------------------------------------------------------
/**
 *  Removes all geometry associated with the specified beacon
 *  (boundary and outline).
 */
void BeaconBoundaryClient::clearGeometry(BeaconInfo & info)
{
    bool r;
    for (unsigned g=0; g < info.geometry_.size(); ++g)
    {
        r = geode_->removeDrawable(info.geometry_[g]);
        assert(r);
    }
    info.geometry_.clear();

    for (unsigned g=0; g < info.outline_geometry_.size(); ++g)
    {
        r = outline_geode_->removeDrawable(info.outline_geometry_[g]);
        assert(r);
    }
    info.outline_geometry_.clear();
}



