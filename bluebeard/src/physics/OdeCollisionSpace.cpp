
#include "OdeCollisionSpace.h"

#include "Log.h"
#include "OdeRigidBody.h"
#include "OdeSimulator.h"
#include "OdeCollision.h"

#include "Profiler.h"
#include "RigidBody.h"

#undef min
#undef max

namespace physics
{
    
const unsigned MAX_NUM_CONTACTS = 20;    // PPPP

const float CONTACT_MERGE_THRESHOLD = 0.05f * 0.05f;

const float RAY_OFFSET = 0.001;


//------------------------------------------------------------------------------
bool operator<(const CollisionInfo & i1, const CollisionInfo & i2)
{
    return i1.penetration_ < i2.penetration_;
}
    
    
//------------------------------------------------------------------------------
/**
 *  Just bounces back the collision callback to the originating
 *  OdeCollisionSpace.
 *
 *  \param data A pointer to the calling space.
 *  \param o1 The first geom.
 *  \param o2 The second geom.
 */
void spaceCollideCallback (void * data, dGeomID o1, dGeomID o2)
{
    // Make sure ode bug was patched out...
    assert(dGeomIsEnabled(o1));
    assert(dGeomIsEnabled(o2));

    if (!dGeomIsEnabled(o1) || !dGeomIsEnabled(o2))
    {
        OdeGeom * geom1 = (OdeGeom*)dGeomGetData(o1);
        OdeGeom * geom2 = (OdeGeom*)dGeomGetData(o2);
        
        s_log << Log::error
              << "Non-enabled geoms in spaceCollideCallback: "
              << geom1->getName()
              << ": " << dGeomIsEnabled(o1)
              << ", "
              << geom2->getName()
              << ": " << dGeomIsEnabled(o2)
              << "\n";

        GameObject * go1 = (GameObject*)geom1->getUserData();
        GameObject * go2 = (GameObject*)geom2->getUserData();

        if (go1) s_log << *go1; else s_log << "NULL";
        s_log << ", ";
        if (go2) s_log << *go2; else s_log << "NULL";
        s_log << "\n";

        return;
    }
    
    physics::OdeCollisionSpace * coll_space = (physics::OdeCollisionSpace*)data;
    coll_space->spaceCollideCallback(o1, o2);
}


//------------------------------------------------------------------------------
/**
 *  \param generate_start_stop_events If false, only CT_IN_PROGRESS
 *  events will be generated. If true, geoms that are going to be
 *  deleted must be removed by a call to removeGeom beforehand in
 *  order to avoid stop events for deleted geoms.
 */
OdeCollisionSpace::OdeCollisionSpace(const std::string & name, bool generate_start_stop_events) :
    name_(name),
    remember_disabled_geoms_(false),
    generate_start_stop_events_(generate_start_stop_events),
    space_id_(dHashSpaceCreate(0)),
    is_quadtree_(false)
{
}


//------------------------------------------------------------------------------
OdeCollisionSpace::~OdeCollisionSpace()
{
    s_log << Log::debug('d')
          << "OdeCollisionSpace destructor\n";
    
    dSpaceDestroy(space_id_);
}


//------------------------------------------------------------------------------
/**
 *  \param other_space If NULL, collide against self.
 */
void OdeCollisionSpace::collide(OdeCollisionSpace * other_space)
{
    PROFILE(OdeCollisionSpace::collide(other_space));
    
    assert(cur_colliding_geoms_.empty());
    assert(!remember_disabled_geoms_ &&
           "Nested call of OdeCollisionSpace::collide()!");

    // Don't allow nested collide() calls for now, only collide(const
    // OdeGeom * geom, CollisionCallback callback) and
    // collideRayMultiple(OdeRayGeom * ray, CollisionCallback
    // callback) calls.
    assert(potentially_colliding_geoms_.empty());
    
    potentially_colliding_geoms_.push(std::vector<std::pair<dGeomID, dGeomID> >());

    if (other_space)
    {
        PROFILE(dSpaceCollide2);
        dSpaceCollide2((dGeomID)space_id_, (dGeomID)other_space->space_id_, this, &physics::spaceCollideCallback);
    } else
    {
        PROFILE(dSpaceCollide);
        dSpaceCollide(space_id_, this, &physics::spaceCollideCallback);
    }

    handlePotentialCollisions();

    if (generate_start_stop_events_) checkForStoppedCollisions();

    disabled_geom_.clear();    
}
    

//------------------------------------------------------------------------------
/**
 *  Handle only collision notifications for this geom, and don't keep
 *  track of stopped collisions. This makes it possible to collide a
 *  single geom against a space without letting the contained objects
 *  know it.
 */
void OdeCollisionSpace::collide(const OdeGeom * geom, CollisionCallback callback)
{
    potentially_colliding_geoms_.push(std::vector<std::pair<dGeomID, dGeomID> >());

    PROFILE(OdeCollisionSpace::collide2);
    
    dSpaceCollide2((dGeomID)space_id_,
                   geom->getId(),
                   this, &physics::spaceCollideCallback);

    handlePotentialCollisionsSingle(geom, callback);
}


//------------------------------------------------------------------------------
/**
 *  Generates collision contacts for all intersection points of the
 *  ray, sorted by distance to starting point.
 */
void OdeCollisionSpace::collideRayMultiple(OdeRayGeom * ray,
                                           CollisionCallback callback)
{
    // First find all potentially colliding geoms.
    potentially_colliding_geoms_.push(std::vector<std::pair<dGeomID, dGeomID> >());

    PROFILE(OdeCollisionSpace::collideRay);
    
    dSpaceCollide2((dGeomID)space_id_,
                   ray->getId(),
                   this, &physics::spaceCollideCallback);

    dContactGeom contact_geom;
    CollisionInfo info;

    Vector ray_pos, ray_dir;    
    ray->get(ray_pos, ray_dir);
    float ray_length = ray->getLength();
    
    std::set<CollisionInfo> collision_point;
    
    for (unsigned c=0; c<potentially_colliding_geoms_.top().size(); ++c)
    {
        dGeomID o1 = potentially_colliding_geoms_.top()[c].first;
        dGeomID o2 = potentially_colliding_geoms_.top()[c].second;

        OdeGeom * geom1 = (OdeGeom*)dGeomGetData(o1);
        OdeGeom * geom2 = (OdeGeom*)dGeomGetData(o2);

        if (geom1 != ray)
        {
            std::swap(geom1, geom2);
            std::swap(o1,o2);
        } 
        assert(geom1 == ray);

        // Now find *all* collisions of the ray with the current other
        // geom
        float penetration_offset = 0.0f; // starting point of ray is
                                         // changed, so penetration
                                         // must be corrected
        while (dCollide(o1, o2, 1, &contact_geom, sizeof(dContactGeom)))
        {
            info.this_geom_  = geom1;
            info.other_geom_ = geom2;
            info.pos_  = Vector(contact_geom.pos[0],
                                contact_geom.pos[1],
                                contact_geom.pos[2]);
            info.n_    = Vector(contact_geom.normal[0],
                                contact_geom.normal[1],
                                contact_geom.normal[2]);
            info.penetration_ = contact_geom.depth + penetration_offset;
            info.type_ = CT_SINGLE;

            collision_point.insert(info);

            // Advance the ray origin to the current intersection
            // point, reduce length accordingly
            assert(contact_geom.depth < ray->getLength());
            ray->setLength(ray->getLength() - contact_geom.depth);
            ray->set(info.pos_ + RAY_OFFSET * ray_dir, ray_dir);

            penetration_offset += contact_geom.depth + RAY_OFFSET;

            if (geom2->getType() == GT_HEIGHTFIELD) break; // ODE 0.9
        }

        // Reset original ray properties to test next geom / leave ray
        // unchanged after return
        ray->set(ray_pos, ray_dir);
        ray->setLength(ray_length);
    }

    potentially_colliding_geoms_.pop();
    
    // Now we can call the callback with the sorted points
    for (std::set<CollisionInfo>::iterator it = collision_point.begin();
        it != collision_point.end();
        ++it)
    {
        callback(*it);
    }
}


//------------------------------------------------------------------------------
/**
 *  Generates a CT_STOP event if the geom is currently in contact with
 *  another geom. Cannot rely on cur_colliding_geoms_ /
 *  prev_colliding_geoms_ mechanism because the geom / its body will
 *  be deleted.
 */
void OdeCollisionSpace::disableGeom(const OdeGeom * body_geom)
{
    assert(std::find(disabled_geom_.begin(), disabled_geom_.end(), body_geom->getId()) == disabled_geom_.end());

    if (remember_disabled_geoms_)
    {
        disabled_geom_.push_back(body_geom->getId());
    }
    
    if (!generate_start_stop_events_) return;

    // A collision stop event has to be generated if geom is either in
    // cur_colliding_geoms_ or prev_colliding_geoms_. The former can
    // be the case if the deletion is triggered from a collision event
    // handler and a collision with the geom to be deleted has already
    // been handled.

    bool stop_event_generated = false;
    std::set<std::pair<OdeGeom*, OdeGeom*> > * cur_set;
    std::set<std::pair<OdeGeom*, OdeGeom*> >::iterator colliding_geom;

    for (unsigned i=0; i<2; ++i)
    {
        if (i==0) cur_set = &prev_colliding_geoms_;
        else      cur_set = &cur_colliding_geoms_;

        colliding_geom = cur_set->begin();
        while (colliding_geom != cur_set->end())
        {
            if (colliding_geom->first  == body_geom ||
                colliding_geom->second == body_geom)
            {
                if (!stop_event_generated)
                {
                    generateStoppedCollisionEvent(colliding_geom->first,
                                                  colliding_geom->second);
                    stop_event_generated = true;
                }

                // post-increment is well defined in this case
                cur_set->erase(colliding_geom++);
            } else ++colliding_geom;
        }
    }
}

//------------------------------------------------------------------------------
/**
 *  Called by dSpaceCollide. Simply append the geoms to a list of
 *  potentially colliding geoms. This way, collisions won't be handled
 *  during the callback and it is possible to remove / add geoms in
 *  the handlers.
 */
void OdeCollisionSpace::spaceCollideCallback(dGeomID o1, dGeomID o2)
{
    potentially_colliding_geoms_.top().push_back(std::make_pair(o1, o2));
}



//------------------------------------------------------------------------------
dSpaceID OdeCollisionSpace::getId() const
{
    return space_id_;
}


//------------------------------------------------------------------------------
const std::string & OdeCollisionSpace::getName() const
{
    return name_;
}

    
//------------------------------------------------------------------------------
void OdeCollisionSpace::dumpContents() const
{
    s_log << "\n OdeCollisionSpace \""
          << name_
          << "\" : \n";

    for (int g=0; g<dSpaceGetNumGeoms (space_id_); ++g)
    {
        dGeomID geom_id = dSpaceGetGeom(space_id_, g);

        OdeGeom * geom = (OdeGeom*)dGeomGetData(geom_id);
        assert(geom);

        s_log << *geom
              << " from body ";
        if (geom->getBody())
        {
            if (geom->getBody()->getUserData())
            {
                s_log << *(GameObject*)geom->getBody()->getUserData();
            } else
            {
                s_log << geom->getBody()->getName();
            }
        } else
        {
            s_log << "NULL";
        }
        s_log << "\n";
    }
    
    s_log << "\n";
}


//------------------------------------------------------------------------------
/**
 *  Replaces the current ode space with a quadtree space with the
 *  given properties. The current space must not contain any geoms.
 */
void OdeCollisionSpace::createQuadtreeSpace(const Vector & center, const Vector & extents, unsigned depth)
{
    is_quadtree_ = true;
    
    assert(dSpaceGetNumGeoms(space_id_) == 0);
    
    dSpaceDestroy(space_id_);
    
    dVector3 c,e;

    c[0] = center.x_;
    c[1] = center.y_;
    c[2] = center.z_;

    e[0] = extents.x_;
    e[1] = extents.y_;
    e[2] = extents.z_;
    
    space_id_ = dQuadTreeSpaceCreate(0, c, e, depth);
}
    

//------------------------------------------------------------------------------
/**
 *  Traverse the previously established list of potentially colliding
 *  geoms.
 *
 *  First check if there actually is a collision and continue with the
 *  next pair if not.
 *
 *  Then let the callback functions react to the collision event and
 *  create a contact joint if so desired.
 */
void OdeCollisionSpace::handlePotentialCollisions()
{
    PROFILE(OdeCollisionSpace::handlePotentialCollisions);

    remember_disabled_geoms_ = true;
    
    dContactGeom contact_geom  [MAX_NUM_CONTACTS];
    dContactGeom merged_contact[MAX_NUM_CONTACTS];

//    s_log << "\n\nhandle potential\n";
    
    for (unsigned c=0; c<potentially_colliding_geoms_.top().size(); ++c)
    {
        dGeomID o1 = potentially_colliding_geoms_.top()[c].first;
        dGeomID o2 = potentially_colliding_geoms_.top()[c].second;

        if(!disabled_geom_.empty())
        {
            // Continue if geom was disabled during this function call
            if (std::find(disabled_geom_.begin(), disabled_geom_.end(), o1) != disabled_geom_.end() ||
                std::find(disabled_geom_.begin(), disabled_geom_.end(), o2) != disabled_geom_.end())
            {
                continue;
            }
        }
        
        OdeGeom * geom1 = (OdeGeom*)dGeomGetData(o1);
        OdeGeom * geom2 = (OdeGeom*)dGeomGetData(o2);

        OdeRigidBody * body1 = geom1->getBody();
        OdeRigidBody * body2 = geom2->getBody();

        // This can happen if a geom is detached from its body.
        if (body1 == body2) continue;
        
        // Ignore sleeping geoms which are not sensors
        if (!(geom1->isSensor() || geom2->isSensor()) &&
            (!body1 || body1->isSleeping()) &&
            (!body2 || body2->isSleeping())) continue;

        unsigned num_contacts;
        {
            PROFILE(dCollide);
            num_contacts = dCollide(o1, o2, MAX_NUM_CONTACTS, &contact_geom[0], sizeof(dContactGeom));
        }
        if (num_contacts == 0) continue;

        if (num_contacts == MAX_NUM_CONTACTS)
        {
            s_log << Log::debug('p')
                  << "max number of contacts ("
                  << MAX_NUM_CONTACTS
                  << ") reached.\n";
        }

        // Find deepest penetration. Rays return distance to ray
        // origin instead of penetration, so we must invert the
        // condition.
        unsigned deepest = 0;
        bool ray = (geom1->getType() == GT_RAY ||
                    geom2->getType() == GT_RAY);
        for (unsigned i=1; i<num_contacts; ++i)
        {
            if ((ray &&  contact_geom[i].depth > contact_geom[deepest].depth) ||
                (!ray && contact_geom[i].depth < contact_geom[deepest].depth))
            {
                deepest = i;
            }
        }

        // Bail if no contact joint should be generated. Never
        // generate contact joints for rays.
        if (!handleCollisionEvent(geom1, geom2, contact_geom[deepest]) || ray) continue;
        
        num_contacts = mergeContacts(num_contacts, contact_geom, merged_contact);

        
        dContact contact;
        memset(&contact, 0, sizeof(dContact));

        // ---------- acquire material properties to use for this collision ----------
        const Material & mat1 = geom1->getMaterial();
        const Material & mat2 = geom2->getMaterial();
        
        contact.surface.mode   = dContactBounce | dContactApprox1;
        contact.surface.mu     = sqrtf ( mat1.friction_   * mat2.friction_ );
        contact.surface.bounce = 0.5f * (mat1.bounciness_ + mat2.bounciness_);
        
        // Now add all contact joints
        for (unsigned c=0; c<num_contacts; ++c)
        {
            assert(!body1 || !body2 || (body1->getSimulator() == body2->getSimulator()));

            // Set collision details (pos, n, penetration)
            contact.geom = merged_contact[c];
            OdeSimulator * sim = body1 ? body1->getSimulator() : body2->getSimulator();
            sim->addContactJoint(contact,
                                 body1 && body1->isStatic() ? NULL : body1,
                                 body2 && body2->isStatic() ? NULL : body2);
        }

    }

    potentially_colliding_geoms_.pop();

    remember_disabled_geoms_ = false;    
}


//------------------------------------------------------------------------------
/**
 *  Collides a single geom against a space without notifying the other
 *  geoms in the space.
 *
 *  \param single_geom The geom which was collided and whose callback
 *  functions should be called.
 */
void OdeCollisionSpace::handlePotentialCollisionsSingle(const OdeGeom * single_geom, CollisionCallback callback)
{
    PROFILE(OdeCollisionSpace::handlePotentialCollisionsSingle);    

    dContactGeom contact_geom;
    CollisionInfo info;

    
    for (unsigned c=0; c<potentially_colliding_geoms_.top().size(); ++c)
    {
        dGeomID o1 = potentially_colliding_geoms_.top()[c].first;
        dGeomID o2 = potentially_colliding_geoms_.top()[c].second;

        OdeGeom * geom1 = (OdeGeom*)dGeomGetData(o1);
        OdeGeom * geom2 = (OdeGeom*)dGeomGetData(o2);

        if (geom1 != single_geom)
        {
            std::swap(o1, o2);
            std::swap(geom1, geom2);
        }
        assert(geom1 == single_geom);

        if (!dCollide(o1, o2, 1, &contact_geom, sizeof(dContactGeom))) continue;

        info.this_geom_  = geom1;
        info.other_geom_ = geom2;
        info.pos_  = Vector(contact_geom.pos[0],
                            contact_geom.pos[1],
                            contact_geom.pos[2]);
        info.n_    = Vector(contact_geom.normal[0],
                            contact_geom.normal[1],
                            contact_geom.normal[2]);
        info.penetration_ = contact_geom.depth;
        info.type_ = CT_SINGLE;

        callback(info);
    }

    potentially_colliding_geoms_.pop();
}


//------------------------------------------------------------------------------
/**
 *  Stores the geoms in the currently colliding geoms list.
 *
 *  \return Whether this is a new collision, that is, the geoms
 *  haven't been colliding in the previous frame.
 */
bool OdeCollisionSpace::addCollidingGeoms(OdeGeom * geom1, OdeGeom * geom2)
{
    assert(generate_start_stop_events_);
    
    if (geom1 > geom2) std::swap(geom1, geom2);

    // geoms shouldn't collide twice in same frame.
    if (cur_colliding_geoms_.find(std::make_pair(geom1, geom2)) != cur_colliding_geoms_.end())
    {
        s_log << Log::error
              << Log::millis
              << geom1->getName()
              << " and "
              << geom2->getName()
              << " added to cur_colliding_geoms_ twice.\n";

        s_log << "potentially_colliding_geoms_ has size "
              << potentially_colliding_geoms_.size()
              << "\n";

        s_log << "new collision: "
              << (prev_colliding_geoms_.find(std::make_pair(geom1, geom2)) == prev_colliding_geoms_.end())
              << "\n";

        s_log << "\n";

        s_log.setDebugClasses(s_log.getDebugClasses() + "p");
    } else
    {
        cur_colliding_geoms_.insert(std::make_pair(geom1, geom2));
    }

    return prev_colliding_geoms_.find(std::make_pair(geom1, geom2)) == prev_colliding_geoms_.end();
}


//------------------------------------------------------------------------------
/**
 *  Check whether geoms have been colliding before and notify their
 *  callback functions.
 *
 *  \return Whether contact joints should be generated for this
 *  collision.
 */
bool OdeCollisionSpace::handleCollisionEvent(OdeGeom * geom1, OdeGeom * geom2, const dContactGeom & contact_geom) 
{
    PROFILE(OdeCollisionSpace::handleCollisionEvent);
    
    OdeRigidBody * body1 = geom1->getBody();
    OdeRigidBody * body2 = geom2->getBody();
    
    if (body1 && body1->isStatic() &&
        body2 && body2->isStatic() &&
        !geom1->isSensor() && !geom2->isSensor())
    {
        // Warn if bodies are static non sensors - this just wastes CPU power
        s_log << Log::warning
              << "Colliding static geoms "
              << geom1->getName()
              << ", "
              << geom2->getName()
              << " with categories "
              << geom1->getCategory()
              << " and "
              << geom2->getCategory()
              << "\n";
        return false;
    }

    // If there are no collision event handlers set, per default
    // sensors don't collide with anything.
    if (!geom1->getCollisionCallback() && !geom2->getCollisionCallback())
    {
        return !(geom1->isSensor() || geom2->isSensor());
    }

    // Now check whether the geoms have been colliding last frame.
    bool new_collision = generate_start_stop_events_ ? addCollidingGeoms(geom1, geom2) : false;

    
    // Notify callbacks
    bool generate_joints = true;
    
    CollisionInfo info;

    info.this_geom_  = geom1;
    info.other_geom_ = geom2;

    info.pos_  = Vector(contact_geom.pos[0],
                        contact_geom.pos[1],
                        contact_geom.pos[2]);
    info.n_    = Vector(contact_geom.normal[0],
                        contact_geom.normal[1],
                        contact_geom.normal[2]);
    info.penetration_ = contact_geom.depth;

    info.type_ = new_collision ? CT_START : CT_IN_PROGRESS;

//     s_log << Log::debug('p')
//           << (new_collision ? std::string("CT_START ") : std::string("CT_IN_PROGRESS "))
//           << " for "
//           << geom1->getName()
//           << " and "
//           << geom2->getName()
//           << "\n";

    if (geom1->getCollisionCallback())
    {
        generate_joints &= (*geom1->getCollisionCallback())(info);
    }

    if (geom2->getCollisionCallback())
    {
        info.n_ *= -1;
        std::swap(info.this_geom_, info.other_geom_);
            
        if ((generate_joints != (*geom2->getCollisionCallback())(info)))
        {
            if (geom1->getCollisionCallback())
            {
                s_log << Log::debug('p')
                      << "Conflicting values for generate_joints while colliding "
                      << geom1->getName()
                      << " and "
                      << geom2->getName()
                      << ". Ignoring collision.\n";
            }
            generate_joints = false;
        }
    }

    return generate_joints;
}



//------------------------------------------------------------------------------
void OdeCollisionSpace::generateStoppedCollisionEvent(OdeGeom * geom1, OdeGeom * geom2)
{
    assert (generate_start_stop_events_);
    
    if (!geom1->getCollisionCallback() && !geom2->getCollisionCallback()) return;

    CollisionInfo info;

    info.this_geom_  = geom1;
    info.other_geom_ = geom2;

    info.pos_         = Vector(0,0,0);
    info.n_           = Vector(0,0,0);
    info.penetration_ = 0.0f;

    info.type_ = CT_STOP;

//     s_log << Log::debug('p')
//           << "CT_STOP for "
//           << geom1->getName()
//           << " and "
//           << geom2->getName()
//           << "\n";    
    
    if (geom1->getCollisionCallback())
    {
        (*geom1->getCollisionCallback())(info);
    }


    if (geom2->getCollisionCallback())
    {
        std::swap(info.this_geom_, info.other_geom_);
        (*geom2->getCollisionCallback())(info);
    }
}


//------------------------------------------------------------------------------
/**
 *  Traverse all previously collided geoms and check whether they are
 *  still colliding. If not, generate a "collision stopped"
 *  event. Finally, put cur to prev collisions.
 */
void OdeCollisionSpace::checkForStoppedCollisions()
{
    assert(generate_start_stop_events_);
    
    PROFILE(OdeCollisionSpace::checkForStoppedCollisions);
    
    for (std::set<std::pair<OdeGeom*, OdeGeom*> >::iterator it = prev_colliding_geoms_.begin();
        it != prev_colliding_geoms_.end();
        ++it)
    {
        if (cur_colliding_geoms_.find(*it) == cur_colliding_geoms_.end())
        {
            generateStoppedCollisionEvent(it->first, it->second);
        }
    }

    std::swap(cur_colliding_geoms_, prev_colliding_geoms_);
    cur_colliding_geoms_.clear();
}


//------------------------------------------------------------------------------
/**
 *  Merges contact points which are closer than a threshold. Geoms for
 *  merged contacts are set to 0 in 'in'.
 *
 *  \param num_contacts The number of valid contacts in 'in'
 *  \param in The contacts to merge.
 *  \param out The result of the merge. in.size() == out.size() must hold.
 *  \return The number of contacts in 'out'.
 */
unsigned OdeCollisionSpace::mergeContacts(unsigned num_contacts,
                                          dContactGeom * in,
                                          dContactGeom * out)
{
    unsigned num_merged = 0;
    
    for (int i=0; i<(int)num_contacts; ++i)
    {
        if (in[i].g1 == 0) continue; // Contact was used in a previous merge

        unsigned cur_num_merged = 1;
        for (int j=i+1; j<(int)num_contacts; ++j)
        {
            if (in[j].g1 == 0) continue; // Contact was used in a previous merge
            
            assert(in[i].g1 == in[j].g1);
            assert(in[i].g2 == in[j].g2);

            dVector3 diff;
            dOP(diff, -, in[i].pos, in[j].pos);
            
            if (dLENGTHSQUARED(diff) < CONTACT_MERGE_THRESHOLD)
            {
                in[j].g1 = 0;

                ++cur_num_merged;

                dOPE(in[i].pos,    +=, in[j].pos);
                dOPE(in[i].normal, +=, in[j].normal);

                in[i].depth += in[j].depth;
            }
        }

        if (cur_num_merged != 1)
        {
            // calc average, renormalize normal
            float inv = 1.0f / cur_num_merged;

            dOPEC(in[i].pos, *=, inv);
            in[i].depth      *=  inv;

            dReal l_sqr = dLENGTHSQUARED(in[i].normal);
            if (equalsZero(l_sqr)) continue;

            dOPEC(in[i].normal, /=, sqrt(l_sqr));
        }
        
        
        out[num_merged++] = in[i];
    }
    
    return num_merged;
}


} // namespace physics
