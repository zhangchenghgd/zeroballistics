
#ifndef BLUEBEARD_SOUNDSOURCECALLBACKS_INCLUDED
#define BLUEBEARD_SOUNDSOURCECALLBACKS_INCLUDED

#include "RigidBody.h"
#include "RegisteredFpGroup.h"



//------------------------------------------------------------------------------
/**
 *  Used to cull sound sources based on distance to listener
 */
class CullDistantSource : public osg::NodeCallback
{
public:
    CullDistantSource()
        {
            cull_distance_ = s_params.get<float>("client.sound.cull_distance");
        }

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        // first update subgraph
        traverse(node,nv);

        SoundSource * snd_src = (SoundSource*)node;

        // check if sound manager wants sounds enabled or not
        if(s_soundmanager.isSoundEnabled()) 
        {
            // check if sound should be enabled based on distance
            if(Vector(s_soundmanager.getListenerInfo().position_ - snd_src->getPosition()).length() >
               cull_distance_)
            {
                snd_src->enable(false);
            }
            else
            {
                snd_src->enable(true);

                /// handle looping sounds different, so that they try to fetch a sound source
                /// again even if there wasn't one available yet.
                if(snd_src->isLooping())
                {
                    if(snd_src->getId())
                    {
                        // we got a sound source everything is ok
                        snd_src->setWaitingForSource(false);
                    }
                    else
                    {
                        // we haven't got a source, set this to waiting
                        // so the snd_src doesn't get deleted and tries to get a free source
                        // next frame
                        snd_src->setWaitingForSource(true);
                    }
                }
            }
        }    
        else // disable all sounds if soundmanager says sounds disabled
        {
                snd_src->enable(false);
        }
    }

 protected:
    float cull_distance_;
};

//------------------------------------------------------------------------------
/**
 *  This is a predefined callback class that deletes a SoundSource after
 *  it stopped playing.
 */
class DeleteSoundSourceAfterPlaying : public osg::NodeCallback
{
public:
    DeleteSoundSourceAfterPlaying() {}

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        // first update subgraph
        traverse(node,nv);
    
        SoundSource * snd_src = (SoundSource*)node;

        // sound sources waiting for a free source must not be deleted!
        if(snd_src->isWaitingForSource()) return;

        // delete finished and invalid sound sources
        if(snd_src->isFinishedPlaying())
        {
            s_scene_manager.scheduleNodeForDeletion(node);
        }        
    }
};




//------------------------------------------------------------------------------
/**
 *  Always places a sound source at the listener's position.
 */
class SoundSourceListenerPositionUpdater : public osg::NodeCallback
{
public:

    virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
    {
        // first update subgraph
        traverse(node,nv);
    
        SoundSource * snd_src = (SoundSource*)node;

        snd_src->setPosition(s_soundmanager.getListenerInfo().position_);
    }
};

//------------------------------------------------------------------------------
/**
 *  This is a predefined callback class that updates Position and Velocity
 *  of a SoundSource according to a RigidBody provided to this callback 
 *  (i.e. the parent node)
 */
class SoundSourcePositionAndVelocityUpdater : public osg::NodeCallback
{
public:
    SoundSourcePositionAndVelocityUpdater(RigidBody * body, bool update_velocity = true) :
        body_(body),
        update_velocity_(update_velocity)
    {
        body->addObserver(ObserverCallbackFun0(this, &SoundSourcePositionAndVelocityUpdater::onBodyDeleted),
                          GOE_SCHEDULED_FOR_DELETION,
                          &fp_group_);
    }

    virtual void operator()(osg::Node * node, osg::NodeVisitor * nv)
    {
        // first update subgraph
        traverse(node,nv);

        if (body_)
        {
            SoundSource * snd_src = (SoundSource*)node;
            snd_src->setPosition(body_->getTransform().getTranslation() );
            if (update_velocity_) snd_src->setVelocity(body_->getGlobalLinearVel());
        }
    }

private:

    void onBodyDeleted()
        {
            body_ = NULL;
        }
    
    const RigidBody * body_;
    bool update_velocity_;
    
    RegisteredFpGroup fp_group_;
};

//------------------------------------------------------------------------------
/**
 *  This is a predefined callback class that updates Position and Pitch
 *  of a SoundSource according to a RigidBody provided to this callback,
 *  primary use is to update an engine sound
 *  (i.e. the parent node)
 */
class SoundSourcePositionAndPitchUpdater : public osg::NodeCallback
{
public:
    SoundSourcePositionAndPitchUpdater(RigidBody * body, bool update_position = true) :
        body_(body),
        update_pos_(update_position)
    {
        body->addObserver(ObserverCallbackFun0(this, &SoundSourcePositionAndPitchUpdater::onBodyDeleted),
                          GOE_SCHEDULED_FOR_DELETION,
                          &fp_group_);
    }

    virtual void operator()(osg::Node * node, osg::NodeVisitor * nv)
    {
        // first update subgraph
        traverse(node,nv);

        if (body_)
        {
            SoundSource * snd_src = (SoundSource*)node;

            if(update_pos_) snd_src->setPosition(body_->getTransform().getTranslation());

            ADD_STATIC_CONSOLE_VAR(float,engine_factor,0.08);
            float freq = 0.5 + (engine_factor * body_->getGlobalLinearVel().length());

            snd_src->setPitch(freq);
        }
    }

private:

    void onBodyDeleted()
        {
            body_ = NULL;
        }

    const RigidBody * body_;
    bool update_pos_;
    RegisteredFpGroup fp_group_;
};








#endif // #ifndef BLUEBEARD_SOUNDSOURCECALLBACKS_INCLUDED
