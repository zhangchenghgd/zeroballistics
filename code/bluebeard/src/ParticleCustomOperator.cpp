
#include "ParticleCustomOperator.h"


#include "UtilsOsg.h"

//------------------------------------------------------------------------------
void AlignWithVelocityVectorOperator::operate(Particle* p, double dt)
{
    osg::Vec3 dir = p->getVelocity();

    if (equalsZero(dir.length())) return;

    Vector part_dir_in_cam = s_scene_manager.getCamera().getTransform().transformVectorInv(vecOsg2Gl(dir));
    Vector pos_in_cam      = s_scene_manager.getCamera().getTransform().transformPointInv (vecOsg2Gl(p->getPosition()));
            
    Matrix rot(true);
    rot.loadOrientation(part_dir_in_cam, -pos_in_cam);

    // The particle quad is drawn by osg in the xy
    // plane. loadOrientation aligns the negative z-coordinate
    // with the given direction and the y--axis with the up
    // vector. Therefore we first have to rotate the particle
    // by 90 deg around the x axis so it lies in the
    // xz--plane. Manually rotate the matrix for
    // efficiency.
    // synonymous:
    //            Matrix align_xz(true);
    //            align_xz.loadCanonicalRotation(PI/2, 0); 
    //            rot = rot * align_xz;
    std::swap(rot.getY(), rot.getZ());
    rot.getY() *= -1;

    // Unfortunately, we cannot directly specify the matrix to
    // osg, so calculate the euler values that will result in
    // our matrix in the praticle render function.
    osg::Vec3 euler;
    rot.getEuler(euler.x(),euler.y(),euler.z());

    p->setAngle(euler);   
}




//------------------------------------------------------------------------------
/**
 *  Apply delta transform to particle position & velocity.
 */
void LocalEffectOperator::operate(Particle* p, double dt)
{
    p->setPosition(transform_.preMult(p->getPosition()));
    p->setVelocity(osg::Matrix::transform3x3(p->getVelocity(), transform_));
}



//------------------------------------------------------------------------------
/**
 *  Calculate the delta transform from previous to current position.
 */
void LocalEffectOperator::beginOperate (Program * program)
{
    transform_ = program->getPreviousWorldToLocalMatrix();
    transform_.postMult(program->getLocalToWorldMatrix());
}
