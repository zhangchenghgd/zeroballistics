
#include "ReaderWriterBbm.h"

#include <osg/MatrixTransform>
#include <osgDB/FileNameUtils>
#include <osgDB/FileUtils>

#include "BbmOsgConverter.h"
#include "BbmImporter.h"
#include "Paths.h"
#include "SceneManager.h"
#include "InstancedGeometry.h"
#include "Profiler.h"

const char * EXTENSION = "bbm";



//------------------------------------------------------------------------------
ModelResource::ModelResource(const std::string & name)
{
    osgDB::ReaderWriter::ReadResult result =
        osgDB::Registry::instance()->readNode(name, NULL);

    if (result.success())
    {
        model_ = dynamic_cast<osg::MatrixTransform*>(result.getNode());
        assert(model_.get());
    } else
    {
        Exception e;
        e << "ReaderWriterBbm::loadModel:"            
          << result.message()
          << "\n";
        throw e;
    }
}

//------------------------------------------------------------------------------
osg::MatrixTransform * ModelResource::getModel() const
{
    return model_.get();
}


//------------------------------------------------------------------------------
ModelManager::ModelManager() : 
    ResourceManager<ModelResource>("models")
{
}


//------------------------------------------------------------------------------
/**
 *  Group activation information mustn't be shared between nodes ->
 *  make deep copy of user data
 */
CloneUserDataVisitor::CloneUserDataVisitor() :
    osg::NodeVisitor(TRAVERSE_ALL_CHILDREN)
{
    setNodeMaskOverride(NODE_MASK_OVERRIDE);
}

//------------------------------------------------------------------------------
void CloneUserDataVisitor::apply(osg::Node & node)
{
    NodeUserData * data = dynamic_cast<NodeUserData*>(node.getUserData());

    if (data) node.setUserData(new NodeUserData(*data));
    
    traverse(node);
}


//------------------------------------------------------------------------------
bool ReaderWriterBbm::acceptsExtension(const std::string& extension) const
{ 
    return osgDB::equalCaseInsensitive( extension, EXTENSION );
}


//------------------------------------------------------------------------------
osgDB::ReaderWriter::ReadResult ReaderWriterBbm::readNode(const std::string & name,
                                                          const osgDB::ReaderWriter::Options* options) const
{
    std::string filename( osgDB::findDataFile( name, options ) );
    if( filename.empty() )
    {
        osgDB::ReaderWriter::ReadResult ret(ReadResult::FILE_NOT_FOUND);
        ret.message() = "Could not find " + name;
        return ret;
    }

    s_log << Log::debug('r') << "ReaderWriterBbm::readNode( \"" << filename << "\" )\n";


    bbm::Node * bbm_root = bbm::Node::loadFromFile(filename);    

    BbmOsgVisitor v;
    bbm_root->accept(v);
    

    if (bbm_root->getCreator() == bbm::BC_BLENDER)
    {
#ifdef ENABLE_DEV_FEATURES
        if (s_params.get<unsigned>("client.graphics.shader_quality") != 2)
        {
            // make sure we do that in dev build
            s_log << Log::error
                  << "Won't convert "
                  << name
                  << " to native format because shader quality is not max.\n";
        } 
#endif

        if (s_params.get<unsigned>("client.graphics.shader_quality") == 2)
        {
            s_log << Log::warning
                  << "Re-saving "
                  << name
                  << " as native\n";
            bbm_root->saveToFile(filename);
        }
    }
    
    delete bbm_root;
  
    return v.getOsgRoot();
}

//------------------------------------------------------------------------------
osgDB::ReaderWriter::WriteResult ReaderWriterBbm::writeNode( const osg::Node& node,
                                                             const std::string& fname,
                                                             const osgDB::ReaderWriter::Options* options ) const
{
    s_log << Log::error << "Bluebeard Model writing not supportet yet.\n";
    return WriteResult::ERROR_IN_WRITING_FILE;
}

//------------------------------------------------------------------------------
/**
 *  Convenience function for model loading. Caches and clones the object.
 */
osg::ref_ptr<OsgNodeWrapper> ReaderWriterBbm::loadModel(const std::string & name)
{
    PROFILE(ReaderWriterBbm::loadModel);
    
    std::string full_name = MODEL_PATH + name + ".bbm";

    ModelResource * res = s_model_manager.getResource(full_name);
    osg::MatrixTransform * model = res->getModel();

    RootNodeUserData * user_data = dynamic_cast<RootNodeUserData*>(model->getUserData());
    assert(user_data);

    if (user_data->flags_ & bbm::BNO_INSTANCED)
    {
        InstancedGeometryDescription * desc;

        desc = s_scene_manager.getInstanceManager()->getOrCreateInstanceDescription(
            name, dynamic_cast<osg::MatrixTransform*>(model));

        return new InstanceProxy(desc);
    } else
    {
        osg::MatrixTransform * node =
            dynamic_cast<osg::MatrixTransform*>(model->clone(osg::CopyOp::DEEP_COPY_NODES));
        CloneUserDataVisitor v;
        node->accept(v);
        return new PlainOsgNode(node);
    }
}
