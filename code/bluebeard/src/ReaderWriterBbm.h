#ifndef READERWRITERBBM_INCLUDED
#define READERWRITERBBM_INCLUDED


#include <sstream>

#include <osg/NodeVisitor>
#include <osg/MatrixTransform>

#include <osgDB/ReaderWriter>


#include "ResourceManager.h"
#include "Log.h"



class OsgNodeWrapper;



//------------------------------------------------------------------------------
/**
 *  Used to enable ResourceManager functionality for osg models.
 */
class ModelResource
{
 public:
    ModelResource(const std::string & name);
    osg::MatrixTransform * getModel() const;
    
 protected:
    osg::ref_ptr<osg::MatrixTransform> model_;
};




#define s_model_manager Loki::SingletonHolder<ModelManager, Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance()
//------------------------------------------------------------------------------
class ModelManager : public ResourceManager<ModelResource>
{
    DECLARE_SINGLETON(ModelManager);
 public:
     virtual ~ModelManager() {};
};


//------------------------------------------------------------------------------
class CloneUserDataVisitor : public osg::NodeVisitor
{ 
 public: 

    CloneUserDataVisitor();
    virtual void apply(osg::Node &node);

};


//------------------------------------------------------------------------------
/**
 *  This class gets registered with the OSG registry to load ".bbm"
 *  files. Lets BBMImporter do all the work and uses BbmOsgConverter
 *  to convert the result to an osg node tree.
 */
class ReaderWriterBbm : public osgDB::ReaderWriter
{
 public:
    ReaderWriterBbm() {}
    virtual ~ReaderWriterBbm() {}

    virtual const char* className() const { return "Bluebeard Model .bbm reader"; }

    virtual bool acceptsExtension(const std::string& extension) const;

    virtual ReadResult  readNode (                  const std::string&, const Options*) const;
    virtual WriteResult writeNode(const osg::Node&, const std::string&, const Options*) const;


    static osg::ref_ptr<OsgNodeWrapper> loadModel(const std::string & name);
    
 protected:
};



#endif
