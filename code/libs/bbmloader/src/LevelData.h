
#ifndef BLUEBEARD_LEVEL_DATA_INCLUDED
#define BLUEBEARD_LEVEL_DATA_INCLUDED


#include <string>
#include <map>

#include "Matrix.h"
#include "ParameterManager.h"

namespace bbm
{

const std::string LEVEL_PATH = "data/levels/";
    

//------------------------------------------------------------------------------    
class ObjectInfo
{
 public:
    ObjectInfo() : transform_(true) {}
    
    std::string name_;
    Matrix transform_;
    
    LocalParameters params_; ///< Filled with properties from Grome.
};


//------------------------------------------------------------------------------
class GrassZoneInfo
{
 public:
    GrassZoneInfo(const std::string & model, float p) :
        model_(model), probability_(p) {}
    
    std::string model_;
    float probability_;
};
 
//------------------------------------------------------------------------------ 
class DetailTexInfo
{
 public:
    DetailTexInfo() : matrix_(true), grass_density_(0.0f) {}
    DetailTexInfo(const std::string & name, const Matrix & m, float d) :
        name_(name), matrix_(m), grass_density_(d) {}

    std::string name_;
    Matrix matrix_;

    float grass_density_;
    std::vector<GrassZoneInfo> zone_info_;
};

 
//------------------------------------------------------------------------------
class LevelData
{
 public:
    LevelData();
    virtual ~LevelData();
    
    void load(const std::string & lvl_name);
    void save(const std::string & data_dir) const;
    
    const std::string & getName() const;
    void setName(const std::string & name);
    
    void addObjectInfo(const ObjectInfo & info);
    const std::vector<ObjectInfo> & getObjectInfo() const;

    
    void setDetailTexInfo(unsigned num, const DetailTexInfo & info);
    const std::vector<DetailTexInfo> & getDetailTexInfo() const;

    const LocalParameters & getParams() const;
    LocalParameters & getParams();
 protected:

    std::string name_;
    
    std::vector<ObjectInfo> object_info_;

    std::vector<DetailTexInfo> detail_texture_;

    LocalParameters params_;
};


}

#endif
