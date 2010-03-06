
#include "LevelData.h"


#include <tinyxml.h>

#include "Log.h"
#include "TinyXmlUtils.h"

#undef min
#undef max

namespace bbm
{

//------------------------------------------------------------------------------
LevelData::LevelData()
{
}
    
//------------------------------------------------------------------------------
LevelData::~LevelData()
{
}


//------------------------------------------------------------------------------
/**
 *  Read objects.xml from the specified level dir
 *  (data/levels/<lvl_name>/objects.xml)
 */
void LevelData::load(const std::string & lvl_name)
{
    assert(object_info_.empty());
    assert(detail_texture_.empty());
    
    name_ = lvl_name;

    // -------------------- Object positions --------------------
    object_info_.clear();
    
    using namespace tinyxml_utils;

    TiXmlDocument xml_doc;
    TiXmlHandle root_handle = getRootHandle(LEVEL_PATH + lvl_name + "/objects.xml", xml_doc);
    if (!root_handle.ToNode())
    {
        Exception e("Unknown level: ");
        e << lvl_name;

        throw e;
    }


    
    for (TiXmlElement * cur_elem = root_handle.FirstChildElement("Object").ToElement();
         cur_elem;
         cur_elem = cur_elem->NextSiblingElement("Object"))
    {
        ObjectInfo cur_info;

        cur_info.transform_ = loadTransform(cur_elem);
        cur_info.name_      = getAttributeString(cur_elem, "name");

        TiXmlHandle property_handle = cur_elem->FirstChildElement("Properties");
        if (property_handle.ToElement())
        {
            cur_info.params_.load(property_handle);
        }

        object_info_.push_back(cur_info);
    }


    // -------------------- Custom Properties --------------------
    params_.load(root_handle.FirstChildElement("Properties"));

    


        
    // -------------------- Detail Textures --------------------
    unsigned cur_detail_tex = 0;
    while (true)
    {
        TiXmlNode * detail_tex_node =
            root_handle.FirstChildElement(std::string("DetailTex") + toString(cur_detail_tex)).ToElement();
        if (!detail_tex_node) break;

        Matrix m = loadTransform(detail_tex_node);
        std::string name    = getAttributeString(detail_tex_node, "name");
        float grass_density = getAttributeFloat (detail_tex_node, "grass_density");        
        
        detail_texture_.push_back(DetailTexInfo(name, m, grass_density));

        TiXmlElement * grass_element = detail_tex_node->FirstChildElement("Grass");
        while (grass_element)
        {
            std::string model = getAttributeString(grass_element, "model");
            float prob        = getAttributeFloat (grass_element, "prob");

            detail_texture_.back().zone_info_.push_back(GrassZoneInfo(model, prob));

            grass_element = grass_element->NextSiblingElement("Grass");
        }
        
        ++cur_detail_tex;
    }    
}

//------------------------------------------------------------------------------
void LevelData::save(const std::string & base_dir) const
{
    assert(*base_dir.rbegin() == '/' ||
           *base_dir.rbegin() == '\\');
   
    std::string dest_file = base_dir + "objects.xml";

    TiXmlDocument doc(dest_file);


    doc.InsertEndChild(TiXmlDeclaration("1.0","","yes"));
    TiXmlElement * root = (TiXmlElement*)doc.InsertEndChild(TiXmlElement("LevelData"));


    // -------------------- Custom Properties --------------------
    TiXmlElement * props = (TiXmlElement*)root->InsertEndChild(TiXmlElement("Properties"));
    params_.save(props);    

    
    for (unsigned o=0; o<object_info_.size(); ++o)
    {
        TiXmlElement * obj_elem = (TiXmlElement*)root->InsertEndChild(TiXmlElement("Object"));

        obj_elem->SetAttribute("name", object_info_[o].name_);
        tinyxml_utils::saveTransform(obj_elem, object_info_[o].transform_);

        if (!object_info_[o].params_.isEmpty())
        {
            TiXmlElement * props = (TiXmlElement*)obj_elem->InsertEndChild(TiXmlElement("Properties"));
            object_info_[o].params_.save(props);
        }
    }


    for (unsigned tex_num = 0; tex_num < detail_texture_.size(); ++tex_num)
    {
        TiXmlElement * detail_tex_elem =
            (TiXmlElement*)root->InsertEndChild(TiXmlElement(std::string("DetailTex") + toString(tex_num)));

        tinyxml_utils::saveTransform(detail_tex_elem, detail_texture_[tex_num].matrix_);
        detail_tex_elem->SetAttribute      ("name",          detail_texture_[tex_num].name_);
        detail_tex_elem->SetDoubleAttribute("grass_density", detail_texture_[tex_num].grass_density_);

        for (unsigned g=0; g<detail_texture_[tex_num].zone_info_.size(); ++g)
        {
            TiXmlElement * zone_info_elem =
                (TiXmlElement*)detail_tex_elem->InsertEndChild(TiXmlElement(std::string("Grass")));

            zone_info_elem->SetAttribute("model", detail_texture_[tex_num].zone_info_[g].model_);
            zone_info_elem->SetAttribute("prob",  toString(detail_texture_[tex_num].zone_info_[g].probability_));
        }
    }

    if (!doc.SaveFile())
    {
        Exception e("Couldn't save ");
        e << dest_file
          << ".";
        throw e;
    }
}

//------------------------------------------------------------------------------
const std::string & LevelData::getName() const
{
    return name_;
}


//------------------------------------------------------------------------------
void LevelData::setName(const std::string & name)
{
    name_ = name;
}


//------------------------------------------------------------------------------
void LevelData::addObjectInfo(const ObjectInfo & info)
{
    object_info_.push_back(info);
}

//------------------------------------------------------------------------------
const std::vector<ObjectInfo> & LevelData::getObjectInfo() const
{
    return object_info_;
}


//------------------------------------------------------------------------------
void LevelData::setDetailTexInfo(unsigned num, const DetailTexInfo & info)
{
    detail_texture_.resize(std::max(detail_texture_.size(), (size_t)num+1));
    detail_texture_[num] = info;
}


//------------------------------------------------------------------------------
const std::vector<DetailTexInfo> & LevelData::getDetailTexInfo() const
{
    return detail_texture_;
}

//------------------------------------------------------------------------------
const LocalParameters & LevelData::getParams() const
{
    return params_;
}

//------------------------------------------------------------------------------
LocalParameters & LevelData::getParams()
{
    return params_;
}


} // namespace bbm


