
#include "OdeModelLoader.h"


#include <tinyxml.h>


#include "Log.h"
#include "TinyXmlUtils.h"
#include "OdeSimulator.h"
#include "OdeRigidBody.h"

#ifndef NO_TERRAIN
#include "TerrainData.h"
#endif

namespace physics
{


const std::string ODE_MODEL_PATH = "data/models/";
    
    
//------------------------------------------------------------------------------
OdeModelLoader::OdeModelLoader()
{
    s_log << Log::debug('i')
          << "OdeModelLoader constructor.\n";
}

//------------------------------------------------------------------------------
/**
 *  Delete "blueprint" rigidbodies.
 */
OdeModelLoader::~OdeModelLoader()
{
    s_log << Log::debug('d')
          << "OdeModelLoader destructor\n";
    
    for (std::vector<OdeModelInfo>::iterator it = info_.begin();
         it != info_.end();
         ++it)
    {
        delete it->body_;
    }

    info_.clear();
}

//------------------------------------------------------------------------------
OdeRigidBody * OdeModelLoader::instantiateModel(OdeSimulator * simulator,
                                                const std::string & name)
{
    assert(simulator);
    
    OdeRigidBody * blueprint = NULL;
     
    for (std::vector<OdeModelInfo>::iterator it = info_.begin();
         it != info_.end();
         ++it)
    {
        if (it->name_ == name)
        {
            blueprint = it->body_;
            break;
        }
    }


    if (!blueprint)
    {
        // Body wasn't loaded before, so we have to do it...
        try
        {
            blueprint = loadModel(name);
            info_.push_back(OdeModelInfo(name, blueprint));
        } catch (Exception & e)
        {
            e.addHistory("OdeModelLoader::instantiateModel(" + name + ")");
            throw e;
        }
    }
    
    return simulator->instantiate(blueprint);
}


//------------------------------------------------------------------------------
OdeRigidBody * OdeModelLoader::loadModel(const std::string & name)
{
    using namespace tinyxml_utils;

    TiXmlBase::SetCondenseWhiteSpace(false);
    TiXmlDocument xml_doc;
    TiXmlHandle root_handle = getRootHandle(ODE_MODEL_PATH + name + ".xml", xml_doc);
    TiXmlBase::SetCondenseWhiteSpace(true);

    if (std::string("RigidBody") != root_handle.ToElement()->Value())
    {
        Exception e;
        e << (ODE_MODEL_PATH + name + ".xml") << " is not a valid rigidbody xml file.\n";
        throw e;
    }

    OdeRigidBody * ret = new OdeRigidBody();

    ret->name_ = name;


    TiXmlNode * static_node = root_handle.FirstChildElement("Static").ToElement();
    if (static_node)
    {
        ret->static_ = getAttributeString(static_node, "value") == "1";
    }


    TiXmlNode * cog_node = root_handle.FirstChildElement("COG").ToElement();
    if (cog_node)
    {
        ret->cog_ = Vector(getAttributeFloat(cog_node, "x"),
                           getAttributeFloat(cog_node, "y"),
                           getAttributeFloat(cog_node, "z"));
    }

    for (TiXmlNode * shape_node = root_handle.FirstChildElement("Shape").ToElement();
         shape_node;
         shape_node = shape_node->NextSiblingElement("Shape"))
    {
        OdeGeom * geom = loadShape(name, shape_node);

        if (geom)
        {
            geom->body_ = ret;
            ret->geom_.push_back(geom);
        }
    }
    
    return ret;
}


//------------------------------------------------------------------------------
/**
 *  \param name Only used to identify any contained trimesh. An xml
 *  file can contain only one trimesh shape.
 *  \param shape_node The shape node to load.
 */
OdeGeom * OdeModelLoader::loadShape(const std::string & name, TiXmlNode * shape_node)
{
    using namespace tinyxml_utils;
    
    OdeGeom * ret;
    std::string shape_name = "";
    
    try
    {
        shape_name = getAttributeString(shape_node, "name");

        s_log << Log::debug('r')
              << "Importing shape "
              << shape_name
              << "\n";
        
        std::string type = getAttributeString(shape_node, "type");
        if (type == "box")
        {
            ret = loadBox(shape_node);
        } else if (type == "sphere")
        {
            ret = loadSphere(shape_node);
        } else if (type == "ccylinder")
        {
            ret = loadCCylinder(shape_node);
        } else if (type == "plane")
        {
            ret = loadPlane(shape_node);
        } else if (type == "ray")
        {
            ret = loadRay(shape_node);
        } else if (type == "trimesh")
        {
            ret = loadTrimesh(name, shape_node);
        } else if (type == "continuous")
        {
            ret = loadContinuous(shape_node);
        } else
        {
            Exception e("Unknown shape type: ");
            e << type;
            throw e;
        }


        ret->name_      = shape_name;
        ret->is_sensor_ = getAttributeString(shape_node, "sensor") == "1";
        ret->material_  = loadMaterial (shape_node);
        
        ret->offset_ = loadTransform(shape_node);
        
    } catch (Exception & e)
    {
        e.addHistory("OdeModelLoader::loadShape(" + shape_name +")");
        throw e;
    }
    
    return ret;
}

//------------------------------------------------------------------------------
OdeGeom * OdeModelLoader::loadSphere(TiXmlNode * sphere_node)
{
    using namespace tinyxml_utils;
    
    float radius = getAttributeFloat(getChildNode(sphere_node, "Dimensions"), "radius");

    return new OdeSphereGeom(radius, false);
}

//------------------------------------------------------------------------------
OdeGeom * OdeModelLoader::loadCCylinder(TiXmlNode * ccylinder_node)
{
    using namespace tinyxml_utils;
    
    float radius = getAttributeFloat(getChildNode(ccylinder_node, "Dimensions"), "radius");
    float length = getAttributeFloat(getChildNode(ccylinder_node, "Dimensions"), "length");

    return new OdeCCylinderGeom(radius, length, false);
}


//------------------------------------------------------------------------------
OdeGeom * OdeModelLoader::loadBox(TiXmlNode * box_node)
{
    using namespace tinyxml_utils;

    TiXmlNode * dim_node = getChildNode(box_node, "Dimensions");
    
    float x = getAttributeFloat(dim_node, "x");
    float y = getAttributeFloat(dim_node, "y");
    float z = getAttributeFloat(dim_node, "z");

    return new OdeBoxGeom(x,y,z);
}

//------------------------------------------------------------------------------
OdeGeom * OdeModelLoader::loadPlane(TiXmlNode * plane_node)
{
    using namespace tinyxml_utils;

    TiXmlNode * dim_node = getChildNode(plane_node, "Dimensions");
    
    float a = getAttributeFloat(dim_node, "a");
    float b = getAttributeFloat(dim_node, "b");
    float c = getAttributeFloat(dim_node, "c");
    float d = getAttributeFloat(dim_node, "d");

    return new OdePlaneGeom(a,b,c,d);
}

//------------------------------------------------------------------------------
OdeGeom * OdeModelLoader::loadRay(TiXmlNode * ray_node)
{
    using namespace tinyxml_utils;
    
    float length = getAttributeFloat(getChildNode(ray_node, "Dimensions"), "length");

    return new OdeRayGeom(length, false);
}


//------------------------------------------------------------------------------
OdeGeom * OdeModelLoader::loadTrimesh(const std::string & name, TiXmlNode * trimesh_node)
{
    using namespace tinyxml_utils;

    Trimesh * trimesh = new Trimesh;


    TiXmlElement * text_element = trimesh_node->ToElement();
    if (!text_element) throw Exception("Bad XML:" + name);


    std::string text = text_element->GetText();
    std::istringstream in(text);

    ::operator>>(in, trimesh->vertex_data_);
    ::operator>>(in, trimesh->index_data_);    

    if (removeDegenerates(trimesh))
    {
        s_log << Log::warning
              << "Degenerate triangles removed in mesh \""
              << name
              << "\"\n";
    }

    s_log << Log::debug('r')
          << "Loaded trimesh "
          << name
          << ": "
          << trimesh->vertex_data_.size()
          << " vertices, "
          << trimesh->index_data_.size()
          << " faces.\n";

    trimesh->buildTrimesh();
    
    return new OdeTrimeshGeom(trimesh);
}


//------------------------------------------------------------------------------
OdeGeom * OdeModelLoader::loadContinuous(TiXmlNode * cont_node)
{
    return new OdeContinuousGeom();
}



//------------------------------------------------------------------------------
Material OdeModelLoader::loadMaterial(TiXmlNode * shape_node)
{
    using namespace tinyxml_utils;
    
    Material mat;
    
    mat.bounciness_ = getAttributeFloat(shape_node, "bounciness");
    mat.mass_       = getAttributeFloat(shape_node, "mass");
    mat.friction_   = getAttributeFloat(shape_node, "friction");

    if (mat.mass_ == 0.0f) throw Exception("Material has no mass");

    return mat;
}


//------------------------------------------------------------------------------
/**
 *  \return Whether any degenerates were removed.
 */
bool OdeModelLoader::removeDegenerates(Trimesh * trimesh) const
{
    std::vector<TrimeshFace>::iterator cur_face = trimesh->index_data_.begin();

    bool ret = false;
    
    while (cur_face != trimesh->index_data_.end())
    {
        Vector ab = trimesh->vertex_data_[cur_face->v2_] - trimesh->vertex_data_[cur_face->v1_];
        Vector ac = trimesh->vertex_data_[cur_face->v3_] - trimesh->vertex_data_[cur_face->v1_];
        Vector cross;

        vecCross(&cross, &ab, &ac);
        float area = cross.lengthSqr();

        if (area < 1e-14)
        {
            ret = true;
            
            s_log << Log::debug('r')
                  << "Ignoring zero-area triangle. Coords: "
                  << trimesh->vertex_data_[cur_face->v1_]
                  << trimesh->vertex_data_[cur_face->v2_]
                  << trimesh->vertex_data_[cur_face->v3_]
                  << "\n";
            s_log << Log::debug('r')
                  << "indices: "
                  << cur_face->v1_ << " "
                  << cur_face->v2_ << " "
                  << cur_face->v3_ << " "
                  << "\n";
            
            cur_face = trimesh->index_data_.erase(cur_face);
            
        } else ++cur_face;
    }

    return ret;
}


} // namespace physics
