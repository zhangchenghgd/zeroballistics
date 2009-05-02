
#include "TinyXmlUtils.h"

#include "Log.h"


namespace tinyxml_utils
{

//------------------------------------------------------------------------------
/**
 *  \param filename The name of the XML file to load
 *  \param xml_doc [out] Will receive the xml document.
 *
 *  \return A handle to the root element of the xml file.
 */
TiXmlHandle getRootHandle(const std::string & filename, TiXmlDocument & xml_doc)
{
    if ( false == xml_doc.LoadFile( filename.c_str() ) )
    {
        Exception e;
        e << "Can't read xml file "
          << filename;
        throw e;
    } 

    TiXmlHandle root_handle = xml_doc.RootElement();
    if ( !root_handle.ToElement() )
    {
        Exception e;
        e << "Can't find root element in "
          << filename;
        throw e;
    }

    return root_handle;
}
    

//------------------------------------------------------------------------------
TiXmlNode * getChildNode(TiXmlNode * node, const std::string & name)
{
    if (!node) return NULL;
    TiXmlNode * ret = node->FirstChild(name.c_str());

    if (!ret)
    {
        Exception e;
        e << "Missing "
          << name
          << " in "
          << node->Value();
        e.addHistory("getChildNode");
        throw e;
    }

    return ret;
}


//------------------------------------------------------------------------------
float getAttributeFloat( const TiXmlNode* node, const std::string & name )
{
    const TiXmlElement * element = node->ToElement();
                        
    double temp = 0.0f;

    if ( !element || NULL == element->Attribute( name.c_str(), &temp ) )
    {
        s_log << Log::error
              << "element "
              << node->Value()
              << " is missing attribute \""
              << name
              << "\".\n";
        return 0;
    } else return (float)temp;
}

//------------------------------------------------------------------------------
int getAttributeInt(const TiXmlNode* node, const std::string & name )
{
    const TiXmlElement * element = node->ToElement();
                        
    int temp = 0;
    if ( !element || NULL == element->Attribute( name.c_str(), &temp ) )
    {
        s_log << Log::warning
              << "element "
              << node->Value()
              << " is missing attribute \""
              << name
              << "\".\n";
        return 0;
    } else return temp;
}


//------------------------------------------------------------------------------
std::string getAttributeString(const TiXmlNode* node, const std::string & name )
{
    const TiXmlElement * element = node->ToElement();

    const char* temp = NULL;

    if (element) temp = element->Attribute( name.c_str() );

    if ( NULL == temp )
    {
        s_log << Log::warning
              << "element "
              << node->Value()
              << " is missing attribute \""
              << name
              << "\".\n";
        return "";
    } else return temp;
}

//------------------------------------------------------------------------------
bool getAttributeBool(const TiXmlNode* node, const std::string & name )
{                        
    std::string temp = "";
    temp = getAttributeString(node, name);

    if(temp == "true") return true;
    if(temp == "false") return false;

    s_log << Log::warning << " Could not identify bool value for "
              << "element "
              << node->Value()
              << " attribute \""
              << name
              << "\".\n";

    return false;
}

//------------------------------------------------------------------------------
Matrix loadTransform(TiXmlNode *offset_node)
{
    Matrix ret(true);
    
    TiXmlNode * transform_node = offset_node->FirstChild("Transform");

    if (!transform_node) return ret;

    ret._11 = getAttributeFloat(transform_node, "_11");
    ret._21 = getAttributeFloat(transform_node, "_21");
    ret._31 = getAttributeFloat(transform_node, "_31");

    ret._12 = getAttributeFloat(transform_node, "_12");
    ret._22 = getAttributeFloat(transform_node, "_22");
    ret._32 = getAttributeFloat(transform_node, "_32");

    ret._13 = getAttributeFloat(transform_node, "_13");
    ret._23 = getAttributeFloat(transform_node, "_23");
    ret._33 = getAttributeFloat(transform_node, "_33");

    ret._14 = getAttributeFloat(transform_node, "_14");
    ret._24 = getAttributeFloat(transform_node, "_24");
    ret._34 = getAttributeFloat(transform_node, "_34");
    
    return ret;
}


//------------------------------------------------------------------------------
void saveTransform(TiXmlElement * parent_elem, const Matrix & transform)
{
    TiXmlElement transform_elem("Transform");

    transform_elem.SetAttribute("_11", toString(transform._11));
    transform_elem.SetAttribute("_21", toString(transform._21));
    transform_elem.SetAttribute("_31", toString(transform._31));

    transform_elem.SetAttribute("_12", toString(transform._12));
    transform_elem.SetAttribute("_22", toString(transform._22));
    transform_elem.SetAttribute("_32", toString(transform._32));

    transform_elem.SetAttribute("_13", toString(transform._13));
    transform_elem.SetAttribute("_23", toString(transform._23));
    transform_elem.SetAttribute("_33", toString(transform._33));

    transform_elem.SetAttribute("_14", toString(transform._14));
    transform_elem.SetAttribute("_24", toString(transform._24));
    transform_elem.SetAttribute("_34", toString(transform._34));
    
    parent_elem->InsertEndChild(transform_elem);
}


}
