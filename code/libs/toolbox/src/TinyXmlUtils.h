
#ifndef BLUEBEARD_TINY_XML_UTILS_INCLUDED
#define BLUEBEARD_TINY_XML_UTILS_INCLUDED


#include <string>


#include <tinyxml.h>


#include "Matrix.h"


namespace tinyxml_utils
{

TiXmlHandle getRootHandle(const std::string & filename, TiXmlDocument & xml_doc);

TiXmlNode * getChildNode(TiXmlNode * node, const std::string & name);
 
float       getAttributeFloat (const TiXmlNode* node, const std::string & name );
int         getAttributeInt   (const TiXmlNode* node, const std::string & name );
std::string getAttributeString(const TiXmlNode* node, const std::string & name );
bool        getAttributeBool  (const TiXmlNode* node, const std::string & name );

Matrix loadTransform(TiXmlNode *offset_node);

void saveTransform(TiXmlElement * parent_elem, const Matrix & transform);
 
} 

#endif
