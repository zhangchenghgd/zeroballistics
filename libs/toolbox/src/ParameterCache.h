
#ifndef TANK_PARAMETERCACHE_INCLUDED
#define TANK_PARAMETERCACHE_INCLUDED

#include <string>
#include <map>

#include "Vector.h"
#include "Vector2d.h"
#include "Datatypes.h"


//------------------------------------------------------------------------------
class ParamNotFoundException : public Exception
{
 public:
    ParamNotFoundException() : Exception("ParamNotFoundException") {}
    ParamNotFoundException(const std::string & msg) : Exception(msg) {}
};




/** \brief Specialized template functions to retrieve string
    representation of datatype used.  **/
template <typename TYPE>
inline std::string getDatatypeAsString();



//------------------------------------------------------------------------------
class ParameterCacheBase
{
public:
    ParameterCacheBase() {};
    virtual ~ParameterCacheBase() {};

    virtual void save(TiXmlElement * element) const = 0;
    
    virtual bool hasKey(const std::string & key) = 0;

    virtual ParameterCacheBase * clone() = 0;
};


//------------------------------------------------------------------------------
/**
 * \brief Templated class for holding pairs key/values 
 *        intended to be used as parameters.
 */
template<class TYPE>
class ParameterCache : public ParameterCacheBase
{
public:
    ParameterCache();
    virtual ~ParameterCache();

    virtual void save(TiXmlElement * elem) const;
    
    TYPE * getPointer(const std::string & name);
    void set(const std::string & name, TYPE value);

    const std::map<std::string, TYPE > & getParameterMap() const;

    virtual bool hasKey(const std::string & key);

    virtual ParameterCache<TYPE> * clone() { return new ParameterCache<TYPE>(*this); }
    
 private:

    std::map<std::string, TYPE > params_;

};

//------------------------------------------------------------------------------
template <class TYPE>
ParameterCache<TYPE>::ParameterCache()
{
}

//------------------------------------------------------------------------------
template <class TYPE>
ParameterCache<TYPE>::~ParameterCache()
{
}

//------------------------------------------------------------------------------
template <class TYPE>
void ParameterCache<TYPE>::save(TiXmlElement * element) const
{
    for (typename std::map<std::string, TYPE >::const_iterator it = params_.begin();
         it != params_.end();
         ++it)
    {
        // Find out section and parameter name
        std::string section_name, name;
        std::string::size_type pos = it->first.rfind('.');
        if (pos != std::string::npos)
        {
            section_name = it->first.substr(0, pos);
            name         = it->first.substr(pos+1);
        } else name = it->first;

        // Try to find section
        TiXmlElement * section;
        for (section = element->FirstChildElement("section");
             section;
             section = section->NextSiblingElement("section"))
        {
            if ((section->Attribute("name") ? section->Attribute("name") : "") == section_name) break;
        }

        // Create section if neccessary
        if (!section)
        {
            section = (TiXmlElement*)element->InsertEndChild(TiXmlElement("section"));
            section->SetAttribute("name", section_name);
        }

        // Add param to section
        TiXmlElement * var = (TiXmlElement*)section->InsertEndChild(TiXmlElement("variable"));
        var->SetAttribute("name", name);
        var->SetAttribute("value", toString(it->second));
        var->SetAttribute("type", getDatatypeAsString<TYPE>());
    }
}


//------------------------------------------------------------------------------
template <class TYPE>
TYPE * ParameterCache<TYPE>::getPointer(const std::string & name)
{
    typename std::map<std::string, TYPE >::iterator it = params_.find(name);

    if (it == params_.end())
    {
        throw ParamNotFoundException("Unable to find parameter: " + name + "\n");
    } else
    {        
        return &(it->second);
    }
}

//------------------------------------------------------------------------------
template <class TYPE>
void ParameterCache<TYPE>::set(const std::string & name, TYPE value)
{
    typename std::map<std::string, TYPE >::iterator it = params_.find(name);

    if (it == params_.end()) 
    {
        params_.insert(make_pair(name, value));
    } else 
    {
        it->second = value;
    }
}

//------------------------------------------------------------------------------
template <class TYPE>
const std::map<std::string, TYPE > & ParameterCache<TYPE>::getParameterMap() const
{
    return params_;
}

//------------------------------------------------------------------------------
template <class TYPE>
bool ParameterCache<TYPE>::hasKey(const std::string & key)
{
    return params_.find(key) != params_.end();
}



template < >
inline std::string getDatatypeAsString<bool>()
{ return std::string("bool"); }

template < >
inline  std::string getDatatypeAsString<int>() 
{ return std::string("int"); }

template < >
inline  std::string getDatatypeAsString<unsigned>() 
{ return std::string("unsigned"); }

template < >
inline  std::string getDatatypeAsString<float>() 
{ return std::string("float"); }

template < >
inline  std::string getDatatypeAsString<double>() 
{ return std::string("double"); }

template < >
inline  std::string getDatatypeAsString<std::string>() 
{ return std::string("string"); }

template < >
inline  std::string getDatatypeAsString<std::vector<bool> >() 
{ return std::string("vector<bool>"); }

template < >
inline  std::string getDatatypeAsString<std::vector<float> >() 
{ return std::string("vector<float>"); }

template < >
inline  std::string getDatatypeAsString<std::vector<std::string> >() 
{ return std::string("vector<string>"); }

template < >
inline  std::string getDatatypeAsString<std::vector<unsigned> >() 
{ return std::string("vector<unsigned>"); }

template < >
inline  std::string getDatatypeAsString<std::vector<std::vector<std::string> > >() 
{ return std::string("vector<vector<string> >"); }

template < >
inline  std::string getDatatypeAsString<std::vector<std::vector<float> > >() 
{ return std::string("vector<vector<float> >"); }

template < >
inline  std::string getDatatypeAsString<std::vector<std::vector<unsigned> > >() 
{ return std::string("vector<vector<unsigned> >"); }

template < >
inline  std::string getDatatypeAsString<Vector>() 
{ return std::string("Vector"); }

template < >
inline  std::string getDatatypeAsString<Vector2d>() 
{ return std::string("Vector2d"); }

template < >
inline  std::string getDatatypeAsString<Color>() 
{ return std::string("Color"); }

template < >
inline  std::string getDatatypeAsString<Matrix>() 
{ return std::string("Matrix"); }

#endif
