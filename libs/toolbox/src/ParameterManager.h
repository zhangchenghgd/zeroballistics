
#ifndef TANK_PARAMETERMANAGER_INCLUDED
#define TANK_PARAMETERMANAGER_INCLUDED

#include <string>
#include <sstream>
#include <map>

#include <tinyxml.h>

#include "Singleton.h"
#include "Log.h"
#include "ParameterCache.h"

#include "RegisteredFpGroup.h"


typedef Loki::Functor<void, LOKI_TYPELIST_2(const std::string &, const std::string&)> ParameterLoadCallback;


//------------------------------------------------------------------------------
/**
 * \brief Class for managing key/value pairs intended to be used as parameters.
 *
 * The parameters are stored in ParameterCaches corresponding to their datatype.
 * These caches are held by ParameterManager inside a CacheMap, defined below,
 * to allow access to each cache through templated get and set methods.
 *
 * The specialized template methods getDatatypeAsString() are needed to find
 * the corresponding cache inside the CacheMap and get access to its parameters.
 *
 **/
class ParameterManager 
{
 public:
    typedef std::map<std::string, ParameterCacheBase*> CacheMap;

    ParameterManager();

    virtual ~ParameterManager();

    void loadParameters(const std::string & filename, const std::string & super_section = "",
                        ParameterLoadCallback * callback = NULL);    
    void load(const TiXmlHandle & handle, ParameterLoadCallback * callback = NULL);

    void mergeCommandLineParams( int argc, char **argv );
	
    bool saveParameters(const std::string & filename, const std::string & super_section = "");
    void save(TiXmlElement * element) const;

    bool isEmpty() const;

    template<typename TYPE> 
    const std::map<std::string, TYPE > & getParameterMap() const;

    template<typename RETURN_TYPE> 
    const RETURN_TYPE & get(const std::string & key) const;

    template <typename RETURN_TYPE>
    RETURN_TYPE * getPointer(const std::string & key) const;

    template<typename TYPE> 
    void set(const std::string & key, const TYPE & value);

 protected:

    void setOnLoad(const std::string & key, const std::string & value, const std::string & datatype, bool console);
    void get(const std::string & key, std::string & value, const std::string & datatype) const;
    CacheMap::const_iterator getCacheForKey(const std::string & key) const;
	
    template <typename TYPE>
    void insert(const std::string & key, TYPE value, const std::string & datatype, bool console);

    CacheMap caches_;

    RegisteredFpGroup fp_group_; ///< For console registration
};

//------------------------------------------------------------------------------
template<typename TYPE> 
const std::map<std::string, TYPE > & ParameterManager::getParameterMap() const
{
    CacheMap::const_iterator it = caches_.find(getDatatypeAsString<TYPE>());
        
    ParameterCache<TYPE> * cache;

    if (it == caches_.end())
    {
        // this should not happen, throw exception
        throw ParamNotFoundException("No ParameterCache available for this datatype: " + getDatatypeAsString<TYPE>());
    } else
    {
        cache = (ParameterCache<TYPE>*)it->second;
        return cache->getParameterMap();
    }    
}


//------------------------------------------------------------------------------
/**
 *  get parameter value of specified type
 *  i.e. int x = get<int>(key_string);
 */
template <typename RETURN_TYPE>
const RETURN_TYPE & ParameterManager::get(const std::string & key) const
{
    const RETURN_TYPE & p = *(getPointer<RETURN_TYPE>(key));
    return p;
}

//------------------------------------------------------------------------------
/**
 *  get parameter pointer to value of specified type
 */
template <typename RETURN_TYPE>
RETURN_TYPE * ParameterManager::getPointer(const std::string & key) const
{
    CacheMap::const_iterator it = caches_.find(getDatatypeAsString<RETURN_TYPE>());
        
    ParameterCache<RETURN_TYPE> * cache;

    if (it == caches_.end())
    {
        // this should not happen, throw exception
        throw ParamNotFoundException("No ParameterCache available for this datatype: " +
                                     getDatatypeAsString<RETURN_TYPE>() + ". Queried key: " + key);
    } else
    {
        cache = (ParameterCache<RETURN_TYPE>*)it->second;
        try
        {
            return cache->getPointer(key);

        } catch (ParamNotFoundException & e)
        {
            CacheMap::const_iterator cache_it = getCacheForKey(key);
            if (cache_it == caches_.end())
            {
                throw; // unable to find any parameter named key, throw e from ParameterCache
            } else
            {
                throw Exception("Parameter '" + key + "' found with type: " + cache_it->first + 
                                " but used with type: " + getDatatypeAsString<RETURN_TYPE>() + "\n");
            }
        }
    }
}

//------------------------------------------------------------------------------
template<typename TYPE> 
void ParameterManager::set(const std::string & key, const TYPE & value)
{
    CacheMap::const_iterator it = caches_.find(getDatatypeAsString<TYPE>());
        
    ParameterCache<TYPE> * cache;
    if (it == caches_.end())
    {
        cache = new ParameterCache<TYPE>();
        caches_[key] = cache;
    } else
    {
        cache = (ParameterCache<TYPE>*)it->second;
    }    

    cache->set(key, value);
}

//------------------------------------------------------------------------------
template <typename TYPE>
void ParameterManager::insert(const std::string & key, TYPE value, const std::string & datatype, bool console)
{
    CacheMap::iterator it = caches_.find(datatype);
 
    ParameterCache<TYPE> * cache;

    if (it == caches_.end())
    {
        cache = new ParameterCache<TYPE>;
        caches_.insert(make_pair(datatype,cache));
    } else
    {
        cache = (ParameterCache<TYPE>*)it->second;
    }
    cache->set(key, value);
    if(console) s_console.addVariable(key.c_str(), cache->getPointer(key), &fp_group_);
}

//------------------------------------------------------------------------------
/**
 * \brief ParameterManager derived as Singleton for use of global Parameters
 */
#define s_params Loki::SingletonHolder<GlobalParameters, Loki::CreateUsingNew, SingletonParameterManagerLifetime >::Instance()
//------------------------------------------------------------------------------
class GlobalParameters : public ParameterManager
{
    DECLARE_SINGLETON(GlobalParameters);
public:
    virtual ~GlobalParameters() { }
};


//------------------------------------------------------------------------------
/**
 * \brief ParameterManager derived for use of Local Parameters
 */
class LocalParameters : public ParameterManager
{
 public:
    LocalParameters() {}

    LocalParameters(const LocalParameters & other);
    LocalParameters & operator=(const LocalParameters & other);
    
    virtual ~LocalParameters() {}
};



#endif
