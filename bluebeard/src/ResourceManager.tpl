
#include "Log.h"

#include "ParameterManager.h"

//------------------------------------------------------------------------------
template <typename RESOURCE>
ResourceManager<RESOURCE>::ResourceManager(const std::string & name)
    : name_(name) 
{
    s_log << Log::debug('i')
          << name
          << " manager constructor.\n";
}


//------------------------------------------------------------------------------
template <typename RESOURCE>
ResourceManager<RESOURCE>::~ResourceManager()
{
    s_log << Log::debug('d')
          << name_
          << " destructor\n";
    
    unloadAllResources();
}

//------------------------------------------------------------------------------
template <typename RESOURCE>
const std::string & ResourceManager<RESOURCE>::getName() const
{
    return name_;
}

//------------------------------------------------------------------------------
/**
 *  First unloads all resources not in the specified list and
 *  currently loaded, then loads any resources still missing.
 *
 *  \param filename Name of a config file containing a section with
 *  name name_, containing a list of resources to load.
 */
template <typename RESOURCE>
void ResourceManager<RESOURCE>::loadResourceSet(const std::string & filename)
{
    s_log << Log::debug('r')
          << "Loading "
          << name_
          << " resource set from "
          << filename
          << "\n";

    std::vector<std::string> resources;
    try
    {
        s_params.loadParameters(filename);
        resources = s_params.get<std::vector<std::string> >("level." + name_);
    } catch(Exception & e) 
    {
        // If we fail to load a resource list, unload old resources nevertheless.
        s_log << Log::warning
              << "Could not read "
              << name_
              << " resource list.\n";
    }

    std::map<std::string, RESOURCE* > to_be_unloaded;
    to_be_unloaded.swap(resource_);
    
    for (unsigned r=0; r<resources.size(); ++r)
    {
        const std::string & cur_name = resources[r];
        
        typename std::map<std::string, RESOURCE*>::iterator it = to_be_unloaded.find(cur_name);

        if (it == to_be_unloaded.end())
        {
            // Resource not yet loaded, load it
            createResource(cur_name);
        } else
        {
            // Resource already loaded, transfer it to resource map,
            // remove from to-be-deleted map
            resource_.insert(std::make_pair(it->first, it->second));
            to_be_unloaded.erase(it);
        }
    }

    // unload any remaining resources
    for (typename std::map<std::string, RESOURCE*>::iterator it = to_be_unloaded.begin();
         it != to_be_unloaded.end();
         ++it)
    {
        s_log << Log::debug('r')
              << "Unloading "
              << name_ << " resource "
              << it->first
              << "\n";
        delete it->second;
    }
}

//------------------------------------------------------------------------------
template <typename RESOURCE>
void ResourceManager<RESOURCE>::unloadAllResources()
{    
    for (typename std::map<std::string, RESOURCE*>::iterator it = resource_.begin();
         it != resource_.end();
         ++it)
    {
        s_log << Log::debug('r')
              << "Unloading "
              << it->first
              << "\n";
        delete it->second;
    }

    resource_.clear();
}


//------------------------------------------------------------------------------
/**
 *  Returns the resource with the specified name. If it wasn't loaded
 *  yet, load it, but issue a warning.
 */
template <typename RESOURCE>
RESOURCE * ResourceManager<RESOURCE>::getResource(const std::string & name, bool warn_on_create)
{
    typename std::map<std::string, RESOURCE* >::iterator it = resource_.find(name);

    if (it == resource_.end())
    {
#ifdef ENABLE_DEV_FEATURES
        if (warn_on_create)
        {
            s_log << Log::warning
                  << "Creating "
                  << name_
                  << " resource "
                  << name
                  << "\n";
        }
        
#endif
        return createResource(name);
    } else
    {
        return it->second;
    }
}

//------------------------------------------------------------------------------
template <typename RESOURCE>
std::vector<std::string> ResourceManager<RESOURCE>::getLoadedResources() const
{
    std::vector<std::string> resources;

    for (typename std::map<std::string, RESOURCE*>::const_iterator it = resource_.begin();
         it != resource_.end();
         ++it)
    {
        resources.push_back(it->first);
    }

    return resources;
}



//------------------------------------------------------------------------------
/**
 *  Creates a yet unloaded resource with the specified name.
 */
template <typename RESOURCE>
RESOURCE * ResourceManager<RESOURCE>::createResource(const std::string & name)
{
    assert(resource_.find(name) == resource_.end());

    s_log << Log::debug('r')
          << "Creating "
          << name
          << "\n";

    RESOURCE * ret = new RESOURCE(name);
    
    resource_.insert(std::make_pair(name, ret));

    return ret;
}
