

#ifndef BLUEBEARD_RESOURCE_MANAGER_INCLUDED
#define BLUEBEARD_RESOURCE_MANAGER_INCLUDED


#include <map>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
template <typename RESOURCE>
class ResourceManager
{
 public:

    ResourceManager(const std::string & name);
    virtual ~ResourceManager();

    const std::string & getName() const;

    void loadResourceSet(const std::string & filename);
    void unloadAllResources();

    RESOURCE * getResource(const std::string & name, bool warn_on_create = true);

    std::vector<std::string> getLoadedResources() const;
    
    
 protected:

    RESOURCE * createResource(const std::string & name);

    std::string name_;

    std::map<std::string, RESOURCE* > resource_;
};


#include "ResourceManager.tpl"

#endif
