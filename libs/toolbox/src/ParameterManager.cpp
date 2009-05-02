
#include "ParameterManager.h"

#include "TinyXmlUtils.h"
#include "Log.h"

//------------------------------------------------------------------------------
class MalformedEntryException : public Exception
{
 public:
    MalformedEntryException() : Exception("MalformedEntryException"){}
};



//------------------------------------------------------------------------------
ParameterManager::ParameterManager()
{
}

//------------------------------------------------------------------------------
ParameterManager::~ParameterManager()
{
    // delete parameter caches, that were instantiated in insert methods
    CacheMap::iterator it;
    for(it = caches_.begin(); it != caches_.end(); it++)
    {
        delete it->second;
    }

    caches_.clear();
}

//------------------------------------------------------------------------------
/**
 * \brief Load params from config XML.
 *
 * Not yet known keys are added and old values are rewriten.
 * You may call this function more times on one object.
 * \param  filename path to file with settings
 */
void ParameterManager::loadParameters(const std::string & filename,
                                      const std::string & super_section,
                                      ParameterLoadCallback * callback)
{
    using namespace tinyxml_utils;
    
    TiXmlDocument xml_doc;
    TiXmlHandle root_handle = getRootHandle(filename, xml_doc);

    try
    {
        if(super_section.empty())
        {
            load(root_handle, callback);
        } else
        {
            bool found_super_section = false;
        
            // loop super sections
            for (TiXmlElement* cur_super_section =
                     root_handle.FirstChild("super_section").Element();
                 cur_super_section;
                 cur_super_section = cur_super_section->NextSiblingElement()) 
            {
                if (getAttributeString(cur_super_section, "name") == super_section)
                {
                    if (found_super_section)
                    {
                        s_log << Log::error
                              << "Super section "
                              << super_section
                              << " exists twice in "
                              << filename
                              << "\n";
                    } else
                    {
                        found_super_section = true;
                        load(cur_super_section, callback);
                    }
                }
            }

            if (!found_super_section)
            {
                s_log << Log::error
                      << "Could not find supersection "
                      << super_section
                      << " in file "
                      << filename
                      << ".\n";
            }
        }
    } catch (MalformedEntryException & e)
    {
        s_log << Log::error
              << "Malformed entry in ParameterManager::loadParameters("
              << filename
              << ", "
              << super_section
              << ")\n";
    } catch(Exception & e)
    {
        e.addHistory("ParameterManager::loadParameters(" + filename + ", " + super_section + ")");
        throw e;
    }
}


//------------------------------------------------------------------------------
void ParameterManager::load(const TiXmlHandle & handle, ParameterLoadCallback * callback)
{
    using namespace tinyxml_utils;
    
    if (!handle.ToElement()) throw Exception("Missing top-lvl element");

    // loop sections
    bool malformed_entry = false;
    for (TiXmlElement * section = handle.FirstChild("section").ToElement();
         section;
         section=section->NextSiblingElement("section")) 
    {
        std::string prefix = getAttributeString(section, "name");

        // loop through 'variable' Elements in each section
        for (TiXmlElement* child = section->FirstChildElement("variable");
             child;
             child=child->NextSiblingElement("variable"))
        {
            const char *key   = child->Attribute("name");
            const char *value = child->Attribute("value");
            const char *datatype = child->Attribute("type");
            const char *console = child->Attribute("console");

            if (key != NULL && value != NULL && datatype != NULL) 
            {
                // if section name is empty, don't append point
                std::string section_name;
                if(!prefix.empty()) section_name = prefix + ".";

                std::string full_key = section_name + std::string(key);

                // insert value into ParameterCache                
                setOnLoad(full_key, value, datatype, console && *console=='1');

                if (callback) (*callback)(full_key, value);
            }
            else
            {
                malformed_entry = true;
                s_log << Log::warning
                      << " Unable to read parameter from section "
                      << prefix << ", either missing [key,value,datatype] ["
                      << ((key==NULL) ? " " : key) << "," 
                      << ((value==NULL) ? " " : value) << "," 
                      << ((datatype==NULL) ? " " : datatype) << "]  \n";
            }
        }
    }

    if (malformed_entry) throw MalformedEntryException();
}


//------------------------------------------------------------------------------
/**
 *  Parameters specified on the command line have the format
 *  --param1=foo --param2=param with spaces --param3=abcd
 */
void ParameterManager::mergeCommandLineParams( int argc, char **argv )
{
    std::string cmd_line;
    // First merge all params to one single string
    for (int i=1; i<argc; ++i)
    {
        cmd_line += argv[i];
        cmd_line += " ";
    }

    Tokenizer tok(cmd_line, "--");

    while (!tok.isEmpty())
    {
        Tokenizer tok2(tok.getNextWord(), '=');

        try
        {
            std::string name  = tok2.getNextWord();
            std::string value;
            if (!tok2.isEmpty()) value = tok2.getNextWord();

            if (!tok2.isEmpty()) throw Exception("More than one '='");
            
            // Now find the parameter cache which has an entry with
            // the given name. Bail if this is ambiguous...
            CacheMap::const_iterator cur_cache = getCacheForKey(name);
            if (cur_cache == caches_.end())
            {
                Exception e;
                e << "No param \""
                  << name
                  << "\" exists.";
                throw e;
            } else
            {
                setOnLoad(name, value, cur_cache->first, false);
            }
        } catch (Exception & e)
        {
            e.addHistory("ParameterManager::mergeCommandLineParams");
            s_log << Log::warning
                  << e
                  << "\n";
        }
    }
}



//------------------------------------------------------------------------------
/**
 * \brief Save params to file.
 *
 * Opens and scans file. Only rewrite values in file to new values.
 * Doesn't add any new variables. Note that indentation may change but
 * all data including comments are kept.
 * \param filename filename
 * \return true if sucessfull
 */
bool ParameterManager::saveParameters(const std::string & filename, const std::string & super_section)
{
    assert(!filename.empty());
	
    TiXmlDocument doc(filename);
    TiXmlHandle   handle(&doc);


    if (!doc.LoadFile()) 
    {
        s_log << Log::error << "Unable to load " << filename 
              <<" , either file does not exist or it is an invalid XML file\n";
        return false;
    }	
	
    // check for super sections & sections
    TiXmlElement* section = NULL;
    
    if(super_section == "")
    {
        section = handle.FirstChild("parameters").FirstChild("section").Element();
    } else
    {
       TiXmlElement* super_sections = handle.FirstChild("parameters").FirstChild("super_section").Element();
        // loop super sections
        for (; super_sections; super_sections=super_sections->NextSiblingElement()) 
        {
            const char *name = super_sections->Attribute("name");
            if(name)
            {
                if(std::string(name) == super_section)
                {
                    TiXmlHandle section_handle(super_sections->FirstChild("section"));
                    section = section_handle.Element();
                    continue;
                }
            }
        }
    }

    if (!section)
    {
        s_log << Log::warning << filename << " no section found.\n";
        return false;
    }

    // loop sections
    for (; section; section=section->NextSiblingElement()) 
    {
        const char *prefix = section->Attribute("name");

        if(!prefix)
        {
            s_log << Log::warning << " Section without name attribute in file: " << filename << "\n";
            continue;
        }

        // loop through 'variable' Elements in each section
        TiXmlElement* child = section->FirstChildElement("variable");
        for (; child; child=child->NextSiblingElement()) 
        {
            const char *key   = child->Attribute("name");
            const char *value = child->Attribute("value");
            const char *datatype = child->Attribute("type");

            if (key != NULL && value != NULL && datatype != NULL) 
            {
                // if section name is empty, don't append point
                std::string section_name = prefix;
                if(!section_name.empty()) section_name += ".";

                std::string full_key = section_name + std::string(key);

                std::string current_value;
                get(full_key,current_value,datatype); // retrieve value from ParameterCache

                child->SetAttribute("value", current_value);  // write new value to file
            }
        }
    }
	
    if (!doc.SaveFile())
    {
        s_log << Log::error << "Unable to save " << filename << "\n";
        return false;
    }

    return true;
}


//------------------------------------------------------------------------------
void ParameterManager::save(TiXmlElement * elem) const
{
    for (CacheMap::const_iterator it = caches_.begin();
         it != caches_.end();
         ++it)
    {
        it->second->save(elem);
    }
}

    
//------------------------------------------------------------------------------
bool ParameterManager::isEmpty() const
{
    return caches_.empty();
}



//------------------------------------------------------------------------------
/**
 * \brief Used internally to insert the value in the correct datatype map
 * 
 * If the key already exists, the old value is overwritten.
 *
 * \param key The Key.
 * \param value String value for the key.
 * \param datatype String representation of datatype for the key.
 * \param console Decides wether the param should be registered as console parameter or not.
 */
void ParameterManager::setOnLoad(const std::string & key, const std::string & value, const std::string & datatype, bool console)
{
    try
    {
        CacheMap::iterator it = caches_.find(datatype);
        
        if(datatype == "bool") 
        { 
            insert(key, fromString<bool>(value), datatype, console); 
            return; 
        }    
        if(datatype == "int") 
        {
            insert(key, fromString<int>(value), datatype, console);
            return;
        }
        if(datatype == "unsigned") 
        {
            insert(key, fromString<unsigned>(value), datatype, console);
            return;
        }
        if(datatype == "float") 
        {
            insert(key, fromString<float>(value), datatype, console);
            return;
        }
        if(datatype == "string")
        {
            insert(key, fromString<std::string>(value), datatype, console);
            return;
        }
        if(datatype == "vector<bool>")
        {
            insert(key, fromString<std::vector<bool> >(value), datatype, console);
            return;
        }
        if(datatype == "vector<float>")
        {
            insert(key, fromString<std::vector<float> >(value), datatype, console);
            return;
        }
        if(datatype == "vector<string>")
        {
            insert(key, fromString<std::vector<std::string> >(value), datatype, console);
            return;
        }    
        if(datatype == "vector<unsigned>")
        {
            insert(key, fromString<std::vector<unsigned> >(value), datatype, console);
            return;
        }    
        if(datatype == "vector<vector<string> >")
        {
            insert(key, fromString<std::vector<std::vector<std::string> > >(value), datatype, console);
            return;
        }
        if(datatype == "vector<vector<float> >")
        {
            insert(key, fromString<std::vector<std::vector<float> > >(value), datatype, console);
            return;
        }
        if(datatype == "vector<vector<unsigned> >")
        {
            insert(key, fromString<std::vector<std::vector<unsigned> > >(value), datatype, console);
            return;
        }
        if(datatype == "Vector")
        {
            insert(key, fromString<Vector>(value), datatype, console);
            return;
        }
        if(datatype == "Vector2d")
        {
            insert(key, fromString<Vector2d>(value), datatype, console);
            return;
        }
        if(datatype == "Color")
        {
            insert(key, fromString<Color>(value), datatype, console);
            return;
        }
        if(datatype == "Matrix")
        {
            insert(key, fromString<Matrix>(value), datatype, console);
            return;
        }

    } catch (Exception & e)
    {
        e.addHistory("ParameterManager::setOnLoad(" + key + ")");
        throw e;
    }
    // if we get here, there is no implementation for this datatype. throw error.
    s_log << Log::error << " Unknown Datatype '" << datatype << "' in ParameterManager for key: '" << key << "' !!\n";

}

//------------------------------------------------------------------------------
void ParameterManager::get(const std::string & key, std::string & value, const std::string & datatype) const
{
    if(datatype == "bool") {
        value = toString(get<bool>(key));
        return;
    }
    if(datatype == "int") {
        value = toString(get<int>(key));
        return;
    }
    if(datatype == "unsigned") {
        value = toString(get<unsigned>(key));
        return;
    }
    if(datatype == "float") {
        value = toString(get<float>(key));
        return;
    }
    if(datatype == "string") {
        value = get<std::string>(key);
        return;
    }
    if(datatype == "vector<unsigned>") {
        value = toString(get<std::vector<unsigned> >(key));
        return;
    }
    if(datatype == "vector<bool>") {
        value = toString(get<std::vector<bool> >(key));
        return;
    }
    if(datatype == "vector<float>") {
        value = toString(get<std::vector<float> >(key));
        return;
    }
    if(datatype == "vector<string>") {
        value = toString(get<std::vector<std::string> >(key));
        return;
    }
    if(datatype == "vector<vector<string> >")
    {
        value = toString(get<std::vector<std::vector<std::string> > >(key));
        return;
    }
    if(datatype == "vector<vector<float> >")
    {
        value = toString(get<std::vector<std::vector<float> > >(key));
        return;
    }
    if(datatype == "vector<vector<unsigned> >")
    {
        value = toString(get<std::vector<std::vector<unsigned> > >(key));
        return;
    }
    if(datatype == "Vector") {
        value = toString(get<Vector>(key));
        return;
    }
    if(datatype == "Vector2d") {
        value = toString(get<Vector2d>(key));
        return;
    }
    if(datatype == "Color") {
        value = toString(get<Color>(key));
        return;
    }
    if(datatype == "Matrix") {
        value = toString(get<Matrix>(key));
        return;
    }

    // if we get here, there is no implementation for this datatype. throw error.
    s_log << Log::error << " Unknown Datatype '" << datatype << "' in ParameterManager for key: '" << key << "' !!\n";

}

//------------------------------------------------------------------------------
/**
 *  Returns an iterator for caches_ which points to the cache
 *  containing the specified key, or caches_.end(). Throws an
 *  exception if more than one cache contains the specified key.
 */
ParameterManager::CacheMap::const_iterator ParameterManager::getCacheForKey(const std::string & key) const
{
    ParameterManager::CacheMap::const_iterator ret = caches_.end();
    
    for(CacheMap::const_iterator it = caches_.begin();
        it != caches_.end();
        it++)
    {
        if (!it->second->hasKey(key)) continue;

        if (ret == caches_.end())
        {
            ret = it;
        } else
        {
            Exception e;
            e << "Key "
              << key
              << " was used with different data types in config file.";
            throw e;
        }
    }

    return ret;
}


//------------------------------------------------------------------------------
GlobalParameters::GlobalParameters()
{
}



//------------------------------------------------------------------------------
LocalParameters::LocalParameters(const LocalParameters & other)
{
    *this = other;
}


//------------------------------------------------------------------------------
LocalParameters & LocalParameters::operator=(const LocalParameters & other)
{
    caches_.clear();
    
    for (CacheMap::const_iterator it = other.caches_.begin();
         it != other.caches_.end();
         ++it)
    {
        caches_[it->first] = it->second->clone();
    }

    return *this;
}
