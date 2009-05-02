
//------------------------------------------------------------------------------
template<class T>
ClassLoader<T>::ClassLoader()
{
}

//------------------------------------------------------------------------------
template<class T>
ClassLoader<T>::~ClassLoader()
{
}


//------------------------------------------------------------------------------
/**
 *  Trys to open the specified dynamic library. Throws an exception on
 *  failure.
 *
 *  \todo display win error (FormatMessage)
 */
template<class T>
void ClassLoader<T>::openLibrary(const std::string & filename)
{
    void * handle;
    
#if _WIN32
    handle = LoadLibraryEx(filename.c_str(),NULL,LOAD_WITH_ALTERED_SEARCH_PATH);    

    if (!handle)
    {
        Exception e("Couldn't load library ");
//        DWORD error = GetLastError();
        //...
        e << filename << "\n";
        throw e;
    }
#else
    handle = dlopen(filename.c_str(), RTLD_NOW);

    if (!handle)
    {
        Exception e("Couldn't load library ");
        e << filename << ":\n" << dlerror();
        throw e;
    }
#endif

    open_handle_.push_back(handle);
}

//------------------------------------------------------------------------------
/**
 *  Returns a new instance of the class registered under the specified
 *  name.
 *
 *  \param name The name under which the class was registered.
 */
template<class T>
T * ClassLoader<T>::create(const std::string & name)
{
     RegisteredClassIterator it = registered_class_.find(name);
     if (it == registered_class_.end())
     {
         Exception e("Couldn't load class ");
         e << name << " because it was not registered.";
         throw e;
     }
     
     return it->second();
}


//------------------------------------------------------------------------------
/**
 *  Adds a new class factory method to the registered classes.
 *
 *  \param name The name under which the class will be registered.
 *  \param maker The factory method for the class.
 */
template<class T>
void ClassLoader<T>::addRegisteredClass(const std::string & name, maker_fun_t maker)
{
    if (registered_class_.find(name) != registered_class_.end())
    {
        s_log << Log::error << "Tried to register a class with an already registered name.\n"
              << Log::error << "Ignoring class " << name << "\n";
        return;
    }

//    s_log << Log::debug('i') << "class " << name << " registered.\n";
    
    registered_class_.insert(std::make_pair(name, maker));
}


//------------------------------------------------------------------------------
template<class T>
std::vector<std::string> ClassLoader<T>::getRegisteredClassNames() const
{
    std::vector<std::string> ret;

    for (ConstRegisteredClassIterator it = registered_class_.begin();
         it != registered_class_.end();
         ++it)
    {
        ret.push_back(it->first);
    }
    
    return ret;
}


//------------------------------------------------------------------------------
template<class T>
bool ClassLoader<T>::isClassRegistered(const std::string & name) const
{
    for (typename RegisteredClassContainer::const_iterator it = registered_class_.begin();
         it != registered_class_.end();
         ++it)
    {
        if (it->first == name) return true;
    }
    
    return false;
}


//------------------------------------------------------------------------------
/**
 *  \todo win error message
 */
template<class T>
void ClassLoader<T>::unregisterAll()
{
    for (HandleIterator it = open_handle_.begin();
         it != open_handle_.end();
         ++it)
    {
#if _WIN32
        if (FreeLibrary((HMODULE)*it) == 0)
        {
            s_log << Log::error << "FreeLibrary error\n ";
        }
#else        
        if (dlclose(*it) != 0)
        {
            s_log << Log::error << "dlclose error: " << dlerror() << "n";
        }
#endif        
    }

    open_handle_.clear();
    registered_class_.clear();
}
