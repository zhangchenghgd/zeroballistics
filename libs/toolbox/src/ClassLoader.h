#ifndef BLUEBEARD_CLASSLOADER_INCLUDED
#define BLUEBEARD_CLASSLOADER_INCLUDED

#if _WIN32
#include "Datatypes.h"
#else
#include <dlfcn.h>
#endif


#include <map>
#include <string>
#include <vector>

#include "Singleton.h"

#include "Log.h"

namespace dyn_class_loading
{


//------------------------------------------------------------------------------
/**
 *  Three classes take part in the automatic registration and class loading process:
 *  - Classloader
 *  - RegistratorProxy
 *  - The class defining the interface of the loadable objects (e.g. IController)
 *
 *  First the ClassLoader-Singleton object, parametrized with the
 *  loadable object type, is created in the main program. In order for
 *  the main program to export the symbol for the ClassLoader object
 *  and therefore make it accessible by the dynamically loaded
 *  libraries, the program must be compiled with the linker option
 *  -rdynamic.
 *
 *  Then the ClassLoader can be used to open the desired libraries. In
 *  order for the autoregistration process to work, each library
 *  should define a global variable of type RegistratorProxy,
 *  parametrized with the interface base class and the loadable class
 *  itself. Furthermore, each registeres class must have a static
 *  member function "make" which returns a new instance of the class.
 *
 *  Opening the library then triggers the RegistratorProxy constructor
 *  which does the job of registering the static factory method with
 *  the ClassLoader Singleton object under he name of the class, which
 *  makes it accessible from the main program.
 */
template<class T>
class ClassLoader
{
    typedef T* (*maker_fun_t)();
    friend struct Loki::CreateUsingNew<ClassLoader<T> >; // macro "DECLARE_SINGLETON" doesn't work

 public:
    virtual ~ClassLoader();
    
    void openLibrary(const std::string & filename);
    T * create(const std::string & name);

    void addRegisteredClass(const std::string & name, maker_fun_t maker);
    std::vector<std::string> getRegisteredClassNames() const;

    void unregisterAll();
 private:
    ClassLoader();
    ClassLoader(const ClassLoader<T>&);
    ClassLoader<T> & operator=(const ClassLoader<T>&);
    void operator&();
    
    typedef std::map<std::string, maker_fun_t> RegisteredClassContainer;
    typedef typename RegisteredClassContainer::iterator RegisteredClassIterator;
    typedef typename RegisteredClassContainer::const_iterator ConstRegisteredClassIterator;
    typedef std::vector<void*>::iterator HandleIterator;

    RegisteredClassContainer registered_class_;
    std::vector<void*> open_handle_;
}; 

#include "ClassLoader.tpl"


} // namespace dyn_class_loading

#endif // ifndef BLUEBEARD_CLASSLOADER_INCLUDED

