#ifndef BLUEBEARD_AUTOREGISTRATOR_INCLUDED
#define BLUEBEARD_AUTOREGISTRATOR_INCLUDED

#include "ClassLoader.h"


// Declares an object of type RegistratorProxy which automatically
// registers the class with the ClassLoader upon creation.
#define REGISTER_CLASS(INTERFACE, CLASS) \
namespace { dyn_class_loading::RegistratorProxy<INTERFACE, CLASS> CLASS##RegisterProxy(#CLASS); }



namespace dyn_class_loading
{
    
//------------------------------------------------------------------------------
/**
 *  Used to automatically register the class specified by the template
 *  parameters upon loading of the library.
 *
 *  \see ClassLoader
 */
template <class INTERFACE, class CLASS>
class RegistratorProxy
{
 public:
    RegistratorProxy<INTERFACE, CLASS>(const char * class_name)
        {
            ClassLoader<INTERFACE> & loader = Loki::SingletonHolder<ClassLoader<INTERFACE> , Loki::CreateUsingNew, SingletonDefaultLifetime >::Instance();
            loader.addRegisteredClass(class_name, &CLASS::create);
        } 
};

 
}


#endif // #ifndef BLUEBEARD_AUTOREGISTRATOR_INCLUDED
