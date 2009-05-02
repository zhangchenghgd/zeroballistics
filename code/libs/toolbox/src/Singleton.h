
#ifndef LIB_SINGLETON_INCLUDED
#define LIB_SINGLETON_INCLUDED

#include <loki/Singleton.h>


/// defines destruction order of singletons
/// lowest first, highest last
enum SINGLETON_LONGEVITY_LIFETIME
{
    SLL_DEFAULT = 1,
    SLL_PARTICLE_MANAGER,
    SLL_EFFECT_LOADER,
    SLL_SCENE_MANAGER,
    SLL_SDL_APP,
    SLL_INPUT_HANDLER,
    SLL_SCHEDULER,
    SLL_ODE_MODEL_LOADER,
    SLL_LOG,
    SLL_PARAMETER_MANAGER,
    SLL_CONSOLE,
    SLL_VARIABLE_WATCHER
};

template<class T>
struct SingletonDefaultLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_DEFAULT ,T> {};

template<class T>
struct SingletonVariableWatcherLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_VARIABLE_WATCHER ,T> {};

template<class T>
struct SingletonConsoleLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_CONSOLE ,T> {};

template<class T>
struct SingletonInputHandlerLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_INPUT_HANDLER ,T> {};

template<class T>
struct SingletonParameterManagerLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_PARAMETER_MANAGER ,T> {};

template<class T>
struct SingletonSceneManagerLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_SCENE_MANAGER ,T> {};

template<class T>
struct SingletonParticleManagerLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_PARTICLE_MANAGER ,T> {};

template<class T>
struct SingletonEffectManagerLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_EFFECT_LOADER ,T> {};

template<class T>
struct SingletonLogLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_LOG ,T> {};

template<class T>
struct SingletonOdeModelLoaderLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_ODE_MODEL_LOADER ,T> {};

template<class T>
struct SingletonSchedulerLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_SCHEDULER ,T> {};

template<class T>
struct SingletonSdlAppLifetime  : Loki::LongevityLifetime::SingletonFixedLongevity< SLL_SDL_APP ,T> {};


//------------------------------------------------------------------------------
namespace Loki
{

//------------------------------------------------------------------------------
/**
 *  This policy lets a static create function do the job of
 *  instantiating the class. This way, a subclass can be dynamically
 *  selected at creation time.
 */
template <class T> struct CreateUsingCreate
{
    static T*   Create()      { return T::create(); }
    static void Destroy(T* p) { delete p; }
};

} // namespace Loki



// Protects a class from accidential instantiation or assignment and
// makes friend with the CreateUsingNew policy.
#define DECLARE_SINGLETON(CLASS) \
friend struct Loki::CreateUsingNew<CLASS>; \
protected: \
CLASS(); \
CLASS(const CLASS&); \
CLASS & operator=(const CLASS&);


#endif // #ifndef RACING_SINGLETON_INCLUDED
