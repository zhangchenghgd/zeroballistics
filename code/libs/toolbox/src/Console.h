


#ifndef RACING_CONSOLE_INCLUDED
#define RACING_CONSOLE_INCLUDED

#include <string>
#include <vector>
#include <iostream>

#include <loki/Functor.h>


#include "Singleton.h"
#include "TextValue.h"
#include "RegisteredFpGroup.h"
#include "Observable.h"

typedef Loki::Functor<void, LOKI_TYPELIST_1(const std::string &) > PrintCallback;

typedef Loki::Functor<std::string, LOKI_TYPELIST_1(const std::vector<std::string>&) > ConsoleFun;
typedef Loki::Functor<std::vector<std::string>,
                      LOKI_TYPELIST_1(const std::vector<std::string>&) > ConsoleCompletionFun;

const unsigned INVALID_INDEX = (unsigned)-1;




//------------------------------------------------------------------------------
/**
 *  \see RegisteredFpGroup.
 */
class ConsoleFunFp : public RegisteredFp
{
 public:
    ConsoleFunFp(const std::string & name = "");
    
    virtual const std::string & toString() const;
    virtual bool operator==(const RegisteredFp & other) const;
    virtual void deregisterPointer() const;

 protected:
    std::string name_;
};



//------------------------------------------------------------------------------
/**
 *  \see RegisteredFpGroup.
 */
class ConsoleVariableFp : public RegisteredFp
{
 public:
    ConsoleVariableFp(const std::string & name = "");
    
    virtual const std::string & toString() const;
    virtual bool operator==(const RegisteredFp & other) const;
    virtual void deregisterPointer() const;
    
 protected:
    std::string name_;
};




//------------------------------------------------------------------------------
class RegisteredConsoleFun
{
 public:
    RegisteredConsoleFun(const std::string & name) : name_(name) {}
    virtual ~RegisteredConsoleFun() {}
    
    virtual std::string operator()(const std::vector<std::string>&args)             = 0;
    virtual std::vector<std::string> complete(const std::vector<std::string>& args) = 0;

    std::string name_;    
};

//------------------------------------------------------------------------------
class RegisteredConsoleFunWithArgs : public RegisteredConsoleFun
{
 public:
    RegisteredConsoleFunWithArgs(const std::string & name,
                                 ConsoleFun fun,
                                 ConsoleCompletionFun completion_fun) :
        RegisteredConsoleFun(name),
        fun_(fun), completion_fun_(completion_fun) {}

    
    virtual std::string operator()(const std::vector<std::string>&args)
        {
            return fun_(args);
        }
    
    virtual std::vector<std::string> complete(const std::vector<std::string>& args)
        {
            return completion_fun_(args);
        }
    
 protected:
    ConsoleFun fun_;
    ConsoleCompletionFun completion_fun_;
};

//------------------------------------------------------------------------------
template <typename T>
class RegisteredConsoleFunWithoutArgs : public RegisteredConsoleFun
{
 public:
    RegisteredConsoleFunWithoutArgs(const std::string & name,
                                    Loki::Functor<T> fun) :
        RegisteredConsoleFun(name),
        fun_(fun) {}

    virtual std::string operator()(const std::vector<std::string>&args)
        {
            if (!args.empty()) return "Function takes no arguments";

            return toString(fun_());
        }
    virtual std::vector<std::string> complete(const std::vector<std::string>& args)
        {
            return std::vector<std::string>();
        }
    
 protected:
    Loki::Functor<T> fun_;          
};

//------------------------------------------------------------------------------
template <>
class RegisteredConsoleFunWithoutArgs<void> : public RegisteredConsoleFun
{
 public:
    RegisteredConsoleFunWithoutArgs(const std::string & name,
                                    Loki::Functor<void> fun) :
        RegisteredConsoleFun(name),
        fun_(fun) {}

    virtual std::string operator()(const std::vector<std::string>&args)
        {
            if (!args.empty()) return "Function takes no arguments";
            fun_();
            return "";
        }
    virtual std::vector<std::string> complete(const std::vector<std::string>& args)
        {
            return std::vector<std::string>();
        }
    
 protected:
    Loki::Functor<void> fun_;          
};



#define s_console Loki::SingletonHolder<Console, Loki::CreateUsingNew, SingletonConsoleLifetime>::Instance()
//------------------------------------------------------------------------------
class Console : public Observable
{
    friend class ConsoleFunFp;
    friend class ConsoleVariableFp;
    
    DECLARE_SINGLETON(Console);
 public:
    virtual ~Console();

#ifndef DEDICATED_SERVER    
    void storeState(const std::string & file) const;
    void restoreState(void * file);
#endif    
    
    std::string executeCommand(const char * cmd);
    const std::vector<std::string> getCompletions(std::string & partial_input);


    std::string getNextHistItem();
    std::string getPrevHistItem();    

    void addFunction(const char * name,
                     ConsoleFun fun,
                     RegisteredFpGroup * group,
                     ConsoleCompletionFun completion_fun = ConsoleCompletionFun(&Console::noCompletion));

    template <typename T>
    void addFunction(const char * name,
                     Loki::Functor<T> fun,
                     RegisteredFpGroup * group);    

    template<class T>
    void addVariable(const char * name, T * var,
                     RegisteredFpGroup * group);
    

    void print(const std::string & msg) const;
    void setPrintCallback(PrintCallback cb);

    // Autocompletion
    std::vector<std::string> completeVariable(const std::vector<std::string> & args);
    
    // Predefined commands
    std::string setVariable(const std::vector<std::string> & args);
    std::string printVariable(const std::vector<std::string> & args);
#ifndef DEDICATED_SERVER    
    std::string plotVariable(const std::vector<std::string> & args);
#endif
    
    RegisteredFpGroup * getFpGroup();
    
    static std::vector<std::string> noCompletion(const std::vector<std::string>&);
    static void dummyPrint(const std::string&);

    static void filterPrefix(std::string prefix, std::vector<std::string> & strings);
    
 protected:

    void removeFunction(const char * name);
    void removeVariable(const char * name);

    
    std::vector<unsigned> getMatchingCommands(const std::string & prefix) const;
    unsigned getCommand(const std::string & name) const;

    std::vector<unsigned> getMatchingVariables(const std::string & prefix) const;
    unsigned getVariable(const std::string & name) const;

    



    std::string completeToCommonPrefix(const std::string & partial_input,
                                       const std::vector<std::string> & completions);



    std::vector<std::string> history_;
    unsigned history_pos_;


    /// The registered variables which can be plotted, changed, etc.
    std::vector<std::pair<std::string, TextValue*> >  variable_;

    /// The registered functions which can be called.
    std::vector<RegisteredConsoleFun*>  function_;


    /// Print commands go to this function, which can output them on
    /// cout, display them on screen etc.
    PrintCallback print_callback_; 

    RegisteredFpGroup fp_group_;
};


//------------------------------------------------------------------------------
template <typename T>
void Console::addFunction(const char * name,
                          Loki::Functor<T> fun,
                          RegisteredFpGroup * group)
{
    group->addFunctionPointer(new ConsoleFunFp(name));
    function_.push_back(new RegisteredConsoleFunWithoutArgs<T>(name, fun));
}


//------------------------------------------------------------------------------
template<class T>
void Console::addVariable(const char * name, T * var,
                          RegisteredFpGroup * group)
{
    std::string unique_var_name = name;
    
    if (getVariable(name) != INVALID_INDEX)
    {
        for (unsigned i=1; ; ++i)
        {
            std::ostringstream strstr;
            strstr << name << "(" << i << ")";
            if (getVariable(strstr.str().c_str()) == INVALID_INDEX)
            {
                unique_var_name = strstr.str();
                break;
            }
        }
    }

    group->addFunctionPointer(new ConsoleVariableFp(unique_var_name.c_str()));
    variable_.push_back(std::make_pair(unique_var_name.c_str(), new TextValuePointer<T>(unique_var_name.c_str(), var)));
}


#define ADD_STATIC_CONSOLE_VAR(type, a, init_value) \
static type a = init_value; \
{ static bool first = 1; if (first) { s_console.addVariable(#a, &a, s_console.getFpGroup()); first = false; }}


#define ADD_LOCAL_CONSOLE_VAR(type, a) \
{ static bool first = true; static type c_var; c_var = a; \
if (first) { first = false; s_console.addVariable(#a, &c_var, s_console.getFpGroup()); }}



#endif // #ifndef RACING_CONSOLE_INCLUDED
