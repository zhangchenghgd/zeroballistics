
#include "Console.h"


#include <set>
#include <fstream>

#include "Tokenizer.h"
#include "VariableWatcher.h"
#include "Log.h"

#include "Cwd.h"

//------------------------------------------------------------------------------
ConsoleFunFp::ConsoleFunFp(const std::string & name) :
    name_(name)
{
}


//------------------------------------------------------------------------------
const std::string & ConsoleFunFp::toString() const
{
    return name_;
}

//------------------------------------------------------------------------------
bool ConsoleFunFp::operator==(const RegisteredFp & other) const
{
    const ConsoleFunFp * other_cf = dynamic_cast<const ConsoleFunFp*>(&other);
    if (!other_cf) return false;

    return name_ == other_cf->name_;
}

//------------------------------------------------------------------------------
void ConsoleFunFp::deregisterPointer() const
{
    s_console.removeFunction(name_.c_str());
}

//------------------------------------------------------------------------------
ConsoleVariableFp::ConsoleVariableFp(const std::string & name) :
    name_(name)
{
}

//------------------------------------------------------------------------------
const std::string & ConsoleVariableFp::toString() const
{
    return name_;
}

//------------------------------------------------------------------------------
bool ConsoleVariableFp::operator==(const RegisteredFp & other) const
{
    const ConsoleVariableFp * other_vf = dynamic_cast<const ConsoleVariableFp*>(&other);
    if (!other_vf) return false;

    return name_ == other_vf->name_;
}

//------------------------------------------------------------------------------
void ConsoleVariableFp::deregisterPointer() const
{
    s_console.removeVariable(name_.c_str());
}


//------------------------------------------------------------------------------
Console::Console() : history_pos_(0),
                     print_callback_(&dummyPrint)
{
#ifdef ENABLE_DEV_FEATURES    
    addFunction("set",
                ConsoleFun(this, &Console::setVariable),
                &fp_group_,
                ConsoleCompletionFun(this, &Console::completeVariable));
    addFunction("show",
                ConsoleFun(this, &Console::printVariable),
                &fp_group_,
                ConsoleCompletionFun(this, &Console::completeVariable));
#ifndef DEDICATED_SERVER
    addFunction("plot",
                ConsoleFun(this, &Console::plotVariable),
                &fp_group_,
                ConsoleCompletionFun(this, &Console::completeVariable));
#endif
#endif


#ifdef CWDEBUG
    addFunction("printMemAllocations",
                ConsoleFun(&printPrevAllocations),
                &fp_group_);
    addFunction("togglePrintAllocs",
                ConsoleFun(&togglePrintAllocs),
                &fp_group_);
#endif
}

//------------------------------------------------------------------------------
Console::~Console()
{
    // Clear our own vars & functions so warnings can be properly
    // issued afterwards...
    fp_group_.deregisterAllOfType(ConsoleFunFp());
    fp_group_.deregisterAllOfType(ConsoleVariableFp());
    
    while(!variable_.empty())
    {
        // Log is already gone here...
        std::cerr << Log::warning
                  << "Deleting still registered console variable "
                  << variable_.begin()->first
                  << "\n";
        
        delete variable_.begin()->second;
        variable_.erase(variable_.begin());
    }

    while(!function_.empty())
    {
        std::cerr << Log::warning
                  << "Deleting still registered console function "
                  << (*function_.begin())->name_
                  << "\n";
        
        delete *function_.begin();
        function_.erase(function_.begin());
    }
}

#ifndef DEDICATED_SERVER

//------------------------------------------------------------------------------
void Console::storeState(const std::string & file) const
{
    if (s_variable_watcher.getGraphedVars().empty()) return;
    
    std::ofstream out(file.c_str());

    for (unsigned i=0; i<s_variable_watcher.getGraphedVars().size(); ++i)
    {
        out << s_variable_watcher.getGraphedVars()[i]->getName() << " ";
    }
}


//------------------------------------------------------------------------------
/**
 *  Callable by scheduler.
 *
 *  \param file Pointer to the std::string with the filename.
 */
void Console::restoreState(void * file)
{
    std::string & f = *(std::string*)file;
    
    std::ifstream in(f.c_str());

    std::string graphed_var;
    while(in >> graphed_var)
    {
        if (!s_variable_watcher.isGraphed(graphed_var))
        {
            plotVariable(std::vector<std::string>(1,graphed_var));
        }
    }
}


#endif

//------------------------------------------------------------------------------
std::string Console::executeCommand(const char * cmd)
{
    Tokenizer tokenizer(cmd);

    if (tokenizer.isEmpty()) return "";
    
    history_.push_back(cmd);
    history_pos_ = history_.size()-1;

    unsigned command = getCommand(tokenizer.getNextWord());
    if (command == INVALID_INDEX) return std::string(cmd) + ": Unknown command. Use TAB for autocompletion.";

    std::vector<std::string> args;
    while (!tokenizer.isEmpty())
    {
        args.push_back(tokenizer.getNextWord());
    }

    std::string ret;
    try
    {
        ret = (*function_[command])(args);
    } catch (Exception & e)
    {
        e.addHistory("Console::executeCommand(" + std::string(cmd) + ")");
        s_log << Log::error << e << "\n";
        emit(EE_EXCEPTION_CAUGHT, &e);
        return e.getMessage();
    }
    
    return ret;
}

//------------------------------------------------------------------------------
/**
 *  Returns an array of possible completions for the specified partial
 *  input.
 *
 *  If all possible completions share a common prefix, this prefix is
 *  added to the partial input.
 */
const std::vector<std::string> Console::getCompletions(std::string & partial_input)
{
    std::vector<std::string> ret;

    Tokenizer tokenizer(partial_input.c_str());

    if (tokenizer.isEmpty())
    {
        // No input: print all available commands
        for (unsigned c=0; c<function_.size(); ++c)
        {
            ret.push_back(function_[c]->name_);
        }
    } else
    {
        // Get indices of all matching commands
        std::string command = tokenizer.getNextWord();
        std::vector<unsigned> matching_commands = getMatchingCommands(command);

        if (matching_commands.empty())
        {
            // No registered command matches, do nothing
        } else if (matching_commands.size() == 1)
        {
            if (command != function_[matching_commands[0]]->name_)
            {
                // Funtion name is not complete.
                // Complete name.
                partial_input = function_[matching_commands[0]]->name_ + ' ';
            } else
            {
                // Function name is complete.
                std::string function_name = function_[matching_commands[0]]->name_;
                
                // Append space if it isn't there yet
                if (partial_input.length() == function_name.length())
                    partial_input += ' ';
                
                // Supply additional arguments to command-specific
                // completion fun

                // Tokenize additional args
                std::vector<std::string> args;
                while (!tokenizer.isEmpty()) args.push_back(tokenizer.getNextWord());

                // Get arg completions
                std::vector<std::string> arg_completion =
                    function_[matching_commands[0]]->complete(args);

                if (arg_completion.empty())
                {
                    // No argument completions returned; Return empty
                    // array.
                    return ret;
                } else
                {
                    partial_input = function_name + ' ' +
                        completeToCommonPrefix(partial_input.substr(function_name.length() +1),
                                               arg_completion);

                    if (arg_completion.size() != 1)
                    {
                        return arg_completion;
                    }
                }
            }
            
        } else
        {
            // Multiple matches, complete to common prefix and return
            // them            
            for (unsigned c=0; c<matching_commands.size(); ++c)
            {
                ret.push_back(function_[matching_commands[c]]->name_);
            }

            partial_input = completeToCommonPrefix(partial_input, ret);
        }
    }
    
    return ret;
}


//------------------------------------------------------------------------------
std::string Console::getNextHistItem()
{
    if (history_.empty()) return "";
    
    std::string ret = history_[history_pos_];
    if (history_pos_ == 0) history_pos_ = history_.size()-1;
    else --history_pos_;
    return ret;
}

//------------------------------------------------------------------------------
std::string Console::getPrevHistItem()
{
    if (history_.empty()) return "";

    std::string ret = history_[history_pos_];
    if (history_pos_ == history_.size()-1) history_pos_ = 0;
    else ++history_pos_;
    return ret;
}


//------------------------------------------------------------------------------
void Console::addFunction(const char * name, ConsoleFun fun,
                          RegisteredFpGroup * group,
                          ConsoleCompletionFun completion_fun)
{
    // Find a unique name for the function
    std::string unique_fun_name = name;
    if (getCommand(name) != INVALID_INDEX)
    {
        for (unsigned i=1; ; ++i)
        {
            std::ostringstream strstr;
            strstr << name << "(" << i << ")";
            if (getCommand(strstr.str().c_str()) == INVALID_INDEX)
            {
                unique_fun_name = strstr.str();
                break;
            }
        }
    }
    
    // Now add the new command
    group->addFunctionPointer(new ConsoleFunFp(unique_fun_name));
    function_.push_back(new RegisteredConsoleFunWithArgs(unique_fun_name, fun, completion_fun));
}


//------------------------------------------------------------------------------
void Console::print(const std::string & msg) const
{
    print_callback_(msg);
}

//------------------------------------------------------------------------------
void Console::setPrintCallback(PrintCallback cb)
{
    print_callback_ = cb;
}


//------------------------------------------------------------------------------
/**
 *  
 */
std::vector<std::string> Console::completeVariable(const std::vector<std::string> & args)
{
    std::vector<std::string> ret;

    // We accept only 1 argument
    if (args.size() > 1) return ret;


    std::set<std::string> matching_set;
    
    if (args.empty())
    {
        for (unsigned v=0; v<variable_.size(); ++v)
        {
            ret.push_back(variable_[v].first);
            matching_set.insert(variable_[v].first.substr(0, variable_[v].first.find('.')));
        }

    } else
    {
        std::vector<unsigned> matching_vars = getMatchingVariables(args[0]);

        for (unsigned v=0; v<matching_vars.size(); ++v)
        {
            std::string full_name = variable_[matching_vars[v]].first;
            ret.push_back(full_name);
        
            // If there is no dot yet, only show categories
            if (args[0].find('.') == std::string::npos)
            {
                matching_set.insert(full_name.substr(0, full_name.find('.')));
            } else
            {
                matching_set.insert(ret.back());
            }
        }
    }

    if (matching_set.size() == 1)
    {
        if (ret.size() == 1 && args[0].find(ret[0]) != std::string::npos)
        {
            unsigned ind = getVariable(args[0]);
            if (ind == INVALID_INDEX) return ret;

            std::ostringstream stream;
            variable_[ind].second->writeToStream(stream);

            ret[0] += " " + stream.str();
            
            return ret;
        } else
        {
            return ret;
        }
    } else
    {
        ret.clear();

        for (std::set<std::string>::const_iterator it = matching_set.begin();
             it != matching_set.end();
             ++it)
        {
            ret.push_back(*it);
        }
    
        return ret;
    }
}



//------------------------------------------------------------------------------
std::string Console::setVariable(const std::vector<std::string> & args)
{
    if (args.size() < 1) return "Invalid number of arguments.";
    
    unsigned index = getVariable(args[0]);
    if (index == INVALID_INDEX) return "Variable unknown.";

    if (args.size() < 2) return "Invalid number of arguments.";

    
    // We need to reconcatenate the splitted args.
    std::string total_args;
    for (unsigned a=1; a<args.size(); ++a)
    {
        total_args += args[a] + " ";
    }

    std::istringstream str(total_args);

    try
    {
        variable_[index].second->readFromStream(str);
    } catch (Exception & e)
    {
        return e.getTotalErrorString();
    }

    s_log << "Set "
          << args[0]
          << " to "
          << total_args
          << "\n";

    return "";
}

//------------------------------------------------------------------------------
std::string Console::printVariable(const std::vector<std::string> & args)
{
    if (args.size() != 1) return "Invalid number of arguments.";

    unsigned index = getVariable(args[0]);
    if (index == INVALID_INDEX) return "Variable unknown.";

    std::ostringstream str;

    variable_[index].second->writeToStream(str);

    return str.str();
}

#ifndef DEDICATED_SERVER
//------------------------------------------------------------------------------
std::string Console::plotVariable(const std::vector<std::string> & args)
{
    if (args.size() != 1) return "Invalid number of arguments.";

    unsigned index = getVariable(args[0]);
    if (index == INVALID_INDEX) return "Variable unknown.";

    if (s_variable_watcher.isGraphed(args[0]))
    {
        s_variable_watcher.removeGraphed(args[0]);
    } else
    {
        s_variable_watcher.addGraphed(variable_[index].second);
    }

    return "";
}
#endif


//------------------------------------------------------------------------------
RegisteredFpGroup * Console::getFpGroup()
{
    return &fp_group_;
}



//------------------------------------------------------------------------------
/**
 *  Default completion function for commands which don't have
 *  autocompletion for their arguments.
 */
std::vector<std::string> Console::noCompletion(const std::vector<std::string>&)
{
    return std::vector<std::string>();
}


//------------------------------------------------------------------------------
/**
 *  Default print function does nothing.
 */
void Console::dummyPrint(const std::string&)
{
}


//------------------------------------------------------------------------------
/**
 *  Removes all strings which do not have the specified prefix.
 */
void Console::filterPrefix(std::string prefix, std::vector<std::string> & strings)
{
    std::vector<std::string>::iterator it = strings.begin();
    
    while (it != strings.end())
    {
        if (it->find(prefix) == 0)
        {
            ++it;
        } else
        {
            it = strings.erase(it);
        }
    }
}


//------------------------------------------------------------------------------
void Console::removeFunction(const char * name)
{
    unsigned index = getCommand(name);
    if (index != INVALID_INDEX)
    {
        delete function_[index];
        function_.erase(function_.begin() + index);
    } else
    {
        assert(false);
    }
}


//------------------------------------------------------------------------------
void Console::removeVariable(const char * name)
{
    unsigned index = getVariable(name);
    if (index != INVALID_INDEX)
    {
        if (s_variable_watcher.isGraphed(name))
        {
            s_variable_watcher.removeGraphed(name);
        }
        
        delete variable_[index].second;
        variable_.erase(variable_.begin() + index);
    } else
    {
        assert(false);
    }
}

      
//------------------------------------------------------------------------------
/**
 *  Returns the indices of all registered commands starting with the
 *  specified prefix.
 */
std::vector<unsigned> Console::getMatchingCommands(const std::string & prefix) const
{
    std::vector<unsigned> ret;

    for (unsigned c=0; c<function_.size(); ++c)
    {
        if (function_[c]->name_.find(prefix) == 0) ret.push_back(c);
    }
    
    return ret;
}

//------------------------------------------------------------------------------
/**
 *  Returns the index of the command matching exactly the given name
 *  or INVALID_INDEX if such a command does not exist.
 */
unsigned Console::getCommand(const std::string & name) const
{
    for (unsigned c=0; c<function_.size(); ++c)
    {
        if (function_[c]->name_ == name) return c;
    }

    return INVALID_INDEX;
}

//------------------------------------------------------------------------------
std::vector<unsigned> Console::getMatchingVariables(const std::string & prefix) const
{
    std::vector<unsigned> ret;

    for (unsigned c=0; c<variable_.size(); ++c)
    {
        if (variable_[c].first.find(prefix) == (size_t)0)
        {
            ret.push_back(c);
        }
    }
    
    return ret;
}


//------------------------------------------------------------------------------
unsigned Console::getVariable(const std::string & name) const
{
    for (unsigned c=0; c<variable_.size(); ++c)
    {
        if (variable_[c].first == name) return c;
    }

    return INVALID_INDEX;
}







//------------------------------------------------------------------------------
/**
 *  Finds and returns the longest common prefix of partial_input and
 *  the strings in completions, but returns at least partial_input.
 */
std::string Console::completeToCommonPrefix(const std::string & partial_input,
                                            const std::vector<std::string> & completions)
{
    std::string ret = partial_input;
    
    for (unsigned cur_pos = partial_input.length();
         /* compare nothing */;
         ++cur_pos)
    {
        if (completions[0].length() <= cur_pos)
        {
            if (completions.size() == 1) ret += ' '; // TODO move this outside
            return ret;
        }

        assert(cur_pos < completions[0].size());
        
        char next_char = completions[0][cur_pos];
        for (unsigned c=1; c<completions.size(); ++c)
        {
            if (completions[c].length() == cur_pos)   return ret;
            if (completions[c][cur_pos] != next_char) return ret;
        }

        ret += next_char;
    }    
}
