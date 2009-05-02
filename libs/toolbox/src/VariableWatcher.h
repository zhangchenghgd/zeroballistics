/*******************************************************************************
 *
 *  Copyright 2004 Muschick Christian
 *  
 *  This file is part of Lear.
 *  
 *  Lear is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  Lear is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with Lear; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * 
 *  -----------------------------------------------------------------------------
 *
 *  filename            : VariableWatcher.h
 *  author              : Muschick Christian
 *  date of creation    : 31.08.2003
 *  date of last change : 19.09.2004
 *
 *
 *******************************************************************************/

//#ifdef _MSC_VER
//#pragma warning (disable : 4786) //"Bezeichner wurde auf '255' Zeichen in den Debug-Informationen verkürzt"
//#endif


#ifndef TANK_WATCHER_INCLUDED
#define TANK_WATCHER_INCLUDED


#include <ostream>
#include <list>
#include <sstream>

#include "Singleton.h"


#include "TextValue.h"

const unsigned NUM_WATCHER_SAMPLES = 300;

//------------------------------------------------------------------------------
class GraphedVar
{
 public:
    GraphedVar(const TextValue * text_value);
    virtual ~GraphedVar();

    void frameMove();
    const std::string & getCurValue() const;

    const std::string & getName() const;
    float getScaledValue(unsigned pos) const;
    void calcMinMax(float & min, float & max) const;
    
 protected:
    GraphedVar(const GraphedVar&);
    GraphedVar & operator=(const GraphedVar&);
    
    const TextValue * text_value_;
    mutable float max_; ///< The maximum value the variable currently has in the watched interval.
    mutable float min_; ///< The minimum value the variable currently has in the watched interval.
    
    unsigned cur_pos_;         ///< The current position in the value_ array.
    std::vector<float> value_; ///< Cyclic array to store the variable's value history.

    std::string cur_value_;

};


#define s_variable_watcher Loki::SingletonHolder<VariableWatcher, Loki::CreateUsingNew, SingletonVariableWatcherLifetime >::Instance()
//------------------------------------------------------------------------------
/**
 *  Provides a way to watch arbitrary and graph scalar variables
 *  during program execution.
 */
class VariableWatcher
{
    DECLARE_SINGLETON(VariableWatcher);
public:
    ~VariableWatcher();

//------------------------------------------------------------------------------
/**
 *  Add a graphed variable. The type of the variable must be castable
 *  to float.
 *
 *  \param name The name of the variable.
 *  \param watched The address of the variable.
 */
    template<class T>
    void addGraphed(const char * name, T * watched)
    {
        GraphedVar * new_graphed = new GraphedVar(new TextValuePointer<T>(name, watched));

        
        graphed_variable_.push_back(new_graphed);
    }

    void addGraphed(TextValue * watched);

    bool isGraphed(const std::string & name) const;
    void removeGraphed(const std::string & name);    

    void frameMove();
    
    void reset();

    const std::vector<GraphedVar*> & getGraphedVars() const;
    
private:
    typedef std::vector<GraphedVar*> GraphedVariableContainer;
    
    GraphedVariableContainer graphed_variable_;
};



#endif // #ifndef STUNTS_WATCHER_INCLUDED

