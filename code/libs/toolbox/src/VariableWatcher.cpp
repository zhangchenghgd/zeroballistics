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
 *  filename            : VariableWatcher.cpp
 *  author              : Muschick Christian
 *  date of creation    : 31.08.2003
 *  date of last change : 19.09.2004
 *
 *  Implementation file for the class VariableWatcher.
 *
 *******************************************************************************/

#include "VariableWatcher.h"

#include "Log.h"

#undef min
#undef max

//------------------------------------------------------------------------------
/**
 *  \param text_value The address of the variable.
 */
GraphedVar::GraphedVar(const TextValue * text_value) :
    text_value_(text_value), max_(1.0f), min_(-1.0f), 
    cur_pos_(0), value_(NUM_WATCHER_SAMPLES)
{
}

//------------------------------------------------------------------------------
GraphedVar::~GraphedVar()
{
    delete text_value_;
}


//------------------------------------------------------------------------------
void GraphedVar::frameMove()
{
    std::ostringstream o_stream;   
    text_value_->writeToStream(o_stream);
    std::istringstream i_stream(o_stream.str());

    i_stream >> value_[cur_pos_++];
    if (cur_pos_ == value_.size()) cur_pos_ = 0;

    cur_value_ = o_stream.str();
}


//------------------------------------------------------------------------------
const std::string & GraphedVar::getCurValue() const
{
    return cur_value_;
}

//------------------------------------------------------------------------------
const std::string & GraphedVar::getName() const
{
    return text_value_->getName();
}


//------------------------------------------------------------------------------
/**
 *  Returns the value at the specified pos scaled in the interval
 *  [-0.5;0.5]
 */
float GraphedVar::getScaledValue(unsigned pos) const
{
    assert(!equalsZero(max_ - min_));
    
    unsigned index = cur_pos_ + pos;
    if (index >= value_.size()) index -= value_.size();

    return (value_[index]-min_) / (max_ - min_) - 0.5f;
}



//------------------------------------------------------------------------------
void GraphedVar::calcMinMax(float & min, float & max) const
{
    max_ = *std::max_element(value_.begin(), value_.end());
    min_ = *std::min_element(value_.begin(), value_.end());

    if (equalsZero(max_ - min_))
    {
        float offset = std::max(abs(min_)*0.01f, 0.01f);
        max_ += offset;
        min_ -= offset;
    }
    
    min=min_;
    max=max_;

    assert(!equalsZero(max_ - min_));    
}



//------------------------------------------------------------------------------
VariableWatcher::VariableWatcher()
{
}

//------------------------------------------------------------------------------
VariableWatcher::~VariableWatcher()
{
    reset();
}

//------------------------------------------------------------------------------
void VariableWatcher::addGraphed(TextValue * watched)
{
    GraphedVar * new_graphed = new GraphedVar(watched->clone());
    graphed_variable_.push_back(new_graphed);    
}


//------------------------------------------------------------------------------
bool VariableWatcher::isGraphed(const std::string & name) const
{
    for (GraphedVariableContainer::const_iterator it = graphed_variable_.begin();
         it != graphed_variable_.end();
         ++it)
    {
        if ((*it)->getName() == name) return true;
    }

    return false;
}


//------------------------------------------------------------------------------
void VariableWatcher::removeGraphed(const std::string & name)
{
    for (GraphedVariableContainer::iterator it = graphed_variable_.begin();
         it != graphed_variable_.end();
         ++it)
    {
        if ((*it)->getName() == name)
        {
            delete *it;
            graphed_variable_.erase(it);
            return;
        }
    }
}


//------------------------------------------------------------------------------
/**
 *  Updates all graphed variables.
 */
void VariableWatcher::frameMove()
{
    for (GraphedVariableContainer::iterator it = graphed_variable_.begin();
         it != graphed_variable_.end();
         ++it)
    {
        (*it)->frameMove();
    }
}

//------------------------------------------------------------------------------
/**
 *  Removes all watched variables.
 */
void VariableWatcher::reset()
{
    for (GraphedVariableContainer::iterator cur_graphed = graphed_variable_.begin();
         cur_graphed != graphed_variable_.end();
         ++cur_graphed)
    {
        delete *cur_graphed;
    }
    graphed_variable_.clear();
}

//------------------------------------------------------------------------------
const std::vector<GraphedVar*> & VariableWatcher::getGraphedVars() const
{
    return graphed_variable_;
}
