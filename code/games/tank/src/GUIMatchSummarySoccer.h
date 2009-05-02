
#ifndef TANK_GUIMATCHSUMMARYSOCCER_INCLUDED
#define TANK_GUIMATCHSUMMARYSOCCER_INCLUDED

#include <CEGUI/CEGUI.h>

#include "GUIMatchSummary.h" 

class PuppetMasterClient;

//------------------------------------------------------------------------------
class GUIMatchSummarySoccer : public GUIMatchSummary
{
 public:
    GUIMatchSummarySoccer(PuppetMasterClient * puppet_master);
    virtual ~GUIMatchSummarySoccer();
     
    void setMatchSummaryText(const std::string & left_text,
                             const std::string & right_text);

    void addSkillElement(const std::string & name,
                         const std::string & skill,
                         const std::string & delta,
                         bool selected);
    
 protected:

    virtual void customizeLayout();

};


#endif
