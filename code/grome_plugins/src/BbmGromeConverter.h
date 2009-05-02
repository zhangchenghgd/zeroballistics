

#ifndef BBM_GROME_CONVERTER_INCLUDED_
#define BBM_GROME_CONVERTER_INCLUDED_


#include <bbmloader/BbmImporter.h>
#include <Engine/sdk.h>

using namespace csdk;


//------------------------------------------------------------------------------
class BbmGromeVisitor : public bbm::NodeVisitor
{
 public:

    BbmGromeVisitor(iGeomEntityTemplateModifier * mod, const std::string & data_path);
    virtual ~BbmGromeVisitor();

    virtual void apply(bbm::Node*);
    virtual void apply(bbm::MeshNode*);
    virtual void apply(bbm::GroupNode*);
    virtual void apply(bbm::EffectNode*);

    virtual void pop();

 protected:

    iGeomEntityTemplateModifier * modifier_;
    std::string data_path_;

    std::vector<std::string> displayed_groups_;

};


#endif
