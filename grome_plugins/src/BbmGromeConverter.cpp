


#include "BbmGromeConverter.h"

#include "ImpBbm.h"
#include "UtilsGrome.h"


//------------------------------------------------------------------------------
BbmGromeVisitor::BbmGromeVisitor(iGeomEntityTemplateModifier * mod, const std::string & data_path) : 
    modifier_(mod),
    data_path_(data_path)
{
    displayed_groups_.push_back("lod1");
    displayed_groups_.push_back("weapon0");
    displayed_groups_.push_back("free");
    displayed_groups_.push_back("in");
    displayed_groups_.push_back("grome");
    displayed_groups_.push_back("deployed");
}



//------------------------------------------------------------------------------
BbmGromeVisitor::~BbmGromeVisitor()
{
}


//------------------------------------------------------------------------------
void BbmGromeVisitor::apply(bbm::Node*node)
{
}


//------------------------------------------------------------------------------
void BbmGromeVisitor::apply(bbm::MeshNode* node)
{
    // Check wether this mesh node is to be displayed based on groups
    bool display = true;
    for (unsigned conj_term=0; conj_term<node->getGroups().size(); ++conj_term)
    {
        bool conj_satisfied = false;
        for (unsigned disj_term=0; disj_term<node->getGroups()[conj_term].size(); ++disj_term)
        {
            if (std::find(displayed_groups_.begin(),
                          displayed_groups_.end(),
                          node->getGroups()[conj_term][disj_term]) != displayed_groups_.end())
            {
                conj_satisfied = true;
                break;
            }
        }

        if (!conj_satisfied)
        {
            display = false;
            break;
        }
    }
    if (!display) return;


    t_error  ret;
    Matrix flip(true);	
    flip._33 = -1;

    for (unsigned m=0; m<node->getMeshes().size(); ++m)
    {
        bbm::Mesh * cur_mesh = node->getMeshes()[m];


        // Apply total transform to vertex & normal data
        for (unsigned v=0; v<cur_mesh->getVertexData().size(); ++v)
        {
            cur_mesh->getVertexData()[v] = node->getTotalTransform().transformPoint(cur_mesh->getVertexData()[v]);
            cur_mesh->getVertexData()[v] = flip.transformPoint(cur_mesh->getVertexData()[v]);

        }

        for (unsigned n=0; n<cur_mesh->getNormalData().size(); ++n)
        {
            cur_mesh->getNormalData()[n] = node->getTotalTransform().transformVector(cur_mesh->getNormalData()[n]);
            cur_mesh->getNormalData()[n] = flip.transformVector(cur_mesh->getNormalData()[n]);
        }

        // Convert indices from uint16 to uint32
        std::vector<uint> indices(cur_mesh->getIndexData().size());
        for (unsigned i=0; i<indices.size(); ++i)
        {
            indices[i] = cur_mesh->getIndexData()[i];
        }
		
        cSurfaceDescription surface;
		
        surface.indices_no = indices.size();

        surface.clockwise_indices = true;

        surface.positions                = (void*)&cur_mesh->getVertexData()[0];
        surface.positions_no             = cur_mesh->getVertexData().size();
        surface.positions_indices        = &indices[0];
        surface.positions_indices_stride = 1;

        surface.normals                = (void*)&cur_mesh->getNormalData()[0];
        surface.normals_no             = cur_mesh->getNormalData().size();
        surface.normals_indices        = &indices[0];
        surface.normals_indices_stride = 1;

        // construct name of texture.
        std::wstring tex_path = toUnicode(data_path_) + M_SZ("textures\\models");
        iStorage * img_storage = g_storage_man->GetStorageFromPath(tex_path.c_str());
        assert(img_storage);
		
        ret = img_storage->Parse();
        assert(M_SUCCEED(ret));
        AUTO_CLOSE(img_storage);

        for (unsigned t=0; t<cur_mesh->getTexData().size(); ++t)
        {
            assert(cur_mesh->getTexData()[t].size() == cur_mesh->getVertexData().size());	

            // Create a texture with a default name, without storage.
            iTexture* tex = (iTexture*)g_sdk_root->NewNode(C_NODE_FACTORY_TEXTURE, NULL, NULL, NULL);
            assert(tex);

            // Set the image from the storage as the source of the texture.
            std::string tex_name;
            if (t==0) tex_name = cur_mesh->getTextureName();
            else tex_name = cur_mesh->getLmName();
            tex->SetSourceImage(toUnicode(tex_name).c_str(), img_storage);

            // Now load the image and fill the texture.
            ret = tex->Load(); 
            assert(M_SUCCEED(ret) && "Couldn't load texture");				

            surface.textures[t]               = tex;
            surface.texmaps[t]                = (void*)&cur_mesh->getTexData()[t][0];
            surface.texmaps_no[t]             = cur_mesh->getTexData()[t].size();
            surface.texmaps_indices[t]        = &indices[0];
            surface.texmaps_indices_stride[t] = 1;
        }


        // Now handle the material
        // Obtain the program installation storage.
        iStorage *prog_storage = g_storage_man->GetStorageFromPath(NULL);
        assert(prog_storage);
        assert(prog_storage->GetDevicePath());

        // Construct the path to the standard location of materials.
        // User can search and load materials from other custom locations as well.
        t_string path = prog_storage->GetDevicePath();
        path += M_SZ("/Libraries/Materials");
		
        iStorage *mat_stor = g_storage_man->GetStorageFromPath(path.Sz());
        assert(mat_stor);
/*
  for (unsigned i=0; i<mat_stor->GetNodesNo(); ++i)
  {
  sStorageNodeInfo info;
  mat_stor->GetNodeInfo(i, &info);
  msgBox(info.node_name);
  }
*/
        std::string mat_name;

        if (cur_mesh->getFlags() & bbm::BMO_ALPHA_TEST)
        {
            if (cur_mesh->getFlags() & bbm::BMO_LIGHT_MAP)
            {
                mat_name = "Lightmap-AlphaTest";
            } else
            {
                mat_name = "AlphaTest";
            }
        } else if (cur_mesh->getFlags() & bbm::BMO_LIGHT_MAP)
        {	
            mat_name = "Lightmap";
        }

        if (mat_name.size())
        {
            iMaterial * mat = (iMaterial*)mat_stor->ExtractNode(mat_name.c_str());
            if (mat) surface.material = mat;
            else msgBox("material " + mat_name + " doesn't exist!");
        }


        t_error ret = modifier_->AddNewSurface(&surface);
        assert(M_SUCCEED(ret));			
    }
}


//------------------------------------------------------------------------------
void BbmGromeVisitor::apply(bbm::GroupNode*)
{
}


//------------------------------------------------------------------------------
void BbmGromeVisitor::apply(bbm::EffectNode*)
{
}

//------------------------------------------------------------------------------
void BbmGromeVisitor::pop()
{
}
