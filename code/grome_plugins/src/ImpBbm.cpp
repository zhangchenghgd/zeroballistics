

#include "ImpBbm.h"
#include <stdio.h>

#include "bbmloader/BbmImporter.h"


#include "UtilsGrome.h"
#include "BbmGromeConverter.h"

#undef max
#undef min



//------------------------------------------------------------------------------
void* GetPluginInterface(void *sdk_root) 
{
    g_sdk_root = (iRootInterface*)sdk_root;

    // Verify if the editor SDK version (the parameter SDK root interface version) 
    // is the same as the SDK version this exporter was written for.
    if(g_sdk_root->Version() != C_CORE_SDK_VERSION)
    {
        // Different version, refuse to start.
        g_sdk_root = NULL;
        return NULL;
    }

    // Obtain the storage manager interface.
    g_storage_man = (iStorageManager*)g_sdk_root->GetInterface(C_STORAGEMANAGER_INTERFACE_NAME);
    if(!g_storage_man)
        return NULL;

    return &g_storage_plugin; 
}

cStorage g_storage_plugin;

iRootInterface *g_sdk_root = NULL;
iStorageManager *g_storage_man = NULL;

HINSTANCE g_hInstance;


//------------------------------------------------------------------------------
cStorage::cStorage()
{
    // TODO: add per plugin init code here.
}

//------------------------------------------------------------------------------
cStorage::~cStorage()
{
    // TODO: add per plugin release code here.
}

//------------------------------------------------------------------------------
t_type_id cStorage::Type()
{
    // Return the type id from the SDK root.
    if(g_sdk_root)
    {
        return g_sdk_root->GetTypeId("Plugin");
    }
    return NULL;
}

//------------------------------------------------------------------------------
void cStorage::OnUnloadModule()
{
    // TODO: add code to be called when the plugin module is released.
}

//------------------------------------------------------------------------------
const char* cStorage::NodeFactory()
{	
    return "Bbm Mesh Storage";
}

//------------------------------------------------------------------------------
cNodeData* cStorage::NewNodeData()
{
//	msgBox("newnodedata                                                                                  ");
    return new cStorageData;
}

//------------------------------------------------------------------------------
void cStorage::ReleaseNodeData(cNodeData *node_data)
{
//	msgBox("releasenodedata                                                                                 ");
    delete((cStorageData*)node_data);
}


//------------------------------------------------------------------------------
/*! \return The extension for our storage files. */
const t_char *cStorage::GetFormatSignature()
{	
    return M_SZ(".bbm");
}

//------------------------------------------------------------------------------
t_error cStorage::ValidateFormat(const t_char *dev_name, iStorage *parent)
{
    return C_GENERIC_SUCCESS;
}

//------------------------------------------------------------------------------
t_error cStorage::SetParentStorage(cNodeData *storage_node_data, iStorage *parent)
{
    return C_GENERIC_SUCCESS;
}

//------------------------------------------------------------------------------
t_error cStorage::SetDevice(cNodeData *storage_node_data, const t_char *dev_name, const t_char *full_dev_path)
{
    assert(full_dev_path);
    assert(dev_name);

    cStorageData* data = (cStorageData*)storage_node_data;
    data->path_ = full_dev_path;
    data->name_ = dev_name;

    // TODO: other code to be executed at init time when the storage device (file path) is set.
//	msgBox(std::wstring(M_SZ("SetDevice:                ")) + dev_name + M_SZ(", ") + full_dev_path);

    return C_GENERIC_SUCCESS;
}

//------------------------------------------------------------------------------
const t_char *cStorage::GetDeviceName(cNodeData *storage_node_data)
{
//	msgBox(std::wstring(M_SZ("getdevicename                              ")) + ((cStorageData*)storage_node_data)->name_);
    return ((cStorageData*)storage_node_data)->name_.c_str();
}

//------------------------------------------------------------------------------
const t_char *cStorage::GetDevicePath(cNodeData *storage_node_data)
{
//	msgBox(std::wstring(M_SZ("getdevicepath                             ")) + ((cStorageData*)storage_node_data)->path_);
    return ((cStorageData*)storage_node_data)->path_.c_str();
}



//------------------------------------------------------------------------------
t_bool cStorage::IsParsed(cNodeData *storage_node_data, const int parse_level)
{
    cStorageData *storage_data = dynamic_cast<cStorageData*>(storage_node_data);
    assert(storage_data);

//	msgBox(M_SZ("isparsed                                               ") + storage_data->path_);

    return storage_data->is_parsed_;
}

//------------------------------------------------------------------------------
t_error cStorage::Parse(cNodeData *storage_node_data, const int parse_level)
{
//	msgBox("parse                                            " + toString(parse_level));

    cStorageData *storage_data = dynamic_cast<cStorageData*>(storage_node_data);
    assert(storage_data);

    storage_data->is_parsed_ = true;
    return C_GENERIC_SUCCESS;
}




//------------------------------------------------------------------------------
uint cStorage::GetSubstoragesNo(cNodeData *storage_node_data)
{
    // TODO: return the numner of substorages this storage contain.
    return 0;
}


//------------------------------------------------------------------------------
t_error cStorage::GetSubstorageInfo(cNodeData *storage_node_data, const uint index, sStoragePluginNodeInfo *o_info)
{
    assert(false);
    // TODO: return information about a substorage.
    return C_NOTIMPLEMENTED_ERR;
}

//------------------------------------------------------------------------------
uint cStorage::GetNodesNo(cNodeData *storage_node_data)
{
    cStorageData *storage_data = dynamic_cast<cStorageData*>(storage_node_data);
    assert(storage_data);

//	msgBox(std::wstring(M_SZ("getnodeno                                         ")) + storage_data->path_);
	
    return storage_data->is_parsed_ ? 1 : 0;
}

//------------------------------------------------------------------------------
t_error cStorage::GetNodeInfo(cNodeData *storage_node_data, const uint index, sStoragePluginNodeInfo *o_info)
{
    cStorageData *storage_data = dynamic_cast<cStorageData*>(storage_node_data);
    assert(storage_data);
    assert(index==0);
	
//	msgBox(std::wstring(M_SZ("getnodeinfo ") + storage_data->path_));

    std::string node_name = fromUnicode(storage_data->name_);
    o_info->node_name              = (const char*)malloc(node_name.size()+1);
    strcpy((char*)o_info->node_name, node_name.c_str());

    o_info->node_factory           = C_NODE_FACTORY_GEOMENTITYTEMPLATE;
    o_info->node_interface_type    = iGeomEntityTemplate::TypeString();
    o_info->node_localization_data = new std::wstring(storage_data->path_);

    return C_GENERIC_SUCCESS;
}

//------------------------------------------------------------------------------
t_error cStorage::ExtractNodeData(
    cNodeData *storage_node_data, void *node_localization_data, iSdkInterface* node_interface)
{
    cStorageData *storage_data = dynamic_cast<cStorageData*>(storage_node_data);
    assert(storage_data);

//	msgBox("extractnodedata                                                                 ");

    if(!node_localization_data) // Strange, we don't have localization data.
        return C_NULLOBJ_ERR;

    std::auto_ptr<bbm::Node> root_node;
    try
    {
        root_node.reset(bbm::Node::loadFromFile(fromUnicode(storage_data->path_)));
    } catch (Exception & e)
    {
        msgBox(e.getTotalErrorString(), "Error loading bbm model");
        return C_GENERIC_ERROR;
    } catch (std::exception & e)
    {
        msgBox(e.what(), "std exception");
        return C_GENERIC_ERROR;
    } catch (...)
    {
        msgBox("Unknwon exception", "Error");
        return C_GENERIC_ERROR;
    }


    iGeomEntityTemplateModifier *modifier = (iGeomEntityTemplateModifier*)node_interface->
        OpenSubinterface(g_sdk_root->GetTypeId(iGeomEntityTemplateModifier::TypeString()));
    assert(modifier);
	


    // strip models/bla.bbm to get data path
    std::wstring data_path = storage_data->path_;
    std::string::size_type s = data_path.rfind(M_SZ("models"));
    assert(s != std::string::npos);
    data_path.resize(s);

    try
    {
        BbmGromeVisitor v(modifier, fromUnicode(data_path));
        root_node->accept(v);
    } 
    catch (AssertException & e)
    {
        return C_GENERIC_ERROR;
    } catch (Exception & e)
    {
        msgBox(e.getTotalErrorString(), "Error converting bbm model");
        return C_GENERIC_ERROR;
    }
	

    return C_GENERIC_SUCCESS;
}
