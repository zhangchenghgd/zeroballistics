
#ifndef __STORAGE_H
#define __STORAGE_H


#include "Datatypes.h" // instead of windows.h for correct include order

#include "tchar.h"

#include <Engine/sdk.h>

// Use the SDK namespace.
using namespace csdk;

//====================================================

//! Structure to hold information for every storage node.
struct cStorageData: public cNodeData
{	
    cStorageData() : is_parsed_(false) {}

    std::wstring name_;
    std::wstring path_;

    bool is_parsed_;
};

//====================================================
// Node localization data.

//! Possible nodes we can have in a storage.
enum E_STORAGE_NODE_TYPE
{
    // TODO: add constants for your support types of nodes (textures, images, geometric templates).
};

//! Base node localization data.
/*! TODO: inherit specific node types structures from this one. */
class cStorageNodeData
{
    friend class cStorage;

 protected:

    virtual ~cStorageNodeData() { }

    //! Type of this node.
    E_STORAGE_NODE_TYPE _type;

    // ...
};

//====================================================

//! Storage interface implementation.
class cStorage: public iStoragePlugin
{
 public:

    cStorage();
    ~cStorage();

    //! Return the long name (the description) of the interface.
    virtual const char* Description() 
	{ 
            // TODO: add your own plugin description here.
#		ifdef D_DEBUG
            return "Bbm Mesh Import Plugin (Debug)"; 
#		else
            return "Bbm Mesh Import Plugin"; 
#		endif
	}

    //! Return the id identifying the type of the interface.
    virtual t_type_id Type();

    virtual const char* PluginName() 
	{ 
            // TODO: add your own plugin name here.
#		ifdef D_DEBUG
            return "Bbm Mesh Import Plugin (Debug)"; 
#		else
            return "Bbm Mesh Import Plugin"; 
#		endif
	}

    //! Return the author of the plug-in.
    virtual const char* Vendor() 
	{ 
            // TODO: add your company name here.
            return "Quanticode"; 
	}
    //! Return any copyright text.
    virtual const char* CopyrightNotice() 
	{ 
            // TODO: add your own compyright notice.
            return "Copyright 2007 Quanticode"; 
	}

    //! Called by the engine when the module from which the plug-in is created is unloaded.
    virtual void OnUnloadModule();

    // Node plug-in specific interface ===========================

    //! Name of the node factory to be used as parameter for iRootInterface::NewNode(factory_name).
    virtual const char* NodeFactory();

    //! Called by the system to create the node custom data.
    virtual cNodeData* NewNodeData();

    //! Release node data created by this plug-in.
    virtual void ReleaseNodeData(cNodeData *node_data);

    // Storage plug-in specific interface =========================

    //! Return the signature identifying the storage on the disc during disc parsing.
    virtual const t_char *GetFormatSignature();

    //! Called for a device (e.g. a file) with a signature as defined by this plug-in to determine if the format is valid.
    virtual t_error ValidateFormat(const t_char *dev_name, iStorage *parent);

    //! Called by the system to set the parent storage.
    virtual t_error SetParentStorage(cNodeData *storage_node_data, iStorage *parent);

    //! Set the destination device this storage is pointing to (e.g. file, folder etc.).
    virtual t_error SetDevice(cNodeData *storage_node_data, const t_char *dev_name, const t_char *full_dev_path = NULL);

    //! Return the path for the current device (example: "my_storage.dae").
    virtual const t_char *GetDeviceName(cNodeData *storage_node_data);

    //! Return the complete path for the device (example: "c:/temp/my_storage.dae").
    virtual const t_char *GetDevicePath(cNodeData *storage_node_data);

    //! Check if the storage is parsed.
    virtual t_bool IsParsed(cNodeData *storage_node_data, const int parse_level = 0);

    //! Parse the device and announce the components to the system.
    virtual t_error Parse(cNodeData *storage_node_data, const int parse_level = 0);

    //! Return the total no of direct sub (or child) storages.
    virtual uint GetSubstoragesNo(cNodeData *storage_node_data);

    //! Called by the system to get information about a substorage.
    virtual t_error GetSubstorageInfo(cNodeData *storage_node_data, const uint index, sStoragePluginNodeInfo *o_info);

    //! Return the no of nodes in the storage.
    virtual uint GetNodesNo(cNodeData *storage_node_data);

    //! Return information about a node in the storage.
    virtual t_error GetNodeInfo(cNodeData *storage_node_data, const uint index, sStoragePluginNodeInfo *o_info);

    //! Called by the system when data for a node must be extracted.
    virtual t_error ExtractNodeData(cNodeData *storage_node_data, void *node_localization_data, iSdkInterface* node_interface);

    // Implementation =====================================

 protected:

    // TODO: add other functions and variables global per plugin.

};

//====================================================

//! Plug-in export function.
extern "C" D_EXPORT void* GetPluginInterface(void *sdk_root);

//! Plug-in instance.
extern cStorage g_storage_plugin;

//! Pointer to the SDK root interface (received in \c GetPluginInterface).
extern iRootInterface *g_sdk_root;
//! Pointer to the storage manager used for various file related operations.
extern iStorageManager *g_storage_man;

//! [Windows specific] Handle to the module (need this for the plug-in dialog)
extern HINSTANCE g_hInstance;

//====================================================
#endif
/*@}*/
