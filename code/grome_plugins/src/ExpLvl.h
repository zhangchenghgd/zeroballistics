#ifndef __EXPORTER_H
#define __EXPORTER_H

#include "Datatypes.h" // instead of windows.h for correct include order
#include <tchar.h>


#include <TerrainEd/sdk.h>
#include <ObjectEd/sdk.h>



#include <toolbox/Vector.h>


#include "UtilsGrome.h"


namespace bbm
{
    class LevelData;
    class DetailTexInfo;
}

class LocalParameters;

using namespace csdk;

//------------------------------------------------------------------------------
class cExporter: public iExportPlugin
{
 public:

    cExporter();
    ~cExporter();

    //! Return the long name (the description) of the interface.
    virtual const char* Description() 
	{ 
            // TODO: Use your own plugin description.
#		ifdef D_DEBUG
            return " Bbm Level Export Plugin (Debug)"; 
#		else
            return " Bbm Level Export Plugin"; 
#		endif
	}

    //! Return the id identifying the type of the interface.
    virtual t_type_id Type();

    virtual const char* PluginName() 
	{ 
            // TODO: Use your own plugin name.
#		ifdef D_DEBUG
            return " Bbm Level Export Plugin (Debug)"; 
#		else
            return " Bbm Level Export Plugin"; 
#		endif
	}

    //! Return the author of the plug-in.
    virtual const char* Vendor() { return "Quanticode"; }
    //! Return any copyright text.
    virtual const char* CopyrightNotice() { return "Copyright 2007 Quanticode"; }

    //! Called by the engine when the module from which the plug-in is created is unloaded.
    virtual void OnUnloadModule();

    //! Load the plug-in in the system (init its data and make it active).
    virtual t_error Activate(const t_bool activate);
    //! Indicate if the plug-in is active or not.
    virtual t_bool IsActive();

    //! Return the extensions no this plug-in support.
    virtual uint GetFileExtNo() 
	{ 
            // TODO: Modify with the no. of extensions your plugin supports.
            return 1; 
	}
    //! Return the i-th file extension this plug-in support (only the extension without the point ".").
    virtual const t_char* GetFileExt(const uint idx) 
	{ 
            // TODO: Modify with your export extension.
            return M_SZ("lvl"); 
	}

    //! Trigger the file export.
    virtual t_error Export(const t_char *file_path);

 protected:

    void exportTerrain(iTerrainZone * zone, const std::wstring & path, bbm::LevelData * lvl_data);

    void exportObjects(iObjectContainer * container, const Vector & offset, bbm::LevelData * lvl_data);

    void getGrassZoneInfo(iPropertyTable * table, const char * layer_name, bbm::DetailTexInfo & info);

    void copyProps(iPropertyTableOwner * owner, const std::string & table_name, LocalParameters & params);
    iPropertyTable * getTable(iPropertyTableOwner * owner, const std::string & name);

    iImage * makeScaledImage(iImage * src, unsigned width, unsigned height);


    template<typename T>
    void getPropertyValue(iPropertyTable * table, unsigned cat_index, const t_char* name, T & dest)
    {
        unsigned index = table->GetPropIndex(cat_index, name);
        if (index == UINT_MAX) throw Exception(std::string("Couldn't find property ") + fromUnicode(name));

        t_error ret = table->GetPropValue(cat_index, index, &dest);
        assert(M_SUCCEED(ret));
    }

};



//====================================================

//! Plug-in export function.
extern "C" D_EXPORT void* GetPluginInterface(void *sdk_root);

//! Plug-in instance.
extern cExporter g_export_plugin;

//! Pointer to the SDK root interface (received in \c GetPluginInterface).
extern iRootInterface *g_sdk_root;

//! Pointer to the storage manager used for various file related operations.
extern iStorageManager *g_storage_man;

//! [Windows specific] Handle to the module (need this for the plug-in dialog)
extern HINSTANCE g_hInstance;

//====================================================
#endif
/*@}*/
