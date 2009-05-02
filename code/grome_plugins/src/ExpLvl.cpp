
#include "ExpLvl.h"


#include <limits>
#include <fstream>

#include <stdio.h>
#include <float.h>


#include "toolbox/Serializer.h"
#include "toolbox/Utils.h"
#include "toolbox/TextValue.h"
#include "bbmloader/LevelData.h"


#undef max
#undef min

const std::string LIGHTMAP_TEX_NAME = "shadow.tga";
const unsigned LM_TEXELS_PER_QUAD = 2;
const unsigned LM_BLUR_ITERATIIONS = 2;

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

    return &g_export_plugin; 
}

cExporter g_export_plugin;

iRootInterface *g_sdk_root = NULL;
iStorageManager *g_storage_man = NULL;

HINSTANCE g_hInstance;

//====================================================

//------------------------------------------------------------------------------
cExporter::cExporter()
{
    // TODO: Init plugin parameters and data here.
    // ...
}

//------------------------------------------------------------------------------
cExporter::~cExporter()
{
    // TODO: Add release code here.
    // ...
}

//------------------------------------------------------------------------------
t_type_id cExporter::Type()
{
    // Return the type id from the SDK root.
    if(g_sdk_root)
    {
        return g_sdk_root->GetTypeId("Plugin");
    }
    return NULL;
}

//------------------------------------------------------------------------------
void cExporter::OnUnloadModule()
{
    // TODO: Add plugin unload code here.
    // ...
}

//------------------------------------------------------------------------------
t_error cExporter::Activate(const t_bool activate)
{
    // TODO: Add plugin activation code here.
    // ...
    return C_GENERIC_SUCCESS;
}

//------------------------------------------------------------------------------
t_bool cExporter::IsActive()
{
    // TODO: return activation status of the plugin here.
    // ...
    return C_FALSE;
}

//------------------------------------------------------------------------------
t_error cExporter::Export(const t_char *file_path)
{
    try
    {
        assert(g_sdk_root);

        // Obtain the interface to the editor framework as it is registered with the SDK root interface.
        iEditor *editor = (iEditor*)g_sdk_root->GetInterface(C_EDITOR_INTERFACE_NAME);
        AUTO_CLOSE(editor);

        // Obtain the interface to the current scene workspace.
        iEdWorkspace *workspace = editor->GetCurrWorkspace();
        AUTO_CLOSE(workspace);

        // Go through all the projects in the current workspace and export their content.
        t_readonly_array<iEdProject*> *projects = workspace->GetActiveProjects();
        AUTO_CLOSE_ARRAY(projects);

        std::wstring path = std::wstring(g_storage_man->ExtractFileDir(file_path));
        path += M_SZ("\\");

        bool found_terrain = false, found_objects = false;
        for(uint i=0; i<projects->No(); i++)
        {
            // Check the project type.
            t_type_id type_id = projects->Elem(i)->GetProjectType();
            if (type_id == g_sdk_root->GetTypeId(iTerrainEdProject::TypeString()))
            {
                assert(!found_terrain && "Two active terrain projects!?!");
                found_terrain = true;

                iTerrainEdProject *ed_project = (iTerrainEdProject*)projects->Elem(i)->OpenSubinterface(type_id);
                AUTO_CLOSE(ed_project);

                t_readonly_array<iTerrainZone*> * zones = ed_project->GetTerrainZones();
                AUTO_CLOSE_ARRAY(zones);
				
                iTerrainZone * exported_zone = NULL;
                if (zones->No() == 1) 
                {
                    exported_zone = zones->Elem(0);
                } else
                {
                    for (unsigned i=0; i<zones->No(); ++i) 
                    {
                        if (zones->Elem(i)->IsSelected())
                        {
                            assert(!exported_zone && "More than one terrain zone selected");
                            exported_zone = zones->Elem(i);
                        }
                    }
                }

                if (!exported_zone) 
                {
                    msgBox("No terrain zone found for export. Select the desired zone.");
                } else
                {
                    bbm::LevelData lvl_data;

                    exportTerrain(exported_zone, path, &lvl_data);


                    // Object positions are in world coordinates, we
                    // need the offset to the terrain zone
                    // corner. Calculate neccessary offset.
                    Vector offset = *((Vector*)&exported_zone->GetTransformMatrix()->m30);
                    offset.x_ -= exported_zone->GetXTileSize()*0.5*exported_zone->GetXTilesNo();
                    offset.z_ += exported_zone->GetZTileSize()*0.5*exported_zone->GetZTilesNo();

                    exportObjects(exported_zone->GetObjectContainer(), offset, &lvl_data);
					
                    lvl_data.save(fromUnicode(path));
                }
            } else if (type_id == g_sdk_root->GetTypeId(iObjectEdProject::TypeString()))
            {
                /*
                  assert(!found_objects && "Two active object projects!?!");
                  found_objects = true;

                  iObjectEdProject *ed_project = (iObjectEdProject*)projects->Elem(i)->OpenSubinterface(type_id);
                  AUTO_CLOSE(ed_project);

                  // Just warn for unexported objects
                  for (unsigned c=0; c<ed_project->GetObjectContainers()->No(); ++c)
                  {
                  t_readonly_array<iObjectInstance*> * objects = ed_project->GetObjectContainers()->Elem(c)->GetInstances();
                  for (unsigned o=0; o<objects->No(); ++o)
                  {
                  if (objects->Elem(o)->GetObjectContainer()->GetLinkOwner() == NULL)
                  {
                  msgBox(std::string(objects->Elem(0)->Name()) + " was not exported because it is not linked to terrain zone.");
                  }
                  }
                  }	
                */
            }
        }

        msgBox("Success!", "Export complete");
        return C_GENERIC_SUCCESS;

    } catch (AssertException & e)
    {
        return C_GENERIC_ERROR;
    } catch (Exception & e)
    {
        msgBox(e.getTotalErrorString(), "An error occured");
    }
}


//------------------------------------------------------------------------------
void cExporter::exportTerrain(iTerrainZone * zone, const std::wstring & path, bbm::LevelData * lvl_data)
{
    const float SCALING_FACTOR = 1.0f;

    t_error ret;	
    zone->UnswapData();


    // First, copy our custom proeprties into the level parameter manager.
    iPropertyTableOwner * prop_owner = (iPropertyTableOwner*)zone->OpenSubinterface(
        g_sdk_root->GetTypeId(iPropertyTableOwner::TypeString()));
    AUTO_CLOSE(prop_owner);
        
    copyProps(prop_owner, "LevelInfo", lvl_data->getParams());


    uint32_t width  = zone->GetXTilesNo()+1;
    uint32_t height = zone->GetZTilesNo()+1;
    assert((width == height) && "Terrain zone must be square");

    float32_t max_height = std::numeric_limits<float>::min();
    float32_t min_height = std::numeric_limits<float>::max();
    for (unsigned b=0; b<8; ++b)
    {
        max_height = std::max(zone->GetBBox()[b].y, max_height)*SCALING_FACTOR;
        min_height = std::min(zone->GetBBox()[b].y, min_height)*SCALING_FACTOR;
    }

    float32_t horz_scale = zone->GetXTileSize()*SCALING_FACTOR;
    assert((zone->GetZTileSize()*SCALING_FACTOR == horz_scale) && "Scale must be the same in X/Z direction");


    // Export Heightmap
    serializer::Serializer s(fromUnicode(path) + "terrain.hm", serializer::SOM_COMPRESS | serializer::SOM_WRITE);
    s.put(width);
    s.put(height);
    s.put(max_height);
    s.put(min_height);
    s.put(horz_scale);

    const float32_t * hm = zone->GetHeightmap();

    for (unsigned r=0; r<height; ++r)
    {
        for (unsigned c=0; c<width; ++c)
        {
            float32_t e = hm[c + (height-r-1)*width]*SCALING_FACTOR;
            s.put(e);
        }
    }


    // -------------------- Lightmap, color & texture layers --------------------
    t_readonly_array<iEditorLayer*> * layers = zone->GetAssignedLayers(iTerrainMaterialLayer::TypeString());   
    assert(layers); 
    
    // First, traverse layers, check assertions and collect information
    std::vector<iTerrainMaterialLayer*> color_layer;
    std::vector<iTerrainMaterialLayer*> tex_layer;
    iTerrainMaterialLayer* lightmap_layer = NULL;

    // Traverse topmost to bottommost for easier processing afterwards
    for (int l=layers->No()-1; l>=0; --l)
    {
        iTerrainMaterialLayer * mat_layer = (iTerrainMaterialLayer*)layers->Elem(l);

        // layer must have mask
        assert(mat_layer->GetTextureChannel(zone, C_TEX_CHANNEL_MASK));
        assert(mat_layer->GetTextureChannel(zone, C_TEX_CHANNEL_MASK)->source);

        iImage * mask_image = mat_layer->GetTextureChannel(zone, C_TEX_CHANNEL_MASK)->source->GetImage();
        
        if (mat_layer->GetMaterial(zone)->channels_no == 1)
        {
            // This should be a color layer
            assert(mask_image->GetPixelFormat() == C_IMG_PIXEL_RGBA);
            assert(mask_image->GetDataType()    == C_IMG_DATA_UNSIGNED_BYTE);
            assert(mask_image->GetBPP()         == 4);
            
            color_layer.push_back(mat_layer);
        } else
        {
            // tex or lightmap layer
            assert(mat_layer->GetMaterial(zone)->channels_no == 2);

            assert(mat_layer->GetTextureChannel(zone, C_TEX_CHANNEL_TEXTURE));
            assert(mat_layer->GetTextureChannel(zone, C_TEX_CHANNEL_TEXTURE)->source);
            
            assert(mask_image->GetPixelFormat() == C_IMG_PIXEL_LUMINANCE);
            assert(mask_image->GetDataType()    == C_IMG_DATA_UNSIGNED_BYTE);
            assert(mask_image->GetBPP()         == 1);
            
            if (mat_layer->GetTextureChannel(zone, C_TEX_CHANNEL_TEXTURE)->source->GetImage()->Name() == LIGHTMAP_TEX_NAME)
            {
                assert(lightmap_layer == NULL && "Two lightmaps, cannot decide which one to pick");
                lightmap_layer = mat_layer;
            } else  if (mat_layer->IsEnabled())
            {
                tex_layer.push_back(mat_layer);
            }
        }
    }



    
    // Export Lightmap
    // We need to invert the color, make new image...
    iImage * exported_lightmap = (iImage*)g_sdk_root->NewNode(C_NODE_FACTORY_IMAGE, "lm_ex", NULL, NULL);
    AUTO_DEL_NODE(exported_lightmap);
    ret = exported_lightmap->MakeBlank(C_IMG_TYPE_PNG, width*LM_TEXELS_PER_QUAD, height*LM_TEXELS_PER_QUAD,
                                       4, C_IMG_PIXEL_RGBA, C_IMG_DATA_UNSIGNED_BYTE , NULL);
    assert(M_SUCCEED(ret));
    std::vector<uint32_t> lm_pixels(width*height*LM_TEXELS_PER_QUAD*LM_TEXELS_PER_QUAD, 0xffffffff);


    if (lightmap_layer)
    {
        // Now that we know the lightmap layer, simply save it as an image.
        iImage * lightmap_mask_image = makeScaledImage(
            lightmap_layer->GetTextureChannel(zone, C_TEX_CHANNEL_MASK)->source->GetImage(),
            width*LM_TEXELS_PER_QUAD,
            height*LM_TEXELS_PER_QUAD);
        AUTO_DEL_NODE(lightmap_mask_image);

		// Lightmap image is only 8 bpp!
        ret = lightmap_mask_image->GetPixels(0, 0, &lm_pixels[0], width*LM_TEXELS_PER_QUAD, height*LM_TEXELS_PER_QUAD);
        assert(M_SUCCEED(ret));		

		averageArray((uint8_t*)&lm_pixels[0], width*LM_TEXELS_PER_QUAD, height*LM_TEXELS_PER_QUAD, LM_BLUR_ITERATIIONS);

		// convert 8 bpp to 32 bpp -> go from back to front to avoid overwriting information
        for (int i=width*height*LM_TEXELS_PER_QUAD*LM_TEXELS_PER_QUAD-1; i >= 0; --i)
        {
			uint8_t val = ((uint8_t*)&lm_pixels[0])[i];
            lm_pixels[i] = 255 - val;
            lm_pixels[i] |= lm_pixels[i] << 8;
            lm_pixels[i] |= lm_pixels[i] << 16;
            lm_pixels[i] |= 0xff000000;
        }
    }

/*
    // Traverse color layers and modulate lightmap
    std::vector<uint32_t> color_pixels(width*LM_TEXELS_PER_QUAD*height*LM_TEXELS_PER_QUAD);
    for (unsigned l=0; l<color_layer.size(); ++l)
    {        
        iImage * color_mask_image = makeScaledImage(
            color_layer[l]->GetTextureChannel(zone, C_TEX_CHANNEL_MASK)->source->GetImage(),
            width*LM_TEXELS_PER_QUAD,
            height*LM_TEXELS_PER_QUAD);
        AUTO_DEL_NODE(color_mask_image);

        ret = color_mask_image->GetPixels(0, 0, &color_pixels[0], width*LM_TEXELS_PER_QUAD, height*LM_TEXELS_PER_QUAD);
        assert(M_SUCCEED(ret));		

        for (unsigned i=0; i<width*height*LM_TEXELS_PER_QUAD*LM_TEXELS_PER_QUAD; ++i)
        {
            uint8_t * cur_lm    = (uint8_t*)&lm_pixels[i];
            uint8_t * cur_color = (uint8_t*)&color_pixels[i];

            for (unsigned c=0; c<4; ++c)
            {
                *cur_lm = (uint8_t)((float)*cur_color / 255.0f * *cur_lm);
            }
        }
    }
*/
    
    ret = exported_lightmap->PutPixels(0, 0, &lm_pixels[0], width*LM_TEXELS_PER_QUAD, height*LM_TEXELS_PER_QUAD);
    assert(M_SUCCEED(ret));

    ret = exported_lightmap->CopyTo((path + M_SZ("lm_color.png")).c_str());	
    assert(M_SUCCEED(ret) && "Could not copy lm_color to destination path.");


    // Export Detail Map

    // Now compose our detail map. Traverse tex layers, add their
    // values to the respective channel without exceeding 1. Start
    // with the topmost layer.
    assert(tex_layer.size() < 7);


    
    
    // We need the texture's names to store in lvl_data afterwards...
    std::vector<std::wstring> img_name(tex_layer.size());
    std::vector<uint32_t> detail_map(width*height, 0);
    for (int l=0; l<tex_layer.size(); ++l)
    {
        // First, copy the image to our export path
        iImage * detail_image = tex_layer[l]->GetTextureChannel(zone, C_TEX_CHANNEL_TEXTURE)->source->GetImage();
//        AUTO_DEL_NODE(detail_image); // XXXX perhaps remove this?
        img_name[l] = detail_image->GetPath();

        // strip path from image name
        img_name[l] = img_name[l].substr(img_name[l].rfind(M_SZ("\\"))+1, std::wstring::npos);
        ret = detail_image->CopyTo((path + M_SZ("\\") + img_name[l]).c_str());       
        assert(M_SUCCEED(ret));
  


        // Now compose the detail map
        sTextureMapping * mapping = tex_layer[l]->GetTextureChannel(zone, C_TEX_CHANNEL_TEXTURE)->mapping;

        float s = 1.0f / (horz_scale * (width-1));

        // Determine whether HV rotation is set
        Matrix tex_mat((float*)mapping->tex_matrix.m);
        if (mapping->rotation.x == 0.0f && mapping->rotation.y == 0.0f)
        {
            Matrix scale(true);
            scale.scale(s, s, 1.0f);
            tex_mat = tex_mat * scale;
        } else
        {
            Matrix rot_h(true), rot_v(true);
            rot_v.loadCanonicalRotation(mapping->rotation.y, 0);
            rot_h.loadCanonicalRotation(mapping->rotation.x, 2);

            Matrix scale(true);
            scale.scale(1.0f, 1.0f, mapping->scale.y);

            tex_mat = scale * tex_mat * rot_v *rot_h;
        }

        Matrix coord_sys_correction(true);
        coord_sys_correction.getY() = Vector(0,0,1);
        coord_sys_correction.getZ() = Vector(0,-1,0);
        coord_sys_correction.getTranslation().y_ = horz_scale * (width-1);

        tex_mat = tex_mat * coord_sys_correction;



        bbm::DetailTexInfo info(fromUnicode(img_name[l]), tex_mat,0);
        // extract grass zone information from properties
        getGrassZoneInfo(getTable(prop_owner, "ZoneInfo"), tex_layer[l]->Name(), info);
        lvl_data->setDetailTexInfo(l, info);



        // every detail tex coefficient gets 4 bits.
        // detail texture coefficients are encoded as follows to allow for easy shader unpacking:
        // A | B | G | R
        // 37  26  15  04
        unsigned cur_shift = 2*4*(l%4) + 4*(1-l/4);
        iImage * mask_image = makeScaledImage(
            tex_layer[l]->GetTextureChannel(zone, C_TEX_CHANNEL_MASK)->source->GetImage(), width, height);
        AUTO_DEL_NODE(mask_image);				

        uint8_t * buf = (uint8_t*)mask_image->GetBuffer(0,0);
        for (unsigned p=0; p<width*height; ++p)
        {		
            uint32_t cur_sum = 0;

			for (unsigned i=0; i<8; ++i)
			{				
				cur_sum += (detail_map[p] >> (4*i)) & 0xf;
			}
            assert(cur_sum < 16);

			unsigned missing = 15-cur_sum;

			if (l == tex_layer.size()-1)
			{
				detail_map[p] |= missing << cur_shift;	
			} else
			{
				detail_map[p] |= std::min((uint32_t)(buf[p]>>4), missing) << cur_shift;			
			}
        }
    }

    iImage * exported_detailmap = (iImage*)g_sdk_root->NewNode(C_NODE_FACTORY_IMAGE, "det_ex", NULL, NULL);
    AUTO_DEL_NODE(exported_detailmap);

    ret = exported_detailmap->MakeBlank(C_IMG_TYPE_PNG, width, height, 4, C_IMG_PIXEL_RGBA, C_IMG_DATA_UNSIGNED_BYTE, NULL);
    assert(M_SUCCEED(ret));

    ret = exported_detailmap->PutPixels(0, 0, &detail_map[0], width, height);
    assert(M_SUCCEED(ret));

    exported_detailmap->CopyTo((path + M_SZ("detail.png")).c_str());
}


//------------------------------------------------------------------------------
void cExporter::exportObjects(iObjectContainer * container, const Vector & offset, bbm::LevelData * lvl_data)
{	
    if (!container) return;

    for (unsigned obj=0; obj<container->GetInstances()->No(); ++obj)
    {
        iObjectInstance * inst = container->GetInstances()->Elem(obj);

        // strip .bbm and any stuff behind it (number)
        std::string name = inst->Name();
        std::string::size_type s = name.find(".bbm");
        if (s == std::string::npos) continue;
        name = name.substr(0, s);


        // Correct handedness & offset of coordinate system
        t_float4x4 t;
        inst->GetTransformMatrix(t);
        Matrix transform((float*)t.m);
        transform.getTranslation() -= offset;
        Matrix m1(true);
        m1.getZ() *= -1;
        transform = m1*transform*m1;


        // Handle special elements (light dir, camera pos)
        bbm::ObjectInfo info;
        info.name_      = name;
        info.transform_ = transform;

        // Custom properties
        iPropertyTableOwner * prop_owner = (iPropertyTableOwner*)inst->OpenSubinterface(
            g_sdk_root->GetTypeId(iPropertyTableOwner::TypeString()));
        AUTO_CLOSE(prop_owner);
        for (unsigned t=0; t<prop_owner->GetPropertyTablesNo(); ++t)
        {                
            copyProps(prop_owner, fromUnicode(prop_owner->GetPropertyTable(t)->GetStructureName()), info.params_);
        }

        lvl_data->addObjectInfo(info);
    }
}





//------------------------------------------------------------------------------
void cExporter::getGrassZoneInfo(iPropertyTable * table, const char * layer_name, bbm::DetailTexInfo & info)
{
    for (unsigned count = 0; /* no condition*/ ; ++count)
    {
        unsigned cat_index = table->GetCategIndex(toUnicode("type" + toString(count+1)).c_str());
        if (cat_index == UINT_MAX) break; 

        t_char * layer_w   = NULL;
        t_char * models_w  = NULL;
        t_char * probs_w   = NULL;
        t_char * density_w = NULL;

        getPropertyValue(table, cat_index, M_SZ("layer"),   layer_w);
        getPropertyValue(table, cat_index, M_SZ("models"),  models_w);
        getPropertyValue(table, cat_index, M_SZ("probs"),   probs_w);
        getPropertyValue(table, cat_index, M_SZ("density"), density_w);

        std::string layer = layer_w ? fromUnicode(layer_w) : "";

        if (layer != layer_name) continue;

        std::vector<std::string> models;
        std::istringstream istr1(models_w ? fromUnicode(models_w) : "");
        istr1 >> models;

        std::vector<float> probs;
        std::istringstream istr2(probs_w ? fromUnicode(probs_w) : "");
        istr2 >> probs;

        float density = density_w ? fromString<float>(fromUnicode(density_w)) : 0;


        // Copy what we've found into the description structure...
        info.grass_density_ = density;
        assert(models.size() == probs.size());
        for (unsigned i=0; i<models.size(); ++i)
        {
            info.zone_info_.push_back(bbm::GrassZoneInfo(models[i], probs[i]));
        }
    }
}



//------------------------------------------------------------------------------
/**
 *  Copies all properties from the specified proptable into the
 *  parameter manager object.
 */
void cExporter::copyProps(iPropertyTableOwner * owner,
                          const std::string & table_name,
                          LocalParameters & params)
{
    iPropertyTable * table = getTable(owner, table_name);
    if (!table) return;


    for (unsigned category=0; category<table->GetCategsNo(); ++category)
    {
        std::string categ_name = fromUnicode(table->GetCategName(category));
        
        for (unsigned prop=0; prop<table->GetPropNo(category); ++prop)
        {
            sPropertyStructure structure;
            table->GetPropStructure(category, prop, structure);     

            if (structure.type == C_PROP_ARRAY)
            {
                sArrayPropertyData arr_data;
                getPropertyValue(table, category, structure.name, arr_data);

                if (arr_data.current_sel == -1) assert(!"No element selected in array");

                switch (arr_data.elem_type)
                {
                case C_PROP_STRING:
                    params.set<std::string>(categ_name + "." + fromUnicode(structure.name),
                                            fromUnicode(((t_char**)arr_data.elems)[arr_data.current_sel]));
                    break;
                default:
                    assert(!"Array type not implemented");
                }
            } else
            {               
                Vector2d v2;
                Vector v;
                switch(structure.type)
                {
                case C_PROP_BOOL:
                    bool b;
                    getPropertyValue(table, category, structure.name, b);
                    params.set<bool>(categ_name + "." + fromUnicode(structure.name), b);
                    break;
                case C_PROP_STRING:
                    t_char * str; str = NULL;
                    getPropertyValue(table, category, structure.name, str);
                    assert(str);
                    params.set<std::string>(categ_name + "." + fromUnicode(structure.name), fromUnicode(str));
                    break;
                case C_PROP_INT:
                    int i;
                    getPropertyValue(table, category, structure.name, i);
                    params.set<int>(categ_name + "." + fromUnicode(structure.name), i);
                    break;
                case C_PROP_UINT:
                    unsigned ui;
                    getPropertyValue(table, category, structure.name, ui);
                    params.set<unsigned>(categ_name + "." + fromUnicode(structure.name), ui);
                    break;
                case C_PROP_FLOAT:
                    float f;
                    getPropertyValue(table, category, structure.name, f);
                    params.set<float>(categ_name + "." + fromUnicode(structure.name), f);
                    break;
                case C_PROP_FLOAT2:
                    getPropertyValue(table, category, structure.name, v2);
                    params.set<Vector2d>(categ_name + "." + fromUnicode(structure.name), v2);
                    break;
                case C_PROP_FLOAT3:
                    getPropertyValue(table, category, structure.name, v);
                    params.set<Vector>(categ_name + "." + fromUnicode(structure.name), v);
                    break;
                default:
                    assert(!"Unknown property type");
                }
            }
        }
    }
}


//------------------------------------------------------------------------------
iPropertyTable * cExporter::getTable(iPropertyTableOwner * owner, const std::string & name)
{
    for (unsigned i=0; i < owner->GetPropertyTablesNo(); ++i)
    {
        iPropertyTable * cur_table = owner->GetPropertyTable(i); 
        if (fromUnicode(cur_table->GetStructureName()) == name) return cur_table;
    }

    return NULL;
}


//------------------------------------------------------------------------------
iImage * cExporter::makeScaledImage(iImage * src, unsigned width, unsigned height)
{
    iImage * img = (iImage*)g_sdk_root->NewNode(
        C_NODE_FACTORY_IMAGE, (toString(src) + toString(rand())).c_str(), NULL, NULL);
    assert(img);

    t_error ret;

    ret = img->MakeBlank(
        C_IMG_TYPE_PNG, src->Width(), src->Height(), src->GetBPP(), src->GetPixelFormat(), src->GetDataType(), NULL);
    assert(M_SUCCEED(ret));

    ret = img->PutPixels(0, 0, src->GetBuffer(0,0), src->Width(), src->Height());
    assert(M_SUCCEED(ret));

    ret = img->Scale(width, height);
    assert(M_SUCCEED(ret));

    return img;
}
