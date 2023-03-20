#include <std_include.hpp>
#include "zonetool.hpp"

#include <utils/io.hpp>

#include "zonetool/h1/zonetool.hpp"

#include "converter/converter.hpp"

namespace zonetool::iw6
{
#ifdef DEBUG
	constexpr auto IS_DEBUG = true;
#else
	constexpr auto IS_DEBUG = false;
#endif

	enum dump_mode : std::int32_t
	{
		iw6,
		h1,
	} dumping_mode{ iw6 };

	void dump_asset(XAsset* asset);
	void stop_dumping();

	bool verify = false;
	bool dump = false;

	filesystem::file csv_file;

	// referenced assets
	std::vector<std::pair<XAssetType, std::string>> referenced_assets;

	std::unordered_map<std::uint32_t, XGfxGlobals*> xGfxGlobals_map;

	const char* get_asset_name(XAssetType type, void* pointer)
	{
		XAssetHeader header{ .data = pointer };
		XAsset xasset = { XAssetType(type), header };
		return DB_GetXAssetName(&xasset);
	}

	const char* get_asset_name(XAsset* asset)
	{
		return DB_GetXAssetName(asset);
	}

	void set_asset_name(XAsset* asset, const char* name)
	{
		DB_SetXAssetName(asset, name);
	}

	const char* type_to_string(XAssetType type)
	{
		return g_assetNames[type];
	}

	std::int32_t type_to_int(std::string type)
	{
		for (std::int32_t i = 0; i < ASSET_TYPE_COUNT; i++)
		{
			if (g_assetNames[i] == type)
				return i;
		}

		return -1;
	}

	bool is_valid_asset_type(const std::string& type)
	{
		return type_to_int(type) >= 0;
	}

	bool zone_exists(const std::string& zone)
	{
		return DB_FileExists(zone.data(), 0);
	}

	bool is_referenced_asset(XAsset* asset)
	{
		if (get_asset_name(asset)[0] == ',')
		{
			return true;
		}
		return false;
	}

	void wait_for_database()
	{
		// wait for database to be ready
		while (!WaitForSingleObject(*reinterpret_cast<HANDLE*>(0x1446B4B48), 0) == 0)
		{
			Sleep(5);
		}
	}

	XAssetHeader DB_FindXAssetHeader_Safe(XAssetType type, const std::string& name)
	{
		const auto asset_entry = DB_FindXAssetEntry(type, name.data());

		if (asset_entry)
		{
			return asset_entry->asset.header;
		}
		return DB_FindXAssetHeader(type, name.data(), 1);
	}

	void DB_EnumXAssets(const XAssetType type,
		const std::function<void(XAssetHeader)>& callback, const bool includeOverride)
	{
		DB_EnumXAssets_Internal(type, static_cast<void(*)(XAssetHeader, void*)>([](XAssetHeader header, void* data)
		{
			const auto& cb = *static_cast<const std::function<void(XAssetHeader)>*>(data);
			cb(header);
		}), &callback, includeOverride);
	}

	void dump_asset_internal_h1(XAsset* asset)
	{
#define DUMP_ASSET_REGULAR(__type__,__interface__,__struct__) \
				if (asset->type == __type__) \
				{ \
					if(IS_DEBUG) ZONETOOL_INFO("Dumping asset \"%s\" of type %s.", get_asset_name(asset), type_to_string(asset->type)); \
					auto asset_ptr = reinterpret_cast<__struct__*>(asset->header.data); \
					__interface__::dump(asset_ptr); \
				}

#define DUMP_ASSET_NO_CONVERT(__type__,__interface__,__struct__) \
				if (asset->type == __type__) \
				{ \
					if(IS_DEBUG) ZONETOOL_INFO("Dumping asset \"%s\" of type %s.", get_asset_name(asset), type_to_string(asset->type)); \
					auto asset_ptr = reinterpret_cast<zonetool::h1::__struct__*>(asset->header.data); \
					zonetool::h1::__interface__::dump(asset_ptr); \
				}

#define DUMP_ASSET_CONVERT(__type__,__namespace__,__struct__) \
				if (asset->type == __type__) \
				{ \
					if(IS_DEBUG) ZONETOOL_INFO("Converting and dumping asset \"%s\" of type %s.", get_asset_name(asset), type_to_string(asset->type)); \
					auto asset_ptr = reinterpret_cast<__struct__*>(asset->header.data); \
					converter::h1::__namespace__::dump(asset_ptr, memory.get()); \
				}
		try
		{
			static std::shared_ptr<ZoneMemory> memory;
			if (!memory)
			{
				// todo: free memory after dump
				memory = std::make_shared<ZoneMemory>((1024u * 1024u * 1024u) * 2u); // 2gb
			}

			converter::h1::techset::converted_techset_assets.clear();

			// dump assets
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_DOPPLER_PRESET, IDopplerPreset, DopplerPreset);
			DUMP_ASSET_CONVERT(ASSET_TYPE_FX, fxeffectdef, FxEffectDef);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_PARTICLE_SIM_ANIMATION, IFxParticleSimAnimation, FxParticleSimAnimation);
			DUMP_ASSET_CONVERT(ASSET_TYPE_IMAGE, gfximage, GfxImage);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_LIGHT_DEF, IGfxLightDef, GfxLightDef);
			DUMP_ASSET_REGULAR(ASSET_TYPE_LOADED_SOUND, ILoadedSound, LoadedSound);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_LOCALIZE_ENTRY, ILocalize, LocalizeEntry);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_LPF_CURVE, ILpfCurve, SndCurve);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_LUA_FILE, ILuaFile, LuaFile);
			DUMP_ASSET_CONVERT(ASSET_TYPE_MATERIAL, material, Material);
			DUMP_ASSET_CONVERT(ASSET_TYPE_MAP_ENTS, mapents, MapEnts);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_NET_CONST_STRINGS, INetConstStrings, NetConstStrings);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_RAWFILE, IRawFile, RawFile);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_REVERB_CURVE, IReverbCurve, SndCurve);
			DUMP_ASSET_CONVERT(ASSET_TYPE_SCRIPTABLE, scriptabledef, ScriptableDef);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_SCRIPTFILE, IScriptFile, ScriptFile);
			DUMP_ASSET_CONVERT(ASSET_TYPE_SOUND, sound, snd_alias_list_t);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_SOUND_CURVE, ISoundCurve, SndCurve);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_STRINGTABLE, IStringTable, StringTable);
			//DUMP_ASSET_NO_CONVERT(ASSET_TYPE_STRUCTURED_DATA_DEF, IStructuredDataDefSet, StructuredDataDefSet); // crashes
			DUMP_ASSET_CONVERT(ASSET_TYPE_TECHNIQUE_SET, techset, MaterialTechniqueSet);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_TRACER, ITracerDef, TracerDef);
			//DUMP_ASSET(ASSET_TYPE_FONT, IFontDef, Font_s);
			//DUMP_ASSET(ASSET_TYPE_ATTACHMENT, IWeaponAttachment, WeaponAttachment);
			//DUMP_ASSET(ASSET_TYPE_WEAPON, IWeaponDef, WeaponCompleteDef);
			DUMP_ASSET_CONVERT(ASSET_TYPE_XANIMPARTS, xanim, XAnimParts);
			DUMP_ASSET_CONVERT(ASSET_TYPE_XMODEL, xmodel, XModel);
			DUMP_ASSET_CONVERT(ASSET_TYPE_XMODEL_SURFS, xsurface, XModelSurfs);

			//DUMP_ASSET_CONVERT(ASSET_TYPE_PHYSCOLLMAP, physcollmap, PhysCollmap);
			//DUMP_ASSET_CONVERT(ASSET_TYPE_PHYSPRESET, physpreset, PhysPreset);

			DUMP_ASSET_CONVERT(ASSET_TYPE_COMPUTESHADER, computeshader, ComputeShader);
			DUMP_ASSET_CONVERT(ASSET_TYPE_DOMAINSHADER, domainshader, MaterialDomainShader);
			DUMP_ASSET_CONVERT(ASSET_TYPE_HULLSHADER, hullshader, MaterialHullShader);
			DUMP_ASSET_CONVERT(ASSET_TYPE_PIXELSHADER, pixelshader, MaterialPixelShader);
			//DUMP_ASSET_CONVERT(ASSET_TYPE_VERTEXDECL, vertexdecl, MaterialVertexDeclaration);
			DUMP_ASSET_CONVERT(ASSET_TYPE_VERTEXSHADER, vertexshader, MaterialVertexShader);

			//DUMP_ASSET_NO_CONVERT(ASSET_TYPE_MENU, IMenuDef, menuDef_t);
			//DUMP_ASSET_NO_CONVERT(ASSET_TYPE_MENULIST, IMenuList, MenuList);

			//DUMP_ASSET_CONVERT(ASSET_TYPE_PATHDATA, pathdata, PathData);
			DUMP_ASSET_CONVERT(ASSET_TYPE_CLIPMAP, clipmap, clipMap_t);
			DUMP_ASSET_CONVERT(ASSET_TYPE_COMWORLD, comworld, ComWorld);
			DUMP_ASSET_CONVERT(ASSET_TYPE_FXWORLD, fxworld, FxWorld);
			DUMP_ASSET_CONVERT(ASSET_TYPE_GFXWORLD, gfxworld, GfxWorld);
			DUMP_ASSET_NO_CONVERT(ASSET_TYPE_GLASSWORLD, IGlassWorld, GlassWorld);
		}
		catch (std::exception& ex)
		{
			ZONETOOL_FATAL("A fatal exception occured while dumping zone \"%s\", exception was: \n%s", filesystem::get_fastfile().data(), ex.what());
		}
	}

	void dump_asset_internal(XAsset* asset)
	{
#define DUMP_ASSET(__type__,__interface__,__struct__) \
				if (asset->type == __type__) \
				{ \
					if(IS_DEBUG) ZONETOOL_INFO("Dumping asset \"%s\" of type %s.", get_asset_name(asset), type_to_string(asset->type)); \
					auto asset_ptr = reinterpret_cast<__struct__*>(asset->header.data); \
					__interface__::dump(asset_ptr); \
				}
		try
		{
			// dump assets
			DUMP_ASSET(ASSET_TYPE_DOPPLER_PRESET, IDopplerPreset, DopplerPreset);
			DUMP_ASSET(ASSET_TYPE_FX, IFxEffectDef, FxEffectDef);
			DUMP_ASSET(ASSET_TYPE_PARTICLE_SIM_ANIMATION, IFxParticleSimAnimation, FxParticleSimAnimation);
			DUMP_ASSET(ASSET_TYPE_IMAGE, IGfxImage, GfxImage);
			DUMP_ASSET(ASSET_TYPE_LIGHT_DEF, IGfxLightDef, GfxLightDef);
			DUMP_ASSET(ASSET_TYPE_LOADED_SOUND, ILoadedSound, LoadedSound);
			DUMP_ASSET(ASSET_TYPE_LOCALIZE_ENTRY, ILocalize, LocalizeEntry);
			DUMP_ASSET(ASSET_TYPE_LPF_CURVE, ILpfCurve, SndCurve);
			DUMP_ASSET(ASSET_TYPE_LUA_FILE, ILuaFile, LuaFile);
			DUMP_ASSET(ASSET_TYPE_MATERIAL, IMaterial, Material);
			DUMP_ASSET(ASSET_TYPE_MAP_ENTS, IMapEnts, MapEnts);
			DUMP_ASSET(ASSET_TYPE_NET_CONST_STRINGS, INetConstStrings, NetConstStrings);
			DUMP_ASSET(ASSET_TYPE_RAWFILE, IRawFile, RawFile);
			DUMP_ASSET(ASSET_TYPE_REVERB_CURVE, IReverbCurve, SndCurve);
			DUMP_ASSET(ASSET_TYPE_SCRIPTABLE, IScriptableDef, ScriptableDef);
			DUMP_ASSET(ASSET_TYPE_SCRIPTFILE, IScriptFile, ScriptFile);
			DUMP_ASSET(ASSET_TYPE_SOUND, ISound, snd_alias_list_t);
			DUMP_ASSET(ASSET_TYPE_SOUND_CURVE, ISoundCurve, SndCurve);
			DUMP_ASSET(ASSET_TYPE_STRINGTABLE, IStringTable, StringTable);
			DUMP_ASSET(ASSET_TYPE_STRUCTURED_DATA_DEF, IStructuredDataDefSet, StructuredDataDefSet);
			DUMP_ASSET(ASSET_TYPE_TECHNIQUE_SET, ITechset, MaterialTechniqueSet);
			DUMP_ASSET(ASSET_TYPE_TRACER, ITracerDef, TracerDef);
			DUMP_ASSET(ASSET_TYPE_FONT, IFontDef, Font_s);
			DUMP_ASSET(ASSET_TYPE_ATTACHMENT, IWeaponAttachment, WeaponAttachment);
			DUMP_ASSET(ASSET_TYPE_WEAPON, IWeaponDef, WeaponCompleteDef);
			DUMP_ASSET(ASSET_TYPE_XANIMPARTS, IXAnimParts, XAnimParts);
			DUMP_ASSET(ASSET_TYPE_XMODEL, IXModel, XModel);
			DUMP_ASSET(ASSET_TYPE_XMODEL_SURFS, IXSurface, XModelSurfs);

			DUMP_ASSET(ASSET_TYPE_PHYSCOLLMAP, IPhysCollmap, PhysCollmap);
			DUMP_ASSET(ASSET_TYPE_PHYSPRESET, IPhysPreset, PhysPreset);

			DUMP_ASSET(ASSET_TYPE_COMPUTESHADER, IComputeShader, ComputeShader);
			DUMP_ASSET(ASSET_TYPE_DOMAINSHADER, IDomainShader, MaterialDomainShader);
			DUMP_ASSET(ASSET_TYPE_HULLSHADER, IHullShader, MaterialHullShader);
			DUMP_ASSET(ASSET_TYPE_PIXELSHADER, IPixelShader, MaterialPixelShader);
			//DUMP_ASSET(ASSET_TYPE_VERTEXDECL, IVertexDecl, MaterialVertexDeclaration);
			DUMP_ASSET(ASSET_TYPE_VERTEXSHADER, IVertexShader, MaterialVertexShader);

			//DUMP_ASSET(ASSET_TYPE_MENU, IMenuDef, menuDef_t);
			//DUMP_ASSET(ASSET_TYPE_MENULIST, IMenuList, MenuList);

			//DUMP_ASSET(ASSET_TYPE_PATHDATA, IAIPaths, PathData);
			DUMP_ASSET(ASSET_TYPE_CLIPMAP, IClipMap, clipMap_t);
			DUMP_ASSET(ASSET_TYPE_COMWORLD, IComWorld, ComWorld);
			DUMP_ASSET(ASSET_TYPE_FXWORLD, IFxWorld, FxWorld);
			DUMP_ASSET(ASSET_TYPE_GFXWORLD, IGfxWorld, GfxWorld);
			DUMP_ASSET(ASSET_TYPE_GLASSWORLD, IGlassWorld, GlassWorld);
		}
		catch (std::exception& ex)
		{
			ZONETOOL_FATAL("A fatal exception occured while dumping zone \"%s\", exception was: \n%s", filesystem::get_fastfile().data(), ex.what());
		}
	}

	void dump_asset(XAsset* asset)
	{
		if (verify)
		{
			ZONETOOL_INFO("Loading asset \"%s\" of type %s.", get_asset_name(asset), type_to_string(asset->type));
		}

		// dump all
		//dump = true;
		//std::string fastfile = static_cast<std::string>(
		//	reinterpret_cast<const char*>(*reinterpret_cast<std::uintptr_t*>(0x141EEFBA0) + 24));
		//filesystem::set_fastfile(fastfile);

		if (dump)
		{
			// open csv file for dumping 
			if (csv_file.get_fp() == nullptr)
			{
				csv_file = filesystem::file(filesystem::get_fastfile() + ".csv");
				csv_file.open("wb");
			}

			// dump assets to disk
			if (csv_file.get_fp()/* && !is_referenced_asset(xasset)*/)
			{
				fprintf(csv_file.get_fp(), "%s,%s\n", type_to_string(asset->type), get_asset_name(asset));
			}

			// dump referenced later
			if (is_referenced_asset(asset))
			{
				//referenced_assets.push_back({ asset->type, get_asset_name(asset) });
			}
			else
			{
				switch (dumping_mode)
				{
				case dump_mode::iw6:
					dump_asset_internal(asset);
					break;
				case dump_mode::h1:
					dump_asset_internal_h1(asset);
					break;
				}
			}
		}
	}

	void stop_dumping()
	{
		static bool stopping = false;
		if (stopping)
		{
			return;
		}
		stopping = true;

		if (dump)
		{
			// remove duplicates
			sort(referenced_assets.begin(), referenced_assets.end());
			referenced_assets.erase(unique(referenced_assets.begin(), referenced_assets.end()), referenced_assets.end());

			for (auto& asset : referenced_assets)
			{
				if (asset.second.length() <= 1)
				{
					continue;
				}

				const auto asset_name = &asset.second[1];

				if (asset.first == ASSET_TYPE_IMAGE)
				{
					ZONETOOL_WARNING("Not dumping referenced asset \"%s\" of type \"%s\"", asset_name, type_to_string(asset.first));
					continue;
				}

				const auto& asset_header = DB_FindXAssetHeader_Safe(asset.first, asset_name);

				if (!asset_header.data || DB_IsXAssetDefault(asset.first, asset_name))
				{
					ZONETOOL_ERROR("Could not find referenced asset \"%s\" of type \"%s\"", asset_name, type_to_string(asset.first));
					continue;
				}

				//ZONETOOL_INFO("Dumping additional asset \"%s\" of type \"%s\"", asset_name, type_to_string(asset.first));

				XAsset referenced_asset = {
					asset.first,
					asset_header
				};

				dump_asset_internal(&referenced_asset);
			}

			ZONETOOL_INFO("Zone \"%s\" dumped.", filesystem::get_fastfile().data());

			referenced_assets.clear();

			csv_file.close();
			csv_file = {};

			dump = false;
		}
		stopping = false;
	}

	utils::hook::detour DB_LinkXAssetEntry1_hook;
	XAssetEntry* DB_LinkXAssetEntry1(XAssetType type, XAssetHeader* header)
	{
		XAsset xasset = {
			type,
			*header
		};

		dump_asset(&xasset);

		return DB_LinkXAssetEntry1_hook.invoke<XAssetEntry*>(type, header);
	}

	utils::hook::detour DB_FinishLoadXFile_hook;
	void DB_FinishLoadXFile()
	{
		verify = false;
		stop_dumping();

		return DB_FinishLoadXFile_hook.invoke<void>();
	}

	utils::hook::detour Load_XGfxGlobals_hook;
	void Load_XGfxGlobals(bool atStreamStart)
	{
		Load_XGfxGlobals_hook.invoke<void>(atStreamStart);

		XGfxGlobals* varXGfxGlobals = *reinterpret_cast<XGfxGlobals**>(0x141F70400);
		xGfxGlobals_map[*g_zoneIndex] = varXGfxGlobals;
	}

	XGfxGlobals* GetXGfxGlobalsForCurrentZone()
	{
		return xGfxGlobals_map[*g_zoneIndex];
	}

	XGfxGlobals* GetXGfxGlobalsForZone(std::uint32_t zone_index)
	{
		return xGfxGlobals_map[zone_index];
	}

	void reallocate_asset_pool(const XAssetType type, const unsigned int new_size)
	{
		const size_t element_size = DB_GetXAssetTypeSize(type);

		auto* new_pool = utils::memory::get_allocator()->allocate(new_size * element_size);
		std::memmove(new_pool, g_assetPool[type], g_poolSize[type] * element_size);

		g_assetPool[type] = new_pool;
		g_poolSize[type] = new_size;
	}

	void reallocate_asset_pool_multiplier(const XAssetType type, unsigned int multiplier)
	{
		reallocate_asset_pool(type, multiplier * g_poolSize[type]);
	}

	bool load_zone(const std::string& name, DBSyncMode mode = DB_LOAD_SYNC, bool inform = true)
	{
		if (!zone_exists(name.data()))
		{
			ZONETOOL_INFO("Zone \"%s\" could not be found!", name.data());
			return false;
		}

		wait_for_database();

		if (!dump && !verify)
		{
			for (unsigned int i = 0; i < *g_zoneCount; i++)
			{
				if (!_strnicmp(g_zoneInfo[i].name, name.data(), 64))
				{
					if (inform)
					{
						ZONETOOL_INFO("zone \"%s\" is already loaded...", name.data());
					}
					return true;
				}
			}
		}
		if (inform)
		{
			ZONETOOL_INFO("Loading zone \"%s\"...", name.data());
		}

		XZoneInfo zone = { name.data(), DB_ZONE_GAME | DB_ZONE_CUSTOM, 0 };
		DB_LoadXAssets(&zone, 1, mode);
		return true;
	}

	void unload_zones()
	{
		ZONETOOL_INFO("Unloading zones...");

		static XZoneInfo zone = { 0, DB_ZONE_NONE, 70 };
		DB_LoadXAssets(&zone, 1, DB_LOAD_ASYNC_FORCE_FREE);

		ZONETOOL_INFO("Unloaded zones...");
	}

	void dump_zone(const std::string& name)
	{
		if (!zone_exists(name.data()))
		{
			ZONETOOL_INFO("Zone \"%s\" could not be found!", name.data());
			return;
		}

		wait_for_database();

		ZONETOOL_INFO("Dumping zone \"%s\"...", name.data());

		filesystem::set_fastfile(name);
		dump = true;
		if (!load_zone(name, DB_LOAD_ASYNC, false))
		{
			dump = false;
		}

		while (dump)
		{
			Sleep(1);
		}
	}

	void verify_zone(const std::string& name)
	{
		if (!zone_exists(name.data()))
		{
			ZONETOOL_INFO("Zone \"%s\" could not be found!", name.data());
			return;
		}

		wait_for_database();

		verify = true;
		if (!load_zone(name, DB_LOAD_ASYNC, true))
		{
			verify = false;
		}

		while (verify)
		{
			Sleep(1);
		}
	}

	void add_assets_using_iterator(const std::string& fastfile, const std::string& type, const std::string& folder,
		const std::string& extension, bool skip_reference, IZone* zone)
	{
		if (std::filesystem::is_directory("zonetool\\" + fastfile + "\\" + folder))
		{
			for (auto& file : std::filesystem::recursive_directory_iterator(
				"zonetool\\" + fastfile + "\\" + folder))
			{
				if (is_regular_file(file))
				{
					auto filename = file.path().filename().string();

					if (skip_reference && filename[0] == ',')
					{
						// skip this file
						continue;
					}

					if (!extension.empty())
					{
						// check if the filename contains the correct extension
						if (filename.length() > extension.length() &&
							filename.substr(filename.length() - extension.length()) == extension)
						{
							// remove the extension
							filename = filename.substr(0, filename.length() - extension.length());

							// add asset to disk
							zone->add_asset_of_type(type, filename);
						}
					}
					else if (file.path().extension().empty())
					{
						// add asset to disk
						zone->add_asset_of_type(type, filename);
					}
				}
			}
		}
	}

	void parse_csv_file(IZone* zone, const std::string& fastfile, const std::string& csv)
	{
		auto path = "zone_source\\" + csv + ".csv";
		auto parser = csv::parser(path.data(), ',');

		if (!parser.valid())
		{
			ZONETOOL_ERROR("Could not find csv file \"%s\" to build zone!", csv.data());
			return;
		}

		auto is_referencing = false;
		auto rows = parser.get_rows();
		if (rows != nullptr)
		{
			for (int row_index = 0; row_index < parser.get_num_rows(); row_index++)
			{
				auto* row = rows[row_index];
				if (row != nullptr)
				{
					if (row->fields)
					{
						// parse options
						if ((strlen(row->fields[0]) >= 1 && row->fields[0][0] == '#') || (strlen(row->fields[0]) >= 2 && row->
							fields[0][0] == '/' && row->fields[0][1] == '/'))
						{
							// comment line, go to next line.
							continue;
						}
						if (!strlen(row->fields[0]))
						{
							// empty line, go to next line.
							continue;
						}
						if (row->fields[0] == "require"s)
						{
							load_zone(row->fields[1], DB_LOAD_ASYNC);
							wait_for_database();
						}
						else if (row->fields[0] == "include"s)
						{
							parse_csv_file(zone, fastfile, row->fields[1]);
						}
						// this allows us to reference assets instead of rewriting them
						else if (row->fields[0] == "reference"s)
						{
							if (row->num_fields >= 2)
							{
								is_referencing = row->fields[1] == "true"s;
							}
						}
						// this will use a directory iterator to automatically add assets
						else if (row->fields[0] == "iterate"s)
						{
							if (row->num_fields >= 2)
							{
								auto type = row->fields[1];
								auto iterate_all = row->fields[1] == "true"s;

								try
								{
									if (type == "fx"s || iterate_all)
									{
										add_assets_using_iterator(fastfile, type, "effects", ".fxe", true, zone);
									}
									if (type == "material"s || iterate_all)
									{
										add_assets_using_iterator(fastfile, type, "materials", "", true, zone);
									}
									if (type == "xmodel"s || iterate_all)
									{
										add_assets_using_iterator(fastfile, type, "xmodel", ".xmodel_export", true, zone);
									}
									if (type == "xanim"s || iterate_all)
									{
										add_assets_using_iterator(fastfile, type, "xanim", ".xanim_export", true, zone);
									}
								}
								catch (std::exception& ex)
								{
									ZONETOOL_FATAL("A fatal exception occured while building zone \"%s\", exception was: \n%s", fastfile.data(), ex.what());
								}
							}
						}
						// if entry is not an option, it should be an asset.
						else
						{
							if (row->fields[0] == "localize"s && row->num_fields >= 2 &&
								filesystem::file("localizedstrings/"s + row->fields[1] + ".str").exists())
							{
								ILocalize::parse_localizedstrings_file(zone, row->fields[1]);
							}
							else if (row->fields[0] == "localize"s && row->num_fields >= 2 &&
								filesystem::file("localizedstrings/"s + row->fields[1] + ".json").exists())
							{
								ILocalize::parse_localizedstrings_json(zone, row->fields[1]);
							}
							else
							{
								if (row->num_fields >= 2)
								{
									if (is_valid_asset_type(row->fields[0]))
									{
										std::string name;
										if ((!row->fields[1] || !strlen(row->fields[1]) && row->fields[2] && strlen(row->fields[2])))
										{
											name = ","s + row->fields[2];
										}
										else
										{
											name = ((is_referencing) ? ","s : ""s) + row->fields[1];
										}

										try
										{
											zone->add_asset_of_type(
												row->fields[0],
												name
											);
										}
										catch (std::exception& ex)
										{
											ZONETOOL_FATAL("A fatal exception occured while building zone \"%s\", exception was: \n%s", fastfile.data(), ex.what());
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	std::shared_ptr<IZone> alloc_zone(const std::string& zone)
	{
		auto ptr = std::make_shared<Zone>(zone);
		return ptr;
	}

	std::shared_ptr<ZoneBuffer> alloc_buffer()
	{
		auto ptr = std::make_shared<ZoneBuffer>();
		ptr->init_streams(7);

		return ptr;
	}

	void build_zone(const std::string& fastfile)
	{
		// make sure FS is correct.
		filesystem::set_fastfile(fastfile);

		ZONETOOL_INFO("Building fastfile \"%s\"", fastfile.data());

		auto zone = alloc_zone(fastfile);
		if (zone == nullptr)
		{
			ZONETOOL_ERROR("An error occured while building fastfile \"%s\": Are you out of memory?", fastfile.data());
			return;
		}

		try
		{
			parse_csv_file(zone.get(), fastfile, fastfile);
		}
		catch (std::exception& ex)
		{
			ZONETOOL_FATAL("%s", ex.what());
		}

		// allocate zone buffer
		auto buffer = alloc_buffer();

		// add branding asset
		zone->add_asset_of_type("rawfile", fastfile);

		// compile zone
		zone->build(buffer.get());

		// clear asset shit
		ITechset::vertexdecl_pointers.clear();
	}

	void register_commands()
	{
		::iw6::command::add("quit", []()
		{
			std::quick_exit(EXIT_SUCCESS);
		});

		::iw6::command::add("buildzone", [](const ::iw6::command::params& params)
		{
			// Check if enough arguments have been passed to the command
			if (params.size() != 2)
			{
				ZONETOOL_ERROR("usage: buildzone <zone>");
				return;
			}

			build_zone(params.get(1));
		});

		::iw6::command::add("loadzone", [](const ::iw6::command::params& params)
		{
			// Check if enough arguments have been passed to the command
			if (params.size() != 2)
			{
				ZONETOOL_ERROR("usage: loadzone <zone>");
				return;
			}

			load_zone(params.get(1));
		});

		::iw6::command::add("unloadzones", []()
		{
			unload_zones();
		});

		::iw6::command::add("dumpzone", [](const ::iw6::command::params& params)
		{
			// Check if enough arguments have been passed to the command
			if (params.size() != 2 && params.size() != 3)
			{
				ZONETOOL_ERROR("usage: dumpzone <zone> <mode>");
				return;
			}

			if (params.size() == 3)
			{
				auto dump_mode_str = params.get(2);
				if (!_stricmp(dump_mode_str, "h1"))
				{
					dumping_mode = dump_mode::h1;
				}
			}
			else
			{
				dumping_mode = dump_mode::iw6;
			}

			dump_zone(params.get(1));
		});

		::iw6::command::add("verifyzone", [](const ::iw6::command::params& params)
		{
			// Check if enough arguments have been passed to the command
			if (params.size() != 2)
			{
				ZONETOOL_ERROR("usage: verifyzone <zone>");
				return;
			}

			verify_zone(params.get(1));
		});
	}

	std::vector<std::string> get_command_line_arguments()
	{
		LPWSTR* szArglist;
		int nArgs;

		szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

		std::vector<std::string> args;
		args.resize(nArgs);

		// convert all args to std::string
		for (int i = 0; i < nArgs; i++)
		{
			auto curArg = std::wstring(szArglist[i]);
			args[i] = std::string(curArg.begin(), curArg.end());
		}

		// return arguments
		return args;
	}

	void handle_params()
	{
		// Execute command line commands
		auto args = get_command_line_arguments();
		if (args.size() > 1)
		{
			for (std::size_t i = 0; i < args.size(); i++)
			{
				if (i < args.size() - 1 && i + 1 < args.size())
				{
					if (args[i] == "-loadzone")
					{
						load_zone(args[i + 1]);
						i++;
					}
					else if (args[i] == "-buildzone")
					{
						build_zone(args[i + 1]);
						i++;
					}
					else if (args[i] == "-buildzones")
					{
						const auto& filename = args[i + 1];
						std::string data{};
						if (!utils::io::read_file(filename, &data))
						{
							return;
						}

						const auto zones = utils::string::split(data, '\n');
						for (auto zone : zones)
						{
							if (zone.ends_with("\r"))
							{
								zone.pop_back();
							}

							build_zone(zone);
						}

						i++;
					}
					else if (args[i] == "-verifyzone")
					{
						verify_zone(args[i + 1]);
						i++;
					}
					else if (args[i] == "-dumpzone")
					{
						dump_zone(args[i + 1]);
						i++;
					}
				}
			}

			std::quick_exit(EXIT_SUCCESS);
		}
	}

	void branding()
	{
		ZONETOOL_INFO("ZoneTool initialization complete!");
		ZONETOOL_INFO("No matter how hard or unlikely, if it's possible, it will be done.");
		ZONETOOL_INFO("Special thanks to: RektInator, Laupetin, NTAuthority, momo5502, TheApadayo, localhost, X3RX35, homura, quaK & fed");
	}

	void on_exit(void)
	{
		dump = false;
		csv_file.close();
	}

	utils::hook::detour doexit_hook;
	void doexit(unsigned int a1, int a2, int a3)
	{
		on_exit();
		doexit_hook.invoke<void>(a1, a2, a3);
		//std::quick_exit(0);
	}

	void init_zonetool()
	{
		static bool initialized = false;
		if (initialized) return;
		initialized = true;

		ZONETOOL_INFO("ZoneTool is initializing...");

		// reallocs
		reallocate_asset_pool_multiplier(ASSET_TYPE_LUA_FILE, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_WEAPON, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_LOCALIZE_ENTRY, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_XANIMPARTS, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_ATTACHMENT, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_FONT, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_EQUIPMENT_SND_TABLE, 4);
		reallocate_asset_pool_multiplier(ASSET_TYPE_SNDDRIVER_GLOBALS, 4);
		reallocate_asset_pool_multiplier(ASSET_TYPE_SOUND, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_LOADED_SOUND, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_LEADERBOARD, 2);
		reallocate_asset_pool_multiplier(ASSET_TYPE_VERTEXDECL, 6);
		reallocate_asset_pool_multiplier(ASSET_TYPE_COMPUTESHADER, 4);
		reallocate_asset_pool_multiplier(ASSET_TYPE_REVERB_PRESET, 2);

		// allow overriding assets
		utils::hook::jump(0x1403206C4, 0x14032070D); // DB_LinkXAssetEntry0
		utils::hook::jump(0x140320A7F, 0x140320ACA); // DB_LinkXAssetEntry1

		// enable dumping
		DB_LinkXAssetEntry1_hook.create(0x1403208E0, &DB_LinkXAssetEntry1);

		// stop dumping
		DB_FinishLoadXFile_hook.create(0x1402FC360, &DB_FinishLoadXFile);

		// store xGfxGlobals pointers
		Load_XGfxGlobals_hook.create(0x1403102E0, &Load_XGfxGlobals);

		doexit_hook.create(0x1407340F4, doexit);
		atexit(on_exit);
	}

	void initialize()
	{
		init_zonetool();
	}

	void start()
	{
		initialize();

		branding();
		register_commands();

		handle_params();
	}
}