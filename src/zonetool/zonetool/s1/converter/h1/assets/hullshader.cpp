#include <std_include.hpp>
#include "zonetool/s1/converter/h1/include.hpp"
#include "hullshader.hpp"

#include "zonetool/h1/assets/hullshader.hpp"

namespace zonetool::s1
{
	namespace converter::h1
	{
		namespace hullshader
		{
			zonetool::h1::MaterialHullShader* convert(MaterialHullShader* asset, zone_memory* mem)
			{
				auto* new_asset = mem->allocate<zonetool::h1::MaterialHullShader>();

				std::memcpy(new_asset, asset, sizeof(MaterialHullShader));
				new_asset->name = mem->duplicate_string(asset->name + TECHSET_PREFIX);

				return new_asset;
			}

			void dump(MaterialHullShader* asset, zone_memory* mem)
			{
				auto* converted_asset = convert(asset, mem);
				zonetool::h1::hull_shader::dump(converted_asset);
			}
		}
	}
}