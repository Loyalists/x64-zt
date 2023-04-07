#include <std_include.hpp>
#include "zonetool/s1/converter/h1/include.hpp"
#include "vertexshader.hpp"

#include "zonetool/h1/assets/vertexshader.hpp"

namespace zonetool::s1
{
	namespace converter::h1
	{
		namespace vertexshader
		{
			zonetool::h1::MaterialVertexShader* convert(MaterialVertexShader* asset, zone_memory* mem)
			{
				auto* new_asset = mem->allocate<zonetool::h1::MaterialVertexShader>();

				std::memcpy(new_asset, asset, sizeof(MaterialVertexShader));
				new_asset->name = mem->duplicate_string(asset->name + TECHSET_PREFIX);

				return new_asset;
			}

			void dump(MaterialVertexShader* asset, zone_memory* mem)
			{
				auto* converted_asset = convert(asset, mem);
				zonetool::h1::vertex_shader::dump(converted_asset);
			}
		}
	}
}