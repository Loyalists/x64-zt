#pragma once

namespace zonetool::s1
{
	namespace converter::h1
	{
		namespace material
		{
			zonetool::h1::Material* convert(Material* asset, zone_memory* mem);
			void dump(Material* asset, zone_memory* mem);
		}
	}
}