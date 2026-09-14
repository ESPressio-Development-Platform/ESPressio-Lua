#pragma once
/// <summary>Public ESPressio Lua 5.5.1 type-binding and scripting-instance API.</summary>
#include "ESPressio_LuaInstance.hpp"

#if __has_include(<ESPressio_TypeDirectory.hpp>)
#include "ESPressio_LuaPrimitiveDiscovery.hpp"
#endif

#if __has_include(<ESPressio_CommandDescriptor.hpp>) && __has_include(<ESPressio_TypeDirectory.hpp>)
#include "ESPressio_LuaCommand.hpp"
#endif
