#pragma once

#include "Engine/Platform/IWindowEnumerator.hpp"
#include "Engine/Platform/IScreenCapture.hpp"
#include "Engine/Platform/IWindowPlatform.hpp"

#include <memory>

namespace PlatformServices
{
std::unique_ptr<IWindowEnumerator> createWindowEnumerator();
std::unique_ptr<IScreenCapture>    createScreenCapture();
std::unique_ptr<IWindowPlatform>   createWindowPlatform();
} // namespace PlatformServices
