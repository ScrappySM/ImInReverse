#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#define IMGUI_DEFINE_MATH_OPERATORS

#include <Windows.h>
#include <dwmapi.h>
#include <Psapi.h>
#include <tlhelp32.h>
#include <Wbemidl.h>
#include <comdef.h>

#include <d3d11.h>

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>
#include <imstb_textedit.h>

#include <font/IconsLucide.h>
#include <font/IconsLucide.h_lucide.ttf.h>

#include <spdlog/spdlog.h>

#include <functional>
#include <iostream>
#include <cstring>
#include <cassert>
#include <vector>
#include <memory>
#include <array>
#include <string>
#include <algorithm>
#include <thread>
#include <sstream>
#include <mutex>
#include <condition_variable>
