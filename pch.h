// pch.h : Pre-compiled header

#pragma once

#ifndef STRICT
#define STRICT
#endif

#include "targetver.h"

#define _ATL_APARTMENT_THREADED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS

#include <windows.h>
#include <windowsx.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <objbase.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>

#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>

#include <string>
#include <vector>
#include <algorithm>

#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

#include <compressapi.h>
#pragma comment(lib, "Cabinet.lib")

using namespace ATL;
