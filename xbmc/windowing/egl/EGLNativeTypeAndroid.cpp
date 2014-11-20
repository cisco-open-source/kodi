/*
 *      Copyright (C) 2011-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "system.h"
#if defined(HAVE_WAYLAND)
#include <wayland-egl.h>
#endif
#include <EGL/egl.h>
#include "EGLNativeTypeAndroid.h"
#include "utils/log.h"
#include "guilib/gui3d.h"
#if defined(TARGET_ANDROID)
  #include "android/activity/XBMCApp.h"
  #if defined(HAS_AMLPLAYER) || defined(HAS_LIBAMCODEC)
    #include "utils/AMLUtils.h"
  #endif
#endif
#include "utils/StringUtils.h"

CEGLNativeTypeAndroid::CEGLNativeTypeAndroid()
{
}

CEGLNativeTypeAndroid::~CEGLNativeTypeAndroid()
{
} 

bool CEGLNativeTypeAndroid::CheckCompatibility()
{
#if defined(TARGET_ANDROID)
  return true;
#endif
  return false;
}

void CEGLNativeTypeAndroid::Initialize()
{
#if defined(TARGET_ANDROID) && (defined(HAS_AMLPLAYER) || defined(HAS_LIBAMCODEC))
  aml_permissions();
  aml_cpufreq_min(true);
  aml_cpufreq_max(true);
#endif

  return;
}
void CEGLNativeTypeAndroid::Destroy()
{
#if defined(TARGET_ANDROID) && (defined(HAS_AMLPLAYER) || defined(HAS_LIBAMCODEC))
  aml_cpufreq_min(false);
  aml_cpufreq_max(false);
#endif

  return;
}

bool CEGLNativeTypeAndroid::CreateNativeDisplay()
{
  m_nativeDisplay = EGL_DEFAULT_DISPLAY;
  return true;
}

bool CEGLNativeTypeAndroid::CreateNativeWindow()
{
#if defined(TARGET_ANDROID)
  // Android hands us a window, we don't have to create it
  return true;
#else
  return false;
#endif
}  

bool CEGLNativeTypeAndroid::GetNativeDisplay(XBNativeDisplayType **nativeDisplay) const
{
  if (!nativeDisplay)
    return false;
  *nativeDisplay = (XBNativeDisplayType*) &m_nativeDisplay;
  return true;
}

bool CEGLNativeTypeAndroid::GetNativeWindow(XBNativeWindowType **nativeWindow) const
{
#if defined(TARGET_ANDROID)
  if (!nativeWindow)
    return false;
  *nativeWindow = (XBNativeWindowType*) CXBMCApp::GetNativeWindow(30000);
  return (*nativeWindow != NULL);
#else
  return false;
#endif
}

bool CEGLNativeTypeAndroid::DestroyNativeDisplay()
{
  return true;
}

bool CEGLNativeTypeAndroid::DestroyNativeWindow()
{
  return true;
}

bool CEGLNativeTypeAndroid::GetNativeResolution(RESOLUTION_INFO *res) const
{
#if defined(TARGET_ANDROID)
  EGLNativeWindowType *nativeWindow = (EGLNativeWindowType*)CXBMCApp::GetNativeWindow(30000);
  if (!nativeWindow)
    return false;

  ANativeWindow_acquire(*nativeWindow);
  res->iWidth = ANativeWindow_getWidth(*nativeWindow);
  res->iHeight= ANativeWindow_getHeight(*nativeWindow);
  ANativeWindow_release(*nativeWindow);

  res->fRefreshRate = 60;
  res->dwFlags= D3DPRESENTFLAG_PROGRESSIVE;
  res->iScreen       = 0;
  res->bFullScreen   = true;
  res->iSubtitles    = (int)(0.965 * res->iHeight);
  res->fPixelRatio   = 1.0f;
  res->iScreenWidth  = res->iWidth;
  res->iScreenHeight = res->iHeight;
  res->strMode       = StringUtils::Format("%dx%d @ %.2f%s - Full Screen", res->iScreenWidth, res->iScreenHeight, res->fRefreshRate,
  res->dwFlags & D3DPRESENTFLAG_INTERLACED ? "i" : "");
  CLog::Log(LOGNOTICE,"Current resolution: %s\n",res->strMode.c_str());
  return true;
#else
  return false;
#endif
}

bool CEGLNativeTypeAndroid::SetNativeResolution(const RESOLUTION_INFO &res)
{
  return false;
}

bool CEGLNativeTypeAndroid::ProbeResolutions(std::vector<RESOLUTION_INFO> &resolutions)
{
  RESOLUTION_INFO res;
  bool ret = false;
  ret = GetNativeResolution(&res);
  if (ret && res.iWidth > 1 && res.iHeight > 1)
  {
    resolutions.push_back(res);
    return true;
  }
  return false;
}

bool CEGLNativeTypeAndroid::GetPreferredResolution(RESOLUTION_INFO *res) const
{
  return false;
}

bool CEGLNativeTypeAndroid::ShowWindow(bool show)
{
  return false;
}
