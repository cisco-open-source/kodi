/*
 *      Copyright (C) 2005-2013 Team XBMC
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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(TARGET_WINDOWS) && !defined(__ppc__) && !defined(__powerpc__) && !defined(TARGET_ANDROID) && !defined(TARGET_DARWIN_IOS) && !defined(_mips)
void * fast_memcpy(void * to, const void * from, size_t len);
//#define fast_memcpy memcpy
#else
#define fast_memcpy memcpy
#endif

#ifdef __cplusplus
}
#endif
