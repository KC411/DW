/************************************************************************
 * unicodewrap.c  Unicode wrapper functions for Win32                   *
 * Copyright (C)  2002-2004  Ben Webb                                   *
 *                Email: benwebb@users.sf.net                           *
 *                WWW: https://dopewars.sourceforge.io/                 *
 *                                                                      *
 * This program is free software; you can redistribute it and/or        *
 * modify it under the terms of the GNU General Public License          *
 * as published by the Free Software Foundation; either version 2       *
 * of the License, or (at your option) any later version.               *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program; if not, write to the Free Software          *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,               *
 *                   MA  02111-1307, USA.                               *
 ************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef CYGWIN
#include <winsock2.h>
#include <windows.h>
#include <glib.h>

#include "unicodewrap.h"

/*
 * Converts a string from our internal representation (UTF-8) to a form
 * suitable for Windows Unicode-aware functions (i.e. UTF-16). This
 * returned string must be g_free'd when no longer needed.
 */
gunichar2 *strtow32(const char *instr, int len)
{
  gunichar2 *outstr;
  if (!instr) {
    return NULL;
  }
  outstr = g_utf8_to_utf16(instr, len, NULL, NULL, NULL);
  if (!outstr) {
    outstr = g_utf8_to_utf16("[?]", len, NULL, NULL, NULL);
  }
  return outstr;
}

gchar *w32tostr(const gunichar2 *instr, int len)
{
  gchar *outstr;
  if (!instr) {
    return NULL;
  }
  outstr = g_utf16_to_utf8(instr, len, NULL, NULL, NULL);
  if (!outstr) {
    outstr = g_strdup("[?]");
  }
  return outstr;
}

BOOL mySetWindowText(HWND hWnd, LPCTSTR lpString)
{
  BOOL retval;
  gunichar2 *text;
  text = strtow32(lpString, -1);
  retval = SetWindowTextW(hWnd, text);
  g_free(text);
  return retval;
}

HWND myCreateWindow(LPCTSTR lpClassName, LPCTSTR lpWindowName, DWORD dwStyle,
                    int x, int y, int nWidth, int nHeight, HWND hwndParent,
                    HMENU hMenu, HANDLE hInstance, LPVOID lpParam)
{
  return myCreateWindowEx(0, lpClassName, lpWindowName, dwStyle, x, y,
                          nWidth, nHeight, hwndParent, hMenu, hInstance,
                          lpParam);
}

HWND myCreateWindowEx(DWORD dwExStyle, LPCTSTR lpClassName,
                      LPCTSTR lpWindowName, DWORD dwStyle, int x, int y,
                      int nWidth, int nHeight, HWND hwndParent, HMENU hMenu,
                      HANDLE hInstance, LPVOID lpParam)
{
  HWND retval;
  gunichar2 *classname, *winname;
  classname = strtow32(lpClassName, -1);
  winname = strtow32(lpWindowName, -1);
  retval = CreateWindowExW(dwExStyle, classname, winname, dwStyle, x, y,
                           nWidth, nHeight, hwndParent, hMenu, hInstance,
                           lpParam);
  g_free(classname);
  g_free(winname);
  return retval;
}

gchar *myGetWindowText(HWND hWnd)
{
  gint textlen;
  gunichar2 *buffer;
  gchar *retstr;

  textlen = SendMessage(hWnd, WM_GETTEXTLENGTH, 0, 0);

  buffer = g_new0(gunichar2, textlen + 1);
  GetWindowTextW(hWnd, buffer, textlen + 1);
  buffer[textlen] = '\0';
  retstr = w32tostr(buffer, textlen);
  g_free(buffer);
  return retstr;
}

int myDrawText(HDC hDC, LPCTSTR lpString, int nCount, LPRECT lpRect,
               UINT uFormat)
{
  int retval;
  gunichar2 *text;

  text = strtow32(lpString, nCount);
  retval = DrawTextW(hDC, text, -1, lpRect, uFormat);
  g_free(text);
  return retval;
}

static BOOL makeMenuItemInfoW(LPMENUITEMINFOW lpmiiw, LPMENUITEMINFO lpmii)
{
  BOOL strdata;
  strdata = (lpmii->fMask & MIIM_TYPE)
            && !(lpmii->fType & (MFT_BITMAP | MFT_SEPARATOR));

  lpmiiw->cbSize = sizeof(MENUITEMINFOW);
  lpmiiw->fMask = lpmii->fMask;
  lpmiiw->fType = lpmii->fType;
  lpmiiw->fState = lpmii->fState;
  lpmiiw->wID = lpmii->wID;
  lpmiiw->hSubMenu = lpmii->hSubMenu;
  lpmiiw->hbmpChecked = lpmii->hbmpChecked;
  lpmiiw->hbmpUnchecked = lpmii->hbmpUnchecked;
  lpmiiw->dwItemData = lpmii->dwItemData;
  lpmiiw->dwTypeData = strdata ? strtow32(lpmii->dwTypeData, -1)
                               : (LPWSTR)lpmii->dwTypeData;
  lpmiiw->cch = lpmii->cch;
  return strdata;
}

BOOL WINAPI mySetMenuItemInfo(HMENU hMenu, UINT uItem, BOOL fByPosition,
                              LPMENUITEMINFO lpmii)
{
  BOOL retval;
  MENUITEMINFOW miiw;
  BOOL strdata;

  strdata = makeMenuItemInfoW(&miiw, lpmii);
  retval = SetMenuItemInfoW(hMenu, uItem, fByPosition, &miiw);
  if (strdata) {
    g_free(miiw.dwTypeData);
  }
  return retval;
}

BOOL WINAPI myInsertMenuItem(HMENU hMenu, UINT uItem, BOOL fByPosition,
                             LPMENUITEMINFO lpmii)
{
  BOOL retval;
  MENUITEMINFOW miiw;
  BOOL strdata;

  strdata = makeMenuItemInfoW(&miiw, lpmii);
  retval = InsertMenuItemW(hMenu, uItem, fByPosition, &miiw);
  if (strdata) {
    g_free(miiw.dwTypeData);
  }
  return retval;
}

static BOOL makeHeaderItemW(HD_ITEMW *phdiw, const HD_ITEM *phdi)
{
  BOOL strdata;

  strdata = phdi->mask & HDI_TEXT;

  phdiw->mask = phdi->mask;
  phdiw->cxy = phdi->cxy;
  phdiw->pszText = strdata ? strtow32(phdi->pszText, -1)
                           : (LPWSTR)phdi->pszText;
  phdiw->hbm = phdi->hbm;
  phdiw->cchTextMax = phdi->cchTextMax;
  phdiw->fmt = phdi->fmt;
  phdiw->lParam = phdi->lParam;
  return strdata;
}

static BOOL makeTabItemW(TC_ITEMW *tiew, const TC_ITEM *tie)
{
  BOOL strdata;

  strdata = tie->mask & TCIF_TEXT;
  tiew->mask = tie->mask;
  tiew->pszText = strdata ? strtow32(tie->pszText, -1)
                          : (LPWSTR)tie->pszText;
  tiew->cchTextMax = tie->cchTextMax;
  tiew->iImage = tie->iImage;
  tiew->lParam = tie->lParam;
  return strdata;
}

int myHeader_InsertItem(HWND hWnd, int index, const HD_ITEM *phdi)
{
  int retval;
  if (IsWindowUnicode(hWnd)) {
    HD_ITEMW hdiw;
    BOOL strdata;

    strdata = makeHeaderItemW(&hdiw, phdi);
    retval = (int)SendMessageW(hWnd, HDM_INSERTITEMW, (WPARAM)index,
                               (LPARAM)&hdiw);
    if (strdata) {
      g_free(hdiw.pszText);
    }
  } else {
    retval = (int)SendMessageA(hWnd, HDM_INSERTITEMA, (WPARAM)index,
                               (LPARAM)phdi);
  }
  return retval;
}

int myTabCtrl_InsertItem(HWND hWnd, int index, const TC_ITEM *pitem)
{
  int retval;
  TC_ITEMW tiew;
  BOOL strdata;
  strdata = makeTabItemW(&tiew, pitem);
  retval = (int)SendMessageW(hWnd, TCM_INSERTITEMW, (WPARAM)index,
                             (LPARAM)&tiew);
  if (strdata) {
    g_free(tiew.pszText);
  }
  return retval;
}

ATOM myRegisterClass(CONST WNDCLASS *lpWndClass)
{
  ATOM retval;
  WNDCLASSW wcw;

  wcw.style = lpWndClass->style;
  wcw.lpfnWndProc = lpWndClass->lpfnWndProc;
  wcw.cbClsExtra = lpWndClass->cbClsExtra;
  wcw.cbWndExtra = lpWndClass->cbWndExtra;
  wcw.hInstance = lpWndClass->hInstance;
  wcw.hIcon = lpWndClass->hIcon;
  wcw.hCursor = lpWndClass->hCursor;
  wcw.hbrBackground = lpWndClass->hbrBackground;
  wcw.lpszMenuName = strtow32(lpWndClass->lpszMenuName, -1);
  wcw.lpszClassName = strtow32(lpWndClass->lpszClassName, -1);
  retval = RegisterClassW(&wcw);
  g_free((LPWSTR)wcw.lpszMenuName);
  g_free((LPWSTR)wcw.lpszClassName);
  return retval;
}

HWND myCreateDialog(HINSTANCE hInstance, LPCTSTR lpTemplate, HWND hWndParent,
                    DLGPROC lpDialogFunc)
{
  HWND retval;
  gunichar2 *text;
  text = strtow32(lpTemplate, -1);
  retval = CreateDialogW(hInstance, text, hWndParent, lpDialogFunc);
  g_free(text);
  return retval;
}

void myEditReplaceSel(HWND hWnd, BOOL fCanUndo, LPCSTR lParam)
{
  gunichar2 *text;
  text = strtow32(lParam, -1);
  SendMessageW(hWnd, EM_REPLACESEL, (WPARAM)fCanUndo, (LPARAM)text);
  g_free(text);
}

int myMessageBox(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType)
{
  int retval;
  gunichar2 *text, *caption;
  text = strtow32(lpText, -1);
  caption = strtow32(lpCaption, -1);
  retval = MessageBoxW(hWnd, text, caption, uType);
  g_free(text);
  g_free(caption);
  return retval;
}

size_t myw32strlen(const char *str)
{
  return g_utf8_strlen(str, -1);
}

LRESULT myComboBox_AddString(HWND hWnd, LPCTSTR text)
{
  LRESULT retval;
  gunichar2 *w32text;
  w32text = strtow32(text, -1);
  retval = SendMessageW(hWnd, CB_ADDSTRING, 0, (LPARAM)w32text);
  g_free(w32text);
  return retval;
}

#endif /* CYGWIN */
