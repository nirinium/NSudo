﻿/******************************************************************************
Project: M2-Team Common Library
Description: Implementation for the message dialog.
File Name: M2MessageDialog.cpp
License: The MIT License
******************************************************************************/

#include "stdafx.h"

#include <Windows.h>

#include "M2DPIScaling.h"
#include "M2MessageDialog.h"
#include "M2MessageDialogResource.h"

// Creates and shows the message dialog.
// Parameters:
//   hInstance: A handle to the module which contains the message dialog 
//   resource. If this parameter is nullptr, then the current executable is 
//   used.
//   hWndParent: A handle to the window that owns the message dialog. 
//   lpIconName: Pointer that references the icon to be displayed in the 
//   message dialog. If this parameter is nullptr or the hInstance parameter is
//   nullptr, no icon will be displayed. This parameter must be an integer 
//   resource identifier passed to the MAKEINTRESOURCE macro.
//   lpTitle: Pointer to the string to be used for the message dialog title. 
//   This parameter is a null-terminated, Unicode string. 
//   lpContent: Pointer to the string to be used for the message dialog 
//   content. This parameter is a null-terminated, Unicode string. 
// Return value:
//   If the function succeeds, the return value is the value of the nResult
//   parameter specified in the call to the EndDialog function used to 
//   terminate the message dialog.
//   If the function fails because the hWndParent parameter is invalid, the 
//   return value is zero. The function returns zero in this case for 
//   compatibility with previous versions of Windows. If the function fails for
//   any other reason, the return value is –1. To get extended error 
//   information, call GetLastError.
INT_PTR M2MessageDialog(
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ HWND hWndParent,
	_In_opt_ LPCWSTR lpIconName,
	_In_ LPCWSTR lpTitle,
	_In_ LPCWSTR lpContent)
{
	struct DIALOG_BOX_PARAM
	{
		HINSTANCE hInstance;
		LPCWSTR lpIconName;
		LPCWSTR lpTitle;
		LPCWSTR lpContent;
	};

	DIALOG_BOX_PARAM Param = { hInstance, lpIconName,lpTitle,lpContent };

	auto DialogCallBack = [](
		_In_ HWND hDlg,
		_In_ UINT uMsg,
		_In_ WPARAM wParam,
		_In_ LPARAM lParam)
	{
		UNREFERENCED_PARAMETER(lParam);

		if (WM_INITDIALOG == uMsg)
		{
			HICON hIcon = reinterpret_cast<HICON>(LoadImageW(
				reinterpret_cast<DIALOG_BOX_PARAM*>(lParam)->hInstance,
				reinterpret_cast<DIALOG_BOX_PARAM*>(lParam)->lpIconName,
				IMAGE_ICON,
				256,
				256,
				LR_SHARED));
			if (nullptr != hIcon)
			{
				SendMessageW(
					hDlg,
					WM_SETICON,
					ICON_SMALL,
					reinterpret_cast<LPARAM>(hIcon));
				SendMessageW(
					hDlg,
					WM_SETICON,
					ICON_BIG,
					reinterpret_cast<LPARAM>(hIcon));
			}

			SetWindowTextW(
				hDlg,
				reinterpret_cast<DIALOG_BOX_PARAM*>(lParam)->lpTitle);
			SetWindowTextW(
				GetDlgItem(hDlg, IDC_MESSAGE_DIALOG_EDIT),
				reinterpret_cast<DIALOG_BOX_PARAM*>(lParam)->lpContent);

			return (INT_PTR)TRUE;
		}
		else if (
			(WM_CLOSE == uMsg) ||
			(WM_COMMAND == uMsg && IDOK == LOWORD(wParam)))
		{
			EndDialog(hDlg, 0);
		}

		return FALSE;
	};


	M2EnablePerMonitorDialogScaling();

	return DialogBoxParamW(
		hInstance,
		MAKEINTRESOURCEW(IDD_MESSAGE_DIALOG),
		hWndParent,
		DialogCallBack,
		reinterpret_cast<LPARAM>(&Param));
}