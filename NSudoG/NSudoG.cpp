// NSudoG.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"

HINSTANCE g_hInstance;

std::wstring nsudo_app_path;

nlohmann::json nsudo_config;
nlohmann::json nsudo_shortcut_list_v2;

nlohmann::json NSudo_String_Translations;

#include <Userenv.h>
#pragma comment(lib, "Userenv.lib")

#include "NSudoVersion.h"

#include "NSudoContextMenuManagement.h"
#include "M2MessageDialog.h"

namespace ProjectInfo
{
	const wchar_t VersionText[] = L"M2-Team NSudo " NSUDO_VERSION_STRING;

	const wchar_t LogoText[] =
		L"M2-Team NSudo " NSUDO_VERSION_STRING L"\r\n"
		L"© M2-Team. All rights reserved.\r\n"
		L"\r\n";
}

class CResource
{
private:
	HMODULE m_Module = nullptr;
	HRSRC m_Resource = nullptr;

public:
	CResource(
		_In_ LPCWSTR lpType,
		_In_ UINT uID) :
		m_Module(GetModuleHandleW(nullptr)),
		m_Resource(FindResourceExW(
			this->m_Module,
			lpType,
			MAKEINTRESOURCEW(uID),
			MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL)))
	{

	}

	LPVOID Get()
	{
		return LockResource(LoadResource(this->m_Module, this->m_Resource));
	}

	DWORD Size()
	{
		return SizeofResource(this->m_Module, this->m_Resource);
	}

};

std::wstring NSudoGetUTF8WithBOMStringResources(
	_In_ UINT uID)
{
	CResource Resource(L"String", uID);

	std::string RawString(
		reinterpret_cast<const char*>(Resource.Get()),
		Resource.Size());

	// Raw string without the UTF-8 BOM. (0xEF,0xBB,0xBF)	
	return m2_base_utf8_to_utf16(RawString.c_str() + 3);
}

void NSudoPrintMsg(
	_In_opt_ HINSTANCE hInstance,
	_In_opt_ HWND hWnd,
	_In_ LPCWSTR lpContent)
{
	std::wstring DialogContent =
		std::wstring(ProjectInfo::LogoText) +
		lpContent +
		NSudoGetUTF8WithBOMStringResources(IDR_String_Links);
	
	M2MessageDialog(
		hInstance,
		hWnd,
		MAKEINTRESOURCE(IDI_NSUDO),
		L"NSudo",
		DialogContent.c_str());
}

std::wstring NSudoGetTranslation(
	_In_ const char* Key)
{
	return m2_base_utf8_to_utf16(
		NSudo_String_Translations[Key].get<std::string>());
}

/*
SuCreateProcess函数创建一个新进程和对应的主线程
The SuCreateProcess function creates a new process and its primary thread.

如果函数执行失败，返回值为NULL。调用GetLastError可获取详细错误码。
If the function fails, the return value is NULL. To get extended error
information, call GetLastError.
*/
bool SuCreateProcess(
	_In_opt_ HANDLE hToken,
	_Inout_ LPCWSTR lpCommandLine)
{
	STARTUPINFOW StartupInfo = { 0 };
	PROCESS_INFORMATION ProcessInfo = { 0 };

	std::wstring ComSpec(MAX_PATH, L'\0');
	GetEnvironmentVariableW(L"ComSpec", &ComSpec[0], (DWORD)ComSpec.size());
	ComSpec.resize(wcslen(ComSpec.c_str()));

	//生成命令行
	std::wstring final_command_line = L"/c start \"" + ComSpec + L"\" ";

	try
	{
		final_command_line += m2_base_utf8_to_utf16(
			nsudo_shortcut_list_v2[m2_base_utf16_to_utf8(lpCommandLine)].get<std::string>());
	}
	catch (const std::exception&)
	{
		final_command_line += lpCommandLine;
	}

	//设置进程所在桌面
	StartupInfo.lpDesktop = const_cast<LPWSTR>(L"WinSta0\\Default");

	LPVOID lpEnvironment = nullptr;

	BOOL result = FALSE;

	M2::CHandle hCurrentToken;
	if (OpenProcessToken(
		GetCurrentProcess(),
		MAXIMUM_ALLOWED,
		&hCurrentToken))
	{
		if (CreateEnvironmentBlock(&lpEnvironment, hCurrentToken, TRUE))
		{
			//启动进程
			result = CreateProcessAsUserW(
				hToken,
				ComSpec.c_str(),
				&final_command_line[0],
				nullptr,
				nullptr,
				FALSE,
				CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
				lpEnvironment,
				nsudo_app_path.c_str(),
				&StartupInfo,
				&ProcessInfo);

			//关闭句柄
			if (result)
			{
				CloseHandle(ProcessInfo.hProcess);
				CloseHandle(ProcessInfo.hThread);
			}

			DestroyEnvironmentBlock(lpEnvironment);
		}
	}

	//返回结果
	return result;
}

HRESULT NSudoShowAboutDialog(
	_In_ HWND hwndParent)
{
	std::wstring DialogContent =
		std::wstring(ProjectInfo::LogoText) +
		NSudoGetUTF8WithBOMStringResources(IDR_String_CommandLineHelp) +
		NSudoGetUTF8WithBOMStringResources(IDR_String_Links);

	SetLastError(ERROR_SUCCESS);

	M2MessageDialog(
		g_hInstance,
		hwndParent,
		MAKEINTRESOURCE(IDI_NSUDO),
		L"NSudo", 
		DialogContent.c_str());

	return NSudoGetLastCOMError();
}

// 分割获取的命令行以方便解析
std::vector<std::wstring> NSudoSplitCommandLine(LPCWSTR lpCommandLine)
{
	std::vector<std::wstring> result;

	int argc = 0;
	wchar_t **argv = CommandLineToArgvW(lpCommandLine, &argc);

	size_t arg_size = 0;

	for (int i = 0; i < argc; ++i)
	{
		// 如果是程序路径或者为命令参数
		if (i == 0 || (argv[i][0] == L'-' || argv[i][0] == L'/'))
		{
			std::wstring arg(argv[i]);

			// 累加长度 (包括空格)
			// 为最后成功保存用户要执行的命令或快捷命令名打基础
			arg_size += arg.size() + 1;

			// 保存入解析器
			result.push_back(arg);
		}
		else
		{
			// 获取搜索用户要执行的命令或快捷命令名的位置（大致位置）
			// 对arg_size减1是为了留出空格，保证程序路径没有引号时也能正确解析
			wchar_t* search_start =
				const_cast<wchar_t*>(lpCommandLine) + (arg_size - 1);

			// 获取用户要执行的命令或快捷命令名
			// 搜索第一个参数分隔符（即空格）开始的位置			
			// 最后对结果增1是因为该返回值是空格开始出，而最开始的空格需要排除
			wchar_t* command = wcsstr(search_start, L" ") + 1;

			std::wstring final_command;

			// 如果最外层有引号则去除，否则直接生成
			if (command[0] == L'\"' || command[0] == L'\'')
			{
				final_command = std::wstring(command + 1);
				final_command.resize(final_command.size() - 1);
			}
			else
			{
				final_command = std::wstring(command);
			}

			// 保存入解析器
			result.push_back(final_command);

			break;
		}
	}

	return result;
}

// 解析命令行
int NSudoCommandLineParser(
	_In_ bool bElevated,
	_In_ std::vector<std::wstring>& args)
{
	if (2 == args.size())
	{
		// 如果参数不满足条件，则返回错误
		if (!(args[1][0] == L'-' || args[1][0] == L'/'))
		{
			std::wstring Buffer = NSudoGetTranslation("Error.Arg");
			NSudoPrintMsg(g_hInstance, nullptr, Buffer.c_str());
			return -1;
		}

		const wchar_t* arg = args[1].c_str() + 1;

		// 如果参数是 /? 或 -?，则显示帮助
		if (0 == _wcsicmp(arg, L"?"))
		{
			NSudoShowAboutDialog(nullptr);
			return 0;
		}

		CNSudoContextMenuManagement ContextMenuManagement;

		if (0 == _wcsicmp(arg, L"Install"))
		{
			// 如果参数是 /Install 或 -Install，则安装NSudo到系统
			if (ERROR_SUCCESS != ContextMenuManagement.Install())
			{
				ContextMenuManagement.Uninstall();
			}

			return 0;
		}	
		else if (0 == _wcsicmp(arg, L"Uninstall"))
		{
			// 如果参数是 /Uninstall 或 -Uninstall，则移除安装到系统的NSudo
			ContextMenuManagement.Uninstall();
			return 0;
		}
		else
		{
			std::wstring Buffer = NSudoGetTranslation("Error.Arg");
			NSudoPrintMsg(g_hInstance, nullptr, Buffer.c_str());
			return -1;
		}
	}

	DWORD dwSessionID = (DWORD)-1;

	// 获取当前进程会话ID
	if (!NSudoGetCurrentProcessSessionID(&dwSessionID))
	{
		std::wstring Buffer = NSudoGetTranslation("Error.Sudo");
		NSudoPrintMsg(g_hInstance, nullptr, Buffer.c_str());
		return 0;
	}

	// 如果未提权或者模拟System权限失败
	if (!(bElevated && NSudoImpersonateAsSystem()))
	{
		std::wstring Buffer = NSudoGetTranslation("Error.NotHeld");
		NSudoPrintMsg(g_hInstance, nullptr, Buffer.c_str());
		return 0;
	}

	bool bArgErr = false;

	bool bGetUser = false;
	bool bGetPrivileges = false;
	bool bGetIntegrityLevel = false;

	M2::CHandle hToken;
	M2::CHandle hTempToken;

	// 解析参数，忽略第一项（必定是程序路径）和最后一项（因为必定是命令行）
	for (size_t i = 1; i < args.size() - 1; ++i)
	{
		// 如果参数不满足条件，则返回错误
		if (!(args[i][0] == L'-' || args[i][0] == L'/'))
		{
			bArgErr = true;
			break;
		}

		const wchar_t* arg = args[i].c_str() + 1;

		if (!bGetUser)
		{
			if (0 == _wcsicmp(arg, L"U:T"))
			{
				if (NSudoDuplicateServiceToken(
					L"TrustedInstaller",
					MAXIMUM_ALLOWED,
					nullptr,
					SecurityIdentification,
					TokenPrimary,
					&hToken))
				{
					SetTokenInformation(
						hToken,
						TokenSessionId,
						(PVOID)&dwSessionID,
						sizeof(DWORD));
				}
			}
			else if (0 == _wcsicmp(arg, L"U:S"))
			{
				NSudoDuplicateSystemToken(
					MAXIMUM_ALLOWED,
					nullptr,
					SecurityIdentification,
					TokenPrimary,
					&hToken);
			}
			else if (0 == _wcsicmp(arg, L"U:C"))
			{
				NSudoDuplicateSessionToken(
					dwSessionID,
					MAXIMUM_ALLOWED,
					nullptr,
					SecurityIdentification,
					TokenPrimary,
					&hToken);
			}
			else if (0 == _wcsicmp(arg, L"U:P"))
			{
				OpenProcessToken(
					GetCurrentProcess(), MAXIMUM_ALLOWED, &hToken);
			}
			else if (0 == _wcsicmp(arg, L"U:D"))
			{
				if (OpenProcessToken(
					GetCurrentProcess(), MAXIMUM_ALLOWED, &hTempToken))
				{
					NSudoCreateLUAToken(&hToken, hTempToken);
				}
			}
			else
			{
				bArgErr = true;
				break;
			}

			bGetUser = true;
		}
		else if (!bGetPrivileges)
		{
			if (0 == _wcsicmp(arg, L"P:E"))
			{
				NSudoSetTokenAllPrivileges(hToken, true);
			}
			else if (0 == _wcsicmp(arg, L"P:D"))
			{
				NSudoSetTokenAllPrivileges(hToken, false);
			}
			else
			{
				bArgErr = true;
				break;
			}

			bGetPrivileges = true;
		}
		else if (!bGetIntegrityLevel)
		{
			if (0 == _wcsicmp(arg, L"M:S"))
			{
				NSudoSetTokenIntegrityLevel(hToken, SystemLevel);
			}
			else if (0 == _wcsicmp(arg, L"M:H"))
			{
				NSudoSetTokenIntegrityLevel(hToken, HighLevel);
			}
			else if (0 == _wcsicmp(arg, L"M:M"))
			{
				NSudoSetTokenIntegrityLevel(hToken, MediumLevel);
			}
			else if (0 == _wcsicmp(arg, L"M:L"))
			{
				NSudoSetTokenIntegrityLevel(hToken, LowLevel);
			}
			else
			{
				bArgErr = true;
				break;
			}

			bGetIntegrityLevel = true;
		}
	}

	if (bArgErr)
	{
		std::wstring Buffer = NSudoGetTranslation("Error.Arg");
		NSudoPrintMsg(g_hInstance, nullptr, Buffer.c_str());
		return -1;
	}
	else
	{
		if (!bGetUser)
		{
			if (NSudoDuplicateServiceToken(
				L"TrustedInstaller",
				MAXIMUM_ALLOWED,
				nullptr,
				SecurityIdentification,
				TokenPrimary,
				&hToken))
			{
				if (SetTokenInformation(
					hToken,
					TokenSessionId,
					(PVOID)&dwSessionID,
					sizeof(DWORD)))
				{
					NSudoSetTokenAllPrivileges(hToken, true);
				}
			}
		}

		if (!SuCreateProcess(hToken, args[args.size() - 1].c_str()))
		{
			std::wstring Buffer = NSudoGetTranslation("Error.Sudo");
			NSudoPrintMsg(g_hInstance, nullptr, Buffer.c_str());
		}
	}

	RevertToSelf();

	return 0;
}

int WINAPI wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nShowCmd)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nShowCmd);

	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	std::vector<std::wstring> command_args = NSudoSplitCommandLine(GetCommandLineW());

	g_hInstance = hInstance;

	std::wstring nsudo_exe_path = M2GetCurrentModulePath();

	nsudo_app_path = nsudo_exe_path;
	wcsrchr(&nsudo_app_path[0], L'\\')[0] = L'\0';
	nsudo_app_path.resize(wcslen(nsudo_app_path.c_str()));

	CResource Resource(L"String", IDR_String_Translations);
	std::string RawString(
		reinterpret_cast<const char*>(Resource.Get()),
		Resource.Size());
	NSudo_String_Translations =
		nlohmann::json::parse(RawString)["Translations"];

	try
	{
		std::ifstream fs;
		fs.open(nsudo_app_path + L"\\NSudo.json");

		nsudo_config = nlohmann::json::parse(fs);
		nsudo_shortcut_list_v2 = nsudo_config["ShortCutList_V2"];
	}
	catch (const std::exception&)
	{

	}

	M2::CHandle CurrentToken;

	bool bElevated = false;
	if (OpenProcessToken(GetCurrentProcess(), MAXIMUM_ALLOWED, &CurrentToken))
	{
		bElevated = NSudoSetTokenPrivilege(
			CurrentToken, SeDebugPrivilege, true);
	}

	if (command_args.size() == 1)
	{
		command_args.push_back(L"-?");
	}
	
	return NSudoCommandLineParser(bElevated, command_args);
}
