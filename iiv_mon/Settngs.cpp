#include <string>

#include "../../lsMisc/stdosd/stdosd.h"
#include "../../lsMisc/UTF16toUTF8.h"
#include "../../profile/cpp/Profile/include/ambiesoft.profile.h"
#include "Settngs.h"

using namespace Ambiesoft;
using namespace Ambiesoft::stdosd;

std::wstring getIniPath()
{
	std::wstring modulePath = stdGetModuleFileName();
	std::wstring iniPath = stdCombinePath(
		stdGetParentDirectory(modulePath).c_str(),
		L"iiv.ini");
	return iniPath;
}
bool CSettngs::loadSettings()
{
	try
	{
		Profile::CHashIni ini(Profile::ReadAll(getIniPath()));

		std::string sVal;
		Profile::GetString(SECTION_OPTION, KEY_VIEWER, "", sVal, ini);
		viewerPath_ = toStdWstringFromUtf8(sVal);

		return true;
	}
	catch (std::exception& e)
	{
		MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
		return false;
	}
}
