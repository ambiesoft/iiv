#pragma once
#include <string>

class CSettngs
{
private:
	constexpr static const char* SECTION_OPTION = "option";
	constexpr static const char* KEY_VIEWER = "viewer";

	std::wstring viewerPath_;

	CSettngs() = default;

public:
	static CSettngs& getInstance()
	{
		static CSettngs instance;
		return instance;
	}
	bool loadSettings();

	std::wstring getViewer() const
	{
		return viewerPath_;
	}
};

inline CSettngs& getSettings()
{
	return CSettngs::getInstance();
}