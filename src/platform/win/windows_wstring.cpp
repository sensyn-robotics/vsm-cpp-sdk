#include <ugcs/vsm/windows_wstring.h>

using namespace ugcs::vsm;

Windows_wstring::Windows_wstring(const std::string& str)
{
    if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wchar_str, MAX_WLEN) <= 0) {
        VSM_EXCEPTION(Conversion_failure,
            "Can't convert UTF-8 string to Windows wide char string.");
    }
}

LPCWSTR
Windows_wstring::Get() const
{
    return wchar_str;
}

Windows_wstring::operator LPCWSTR() const
{
    return Get();
}
