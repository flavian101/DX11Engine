#include "dxpch.h"
#include "PlatformDialogs.h"
#include "assets/AssetCatalog.h"
#include <commdlg.h>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "comdlg32.lib")
#endif

namespace DXEngine::PlatformDialogs
{
    namespace
    {
        std::wstring Widen(const std::string& s)
        {
            if (s.empty()) return {};
            const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
            std::wstring w(static_cast<size_t>(n), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
            if (!w.empty()) w.pop_back();                       // the terminator the call counted
            return w;
        }

        std::string Narrow(const std::wstring& w)
        {
            if (w.empty()) return {};
            const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string s(static_cast<size_t>(n), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, s.data(), n, nullptr, nullptr);
            if (!s.empty()) s.pop_back();
            return s;
        }
    }

    std::string OpenModelFile()
    {
        // "Model files\0*.fbx;*.obj;...\0All files\0*.*\0\0"
        std::wstring patterns;
        for (const std::string& ext : Assets::ModelExtensions())
        {
            if (!patterns.empty()) patterns += L';';
            patterns += L"*" + Widen(ext);
        }
        std::wstring filter = L"Model files";
        filter.push_back(L'\0');
        filter += patterns;
        filter.push_back(L'\0');
        filter += L"All files";
        filter.push_back(L'\0');
        filter += L"*.*";
        filter.push_back(L'\0');

        std::vector<wchar_t> file(32768, L'\0');
        OPENFILENAMEW ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile = file.data();
        ofn.nMaxFile = static_cast<DWORD>(file.size());
        ofn.lpstrTitle = L"Import model";
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&ofn))
            return {};
        return Narrow(file.data());
    }
}
