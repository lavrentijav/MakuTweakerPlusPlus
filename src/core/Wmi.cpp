#include "core/Wmi.h"
#include <comdef.h>
#include <Wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")

namespace maku::wmi {
namespace {

bool InitCom() {
    static bool once = [] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        return true;
    }();
    return once;
}

bool QueryRows(const std::wstring& wql, std::vector<std::vector<Row>>& rows,
               const std::wstring* onlyProperty) {
    rows.clear();
    InitCom();
    IWbemLocator* loc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator,
                              reinterpret_cast<void**>(&loc))))
        return false;

    IWbemServices* svc = nullptr;
    if (FAILED(loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr,
                                  nullptr, &svc))) {
        loc->Release();
        return false;
    }
    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

    IEnumWbemClassObject* enumerator = nullptr;
    if (FAILED(svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(wql.c_str()),
                              WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr,
                              &enumerator))) {
        svc->Release();
        loc->Release();
        return false;
    }

    IWbemClassObject* obj = nullptr;
    ULONG ret = 0;
    while (enumerator) {
        HRESULT hr = enumerator->Next(2000, 1, &obj, &ret);
        if (ret == 0 || FAILED(hr)) break;

        std::vector<Row> row;
        if (onlyProperty && !onlyProperty->empty()) {
            VARIANT vt;
            VariantInit(&vt);
            if (SUCCEEDED(obj->Get(onlyProperty->c_str(), 0, &vt, nullptr, nullptr))) {
                if (vt.vt == VT_BSTR && vt.bstrVal)
                    row.push_back({*onlyProperty, vt.bstrVal});
                else if (vt.vt == VT_I4)
                    row.push_back({*onlyProperty, std::to_wstring(vt.lVal)});
                else if (vt.vt == VT_UI4)
                    row.push_back({*onlyProperty, std::to_wstring(vt.ulVal)});
                else if (vt.vt == VT_I8)
                    row.push_back({*onlyProperty, std::to_wstring(vt.llVal)});
                else if (vt.vt == VT_BOOL)
                    row.push_back({*onlyProperty, vt.boolVal ? L"TRUE" : L"FALSE"});
            }
            VariantClear(&vt);
        } else {
            SAFEARRAY* names = nullptr;
            obj->GetNames(nullptr, WBEM_FLAG_ALWAYS | WBEM_FLAG_NONSYSTEM_ONLY, nullptr, &names);
            if (names) {
                long lBound{}, uBound{};
                SafeArrayGetLBound(names, 1, &lBound);
                SafeArrayGetUBound(names, 1, &uBound);
                for (long i = lBound; i <= uBound; ++i) {
                    BSTR prop{};
                    SafeArrayGetElement(names, &i, &prop);
                    VARIANT vt;
                    VariantInit(&vt);
                    if (SUCCEEDED(obj->Get(prop, 0, &vt, nullptr, nullptr))) {
                        if (vt.vt == VT_BSTR && vt.bstrVal)
                            row.push_back({prop, vt.bstrVal});
                        else if (vt.vt == VT_I4)
                            row.push_back({prop, std::to_wstring(vt.lVal)});
                        else if (vt.vt == VT_UI4)
                            row.push_back({prop, std::to_wstring(vt.ulVal)});
                        else if (vt.vt == VT_I8)
                            row.push_back({prop, std::to_wstring(vt.llVal)});
                    }
                    VariantClear(&vt);
                    SysFreeString(prop);
                }
                SafeArrayDestroy(names);
            }
        }
        if (!row.empty()) rows.push_back(std::move(row));
        obj->Release();
    }
    enumerator->Release();
    svc->Release();
    loc->Release();
    return true;
}

} // namespace

bool Query(const std::wstring& wql, std::vector<std::vector<Row>>& rows) {
    return QueryRows(wql, rows, nullptr);
}

std::wstring QueryScalar(const std::wstring& wql, const std::wstring& property) {
    std::vector<std::vector<Row>> rows;
    if (!QueryRows(wql, rows, &property) || rows.empty()) return L"";
    for (auto& r : rows[0])
        if (_wcsicmp(r.name.c_str(), property.c_str()) == 0) return r.value;
    return L"";
}

std::vector<std::wstring> QueryList(const std::wstring& wql, const std::wstring& property) {
    std::vector<std::wstring> out;
    std::vector<std::vector<Row>> rows;
    if (!QueryRows(wql, rows, &property)) return out;
    for (auto& row : rows)
        for (auto& r : row)
            if (_wcsicmp(r.name.c_str(), property.c_str()) == 0) out.push_back(r.value);
    return out;
}

} // namespace maku::wmi
