﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.
#include "pch.h"

#include <MddBootstrap.h>
#include <MddBootstrapTest.h>

using namespace winrt;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Microsoft::Windows::AppLifecycle;

using namespace std::chrono;

HWND g_window = NULL;
wchar_t g_windowClass[] = L"TestWndClass"; // the main window class name

ATOM _RegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

std::wstring GetFullIdentityString()
{
    std::wstring identityString;
    WCHAR idNameBuffer[PACKAGE_FULL_NAME_MAX_LENGTH + 1];
    UINT32 idNameBufferLen = ARRAYSIZE(idNameBuffer);
    if (::GetCurrentPackageFullName(&idNameBufferLen, idNameBuffer) == ERROR_SUCCESS)
    {
        identityString = idNameBuffer;
    }

    return identityString;
}

bool HasIdentity()
{
    static bool hasIdentity = !(GetFullIdentityString()).empty();
    return hasIdentity;
}

bool NeedDynamicDependencies()
{
    return !HasIdentity();
}

HRESULT BootstrapInitialize()
{
    if (!NeedDynamicDependencies())
    {
        return S_OK;
    }

    constexpr PCWSTR c_PackageNamePrefix{ L"WindowsAppRuntime.Test.DDLM" };
    constexpr PCWSTR c_PackagePublisherId{ L"8wekyb3d8bbwe" };
    RETURN_IF_FAILED(MddBootstrapTestInitialize(c_PackageNamePrefix, c_PackagePublisherId));

    // Major.Minor version, MinVersion=0 to find any framework package for this major.minor version
    const UINT32 c_Version_MajorMinor{ 0x00040001 };
    const PACKAGE_VERSION minVersion{};
    RETURN_IF_FAILED(MddBootstrapInitialize(c_Version_MajorMinor, nullptr, minVersion));

    return S_OK;
}

void BootstrapShutdown()
{
    if (!NeedDynamicDependencies())
    {
        return;
    }

    MddBootstrapShutdown();
}

IAsyncAction WaitForActivations()
{
    co_await 120000s;
    co_return;
}

void OnActivated(const winrt::Windows::Foundation::IInspectable&, const AppActivationArguments& args)
{
    auto launchArgs = args.Data().as<ILaunchActivatedEventArgs>();
    wprintf(L"Activated via redirection with args: %s\n", launchArgs.Arguments().c_str());
    SetForegroundWindow(g_window);
}

int main()
{
    init_apartment();

    THROW_IF_FAILED(BootstrapInitialize());

    AppInstance::Activated_revoker token;

    AppInstance keyInstance = AppInstance::FindOrRegisterForKey(L"derp.txt");
    if (!keyInstance.IsCurrent())
    {
        keyInstance.RedirectActivationToAsync(AppInstance::GetCurrent().GetActivatedEventArgs()).get();
    }
    else
    {
        AppInstance thisInstance = AppInstance::GetCurrent();
        token = thisInstance.Activated(
            auto_revoke, [&thisInstance](
                const auto& sender, const AppActivationArguments& args)
            { OnActivated(sender, args); }
        );

        auto hInstance = GetModuleHandle(NULL);
        _RegisterClass(hInstance);

        // Perform application initialization:
        if (!InitInstance(hInstance, SW_SHOWDEFAULT))
        {
            return 1;
        }

        // Main message loop:
        MSG msg;
        BOOL msgRet;
        while ((msgRet = GetMessage(&msg, NULL, 0, 0)) != 0)
        {
            if (msgRet == -1)
            {
                return (int)GetLastError();
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    BootstrapShutdown();
    return 0;
}

ATOM _RegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = {};

    wcex.cbSize = sizeof(wcex);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = g_windowClass;

    return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    g_window = CreateWindow(g_windowClass, L"ContainerWindowingTestApp", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!g_window)
    {
        return FALSE;
    }

    ShowWindow(g_window, nCmdShow);
    UpdateWindow(g_window);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
