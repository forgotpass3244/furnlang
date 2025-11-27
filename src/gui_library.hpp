// Gui.hpp
#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <variant>

// MapId type alias as you described
using MapId = unsigned long long;

inline std::wstring ToWString(const std::string &Str)
{
    int Size = ::MultiByteToWideChar(CP_UTF8, 0, Str.c_str(), -1, nullptr, 0);
    if (Size <= 0)
        return std::wstring();
    std::wstring WStr(Size, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, Str.c_str(), -1, &WStr[0], Size);
    if (!WStr.empty())
        WStr.pop_back();
    return WStr;
}

class Gui
{
public:
    Gui() = default;
    explicit Gui(const std::wstring &Title, int Width, int Height);
    ~Gui();

    void Run();
    void Exit();
    bool CheckIsFocused() const;
    void DrawSpritesNow(bool Yield = false);

    // Thread-safe sprite accessors
    void SetSprites(const std::vector<Value> &NewSprites);
    std::vector<Value> GetSprites() const;

    // Optional: set initial window title/size before Run
    void SetWindowTitle(const std::wstring &Title);
    void SetWindowSize(int Width, int Height);

private:
    // Win32 objects
    HWND WindowHandle = nullptr;
    std::wstring WindowTitle = L"FurnGui";
    int Width = 800;
    int Height = 600;
    std::thread GuiThread;

    // Backbuffer
    HDC MemDC = nullptr;
    HBITMAP BufferBitmap = nullptr;
    HBITMAP OldBitmap = nullptr;
    int BufferW = 0;
    int BufferH = 0;

    // Runtime state
    std::atomic<bool> Running{false};
    mutable std::mutex SpritesMutex;
    std::vector<Value> Sprites;

    // Bitmap cache keyed by ClassObject.Id (MapId)
    std::unordered_map<MapId, HBITMAP> BitmapCache;
    std::mutex BitmapCacheMutex;

    short CurrentRenderQuality = 4;

    // --- Win32 helpers ---
    static LRESULT CALLBACK WndProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
    void RegisterWindowClass(HINSTANCE Instance);
    void MessageLoop();

    // --- Painting / drawing ---
    LRESULT OnPaint(HWND Hwnd);
    void DrawSingleSprite(HDC Hdc, ClassObject &Object);
    void TryLoadBitmapForObject(ClassObject &Object);
    void SetBitmapQuality(HDC Hdc);

    // --- Mouse handling ---
    void OnMouseDown(int X, int Y, short MouseButton);
    void OnMouseUp(int X, int Y, short MouseButton);

    // --- Utilities ---
    HBITMAP GetCachedBitmap(MapId Id) const;
    void SetCachedBitmap(MapId Id, HBITMAP Hbmp);

    // Disallow copying to avoid thread/DC mess
    Gui(const Gui &) = delete;
    Gui &operator=(const Gui &) = delete;
};

// ---------------- Implementation ----------------

inline Gui::Gui(const std::wstring &Title, int Width_, int Height_)
    : WindowTitle(Title), Width(Width_), Height(Height_)
{
}

inline Gui::~Gui()
{
    Exit();
}

inline void Gui::SetWindowTitle(const std::wstring &Title)
{
    WindowTitle = Title;
    if (WindowHandle)
    {
        ::SetWindowTextW(WindowHandle, WindowTitle.c_str());
    }
}

inline void Gui::SetWindowSize(int Width_, int Height_)
{
    Width = Width_;
    Height = Height_;
    if (WindowHandle)
    {
        ::SetWindowPos(WindowHandle, nullptr, 0, 0, Width, Height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

inline void Gui::Run()
{
    if (Running)
        return;
    Running = true;

    GuiThread = std::thread([this]()
                            {
        HINSTANCE HInst = ::GetModuleHandleW(nullptr);
        RegisterWindowClass(HInst);

        WindowHandle = ::CreateWindowExW(
            0,
            L"FurnGuiClass",
            WindowTitle.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            Width, Height,
            nullptr, nullptr, HInst, this);

        if (!WindowHandle)
        {
            ::MessageBoxW(nullptr, L"Failed to create GUI window.", L"Error", MB_OK | MB_ICONERROR);
            Running = false;
            return;
        }

        ::ShowWindow(WindowHandle, SW_SHOW);
        ::UpdateWindow(WindowHandle);

        MessageLoop(); });
}

inline void Gui::Exit()
{
    if (!Running)
        return;
    Running = false;

    if (WindowHandle)
    {
        ::PostMessageW(WindowHandle, WM_CLOSE, 0, 0);
    }

    if (GuiThread.joinable())
        GuiThread.join();

    // Release DC resources
    if (MemDC)
    {
        if (BufferBitmap)
        {
            ::SelectObject(MemDC, OldBitmap);
            ::DeleteObject(BufferBitmap);
            BufferBitmap = nullptr;
        }
        ::DeleteDC(MemDC);
        MemDC = nullptr;
    }

    // Release cached bitmaps
    {
        std::lock_guard<std::mutex> Lock(BitmapCacheMutex);
        for (auto &Pair : BitmapCache)
        {
            if (Pair.second)
            {
                ::DeleteObject(Pair.second);
            }
        }
        BitmapCache.clear();
    }
}

inline bool Gui::CheckIsFocused() const
{
    return ::GetForegroundWindow() == WindowHandle;
}

inline void Gui::DrawSpritesNow(bool Yield)
{
    if (!WindowHandle)
        return;
    ::InvalidateRect(WindowHandle, nullptr, FALSE);
    if (Yield)
        ::UpdateWindow(WindowHandle);
}

inline void Gui::SetSprites(const std::vector<Value> &NewSprites)
{
    std::lock_guard<std::mutex> Lock(SpritesMutex);
    Sprites = NewSprites;
}

inline std::vector<Value> Gui::GetSprites() const
{
    std::lock_guard<std::mutex> Lock(SpritesMutex);
    return Sprites;
}

inline void Gui::RegisterWindowClass(HINSTANCE Instance)
{
    WNDCLASSW Wc = {};
    Wc.lpfnWndProc = Gui::WndProc;
    Wc.hInstance = Instance;
    Wc.lpszClassName = L"FurnGuiClass";
    Wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    Wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    ::RegisterClassW(&Wc);
}

inline void Gui::MessageLoop()
{
    MSG Msg;
    while (Running && ::GetMessageW(&Msg, nullptr, 0, 0))
    {
        ::TranslateMessage(&Msg);
        ::DispatchMessageW(&Msg);
    }
}

inline LRESULT CALLBACK Gui::WndProc(HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    Gui *Self = nullptr;
    if (Msg == WM_NCCREATE)
    {
        auto *Cs = reinterpret_cast<CREATESTRUCT *>(LParam);
        Self = static_cast<Gui *>(Cs->lpCreateParams);
        ::SetWindowLongPtrW(Hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Self));
        Self->WindowHandle = Hwnd;
    }
    else
    {
        Self = reinterpret_cast<Gui *>(::GetWindowLongPtrW(Hwnd, GWLP_USERDATA));
    }

    if (!Self)
        return ::DefWindowProcW(Hwnd, Msg, WParam, LParam);

    switch (Msg)
    {
    case WM_PAINT:
        return Self->OnPaint(Hwnd);
    case WM_DESTROY:
        ::PostQuitMessage(0);
        Self->Running = false;
        return 0;
    case WM_LBUTTONDOWN:
        Self->OnMouseDown(LOWORD(LParam), HIWORD(LParam), 1);
        return 0;
    case WM_RBUTTONDOWN:
        Self->OnMouseDown(LOWORD(LParam), HIWORD(LParam), 2);
        return 0;
    case WM_MBUTTONDOWN:
        Self->OnMouseDown(LOWORD(LParam), HIWORD(LParam), 3);
        return 0;
    case WM_LBUTTONUP:
        Self->OnMouseUp(LOWORD(LParam), HIWORD(LParam), 1);
        return 0;
    case WM_RBUTTONUP:
        Self->OnMouseUp(LOWORD(LParam), HIWORD(LParam), 2);
        return 0;
    case WM_MBUTTONUP:
        Self->OnMouseUp(LOWORD(LParam), HIWORD(LParam), 3);
        return 0;
    default:
        break;
    }

    return ::DefWindowProcW(Hwnd, Msg, WParam, LParam);
}

inline LRESULT Gui::OnPaint(HWND Hwnd)
{
    PAINTSTRUCT Ps;
    HDC Hdc = ::BeginPaint(Hwnd, &Ps);

    RECT Rc;
    ::GetClientRect(Hwnd, &Rc);
    int W = Rc.right - Rc.left;
    int H = Rc.bottom - Rc.top;

    // Recreate buffer if size changed
    if (!BufferBitmap || W != BufferW || H != BufferH)
    {
        if (BufferBitmap && MemDC)
        {
            ::SelectObject(MemDC, OldBitmap);
            ::DeleteObject(BufferBitmap);
            BufferBitmap = nullptr;
        }
        if (MemDC)
        {
            ::DeleteDC(MemDC);
            MemDC = nullptr;
        }

        HDC TempDC = ::GetDC(Hwnd);
        MemDC = ::CreateCompatibleDC(TempDC);
        BufferBitmap = ::CreateCompatibleBitmap(TempDC, W, H);
        OldBitmap = static_cast<HBITMAP>(::SelectObject(MemDC, BufferBitmap));
        ::ReleaseDC(Hwnd, TempDC);

        BufferW = W;
        BufferH = H;
    }

    // Clear backbuffer and set quality
    ::FillRect(MemDC, &Rc, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
    SetBitmapQuality(MemDC);

    // Draw sprites (snapshot under lock)
    std::vector<Value> LocalSprites;
    {
        std::lock_guard<std::mutex> Lock(SpritesMutex);
        LocalSprites = Sprites;
    }

    for (auto &Val : LocalSprites)
    {
        // Expect ClassObject stored inside Value variant
        if (auto *ObjPtr = std::get_if<ClassObject>(&Val))
        {
            try
            {
                DrawSingleSprite(MemDC, *ObjPtr);
            }
            catch (...)
            {
                // skip invalid sprite quietly
                continue;
            }
        }
    }

    ::BitBlt(Hdc, 0, 0, W, H, MemDC, 0, 0, SRCCOPY);
    ::EndPaint(Hwnd, &Ps);
    return 0;
}

inline HBITMAP Gui::GetCachedBitmap(MapId Id) const
{
    std::lock_guard<std::mutex> Lock(BitmapCacheMutex);
    auto It = BitmapCache.find(Id);
    if (It != BitmapCache.end())
        return It->second;
    return nullptr;
}

inline void Gui::SetCachedBitmap(MapId Id, HBITMAP Hbmp)
{
    std::lock_guard<std::mutex> Lock(BitmapCacheMutex);
    // If replacing an existing bitmap, delete old handle
    auto It = BitmapCache.find(Id);
    if (It != BitmapCache.end())
    {
        if (It->second && It->second != Hbmp)
            ::DeleteObject(It->second);
        It->second = Hbmp;
    }
    else
    {
        BitmapCache.emplace(Id, Hbmp);
    }
}

inline void Gui::TryLoadBitmapForObject(ClassObject &Object)
{
    if (!Object.Members)
        return;
    if (!Object.Members->HasKey("Image"))
        return;

    try
    {
        Value ImgVal = Object.Members->Get("Image");
        if (auto *StrPtr = std::get_if<std::string>(&ImgVal))
        {
            std::wstring Path = ToWString(*StrPtr);
            HBITMAP Hbmp = static_cast<HBITMAP>(
                ::LoadImageW(nullptr, Path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
            if (Hbmp)
            {
                SetCachedBitmap(Object.Members->Id, Hbmp);
            }
        }
    }
    catch (...)
    {
        // ignore
    }
}

inline void Gui::DrawSingleSprite(HDC Hdc, ClassObject &Object)
{
    if (!Object.Members)
        return;

    // Visible check
    if (Object.Members->HasKey("Visible"))
    {
        Value VisibleVal = Object.Members->Get("Visible");
        if (auto *BoolPtr = std::get_if<bool>(&VisibleVal))
        {
            if (!(*BoolPtr))
                return;
        }
    }

    // Position extraction: support PositionX/PositionY or Position { X, Y }
    int PosX = 0;
    int PosY = 0;

    if (Object.Members->HasKey("PositionX") && Object.Members->HasKey("PositionY"))
    {
        Value Px = Object.Members->Get("PositionX");
        Value Py = Object.Members->Get("PositionY");
        if (auto *Pxl = std::get_if<long>(&Px))
            PosX = static_cast<int>(*Pxl);
        else if (auto *Pxi = std::get_if<int>(&Px))
            PosX = *Pxi;
        if (auto *Pyl = std::get_if<long>(&Py))
            PosY = static_cast<int>(*Pyl);
        else if (auto *Pyi = std::get_if<int>(&Py))
            PosY = *Pyi;
    }
    else if (Object.Members->HasKey("Position"))
    {
        Value PosVal = Object.Members->Get("Position");
        if (auto *PosObj = std::get_if<ClassObject>(&PosVal))
        {
            if (PosObj->Members->HasKey("X"))
            {
                Value XVal = PosObj->Members->Get("X");
                if (auto *Xl = std::get_if<long>(&XVal))
                    PosX = static_cast<int>(*Xl);
                else if (auto *Xi = std::get_if<int>(&XVal))
                    PosX = *Xi;
            }
            if (PosObj->Members->HasKey("Y"))
            {
                Value YVal = PosObj->Members->Get("Y");
                if (auto *Yl = std::get_if<long>(&YVal))
                    PosY = static_cast<int>(*Yl);
                else if (auto *Yi = std::get_if<int>(&YVal))
                    PosY = *Yi;
            }
        }
    }

    // Scale
    double ScaleX = 1.0;
    double ScaleY = 1.0;
    if (Object.Members->HasKey("ScaleX"))
    {
        Value Sx = Object.Members->Get("ScaleX");
        if (auto *Sd = std::get_if<double>(&Sx))
            ScaleX = *Sd;
        else if (auto *Sf = std::get_if<float>(&Sx))
            ScaleX = static_cast<double>(*Sf);
    }
    if (Object.Members->HasKey("ScaleY"))
    {
        Value Sy = Object.Members->Get("ScaleY");
        if (auto *Sd = std::get_if<double>(&Sy))
            ScaleY = *Sd;
        else if (auto *Sf = std::get_if<float>(&Sy))
            ScaleY = static_cast<double>(*Sf);
    }

    // Get or load bitmap from cache
    HBITMAP Hbmp = GetCachedBitmap(Object.Members->Id);
    if (!Hbmp)
    {
        TryLoadBitmapForObject(Object);
        Hbmp = GetCachedBitmap(Object.Members->Id);
    }

    if (!Hbmp)
    {
        // Placeholder rect + optional debug text
        RECT R = {PosX, PosY, PosX + static_cast<int>(100 * ScaleX), PosY + static_cast<int>(100 * ScaleY)};
        ::FillRect(Hdc, &R, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 2));
        if (Object.Members->HasKey("Image"))
        {
            Value ImgVal = Object.Members->Get("Image");
            if (auto *S = std::get_if<std::string>(&ImgVal))
            {
                std::cerr << "Gui: Missing bitmap '" << *S << "' for object " << Object.Members->Id << "\n";
            }
        }
        return;
    }

    // Draw the bitmap
    HDC Mem = ::CreateCompatibleDC(Hdc);
    HBITMAP Old = static_cast<HBITMAP>(::SelectObject(Mem, Hbmp));
    BITMAP Bmp;
    ::GetObjectW(Hbmp, sizeof(BITMAP), &Bmp);
    int DrawW = static_cast<int>(Bmp.bmWidth * ScaleX);
    int DrawH = static_cast<int>(Bmp.bmHeight * ScaleY);
    ::StretchBlt(Hdc, PosX, PosY, DrawW, DrawH, Mem, 0, 0, Bmp.bmWidth, Bmp.bmHeight, SRCCOPY);
    ::SelectObject(Mem, Old);
    ::DeleteDC(Mem);
}

inline void Gui::SetBitmapQuality(HDC Hdc)
{
    switch (CurrentRenderQuality)
    {
    case 4:
        ::SetStretchBltMode(Hdc, HALFTONE);
        break;
    case 3:
        ::SetStretchBltMode(Hdc, COLORONCOLOR);
        break;
    case 2:
        ::SetStretchBltMode(Hdc, BLACKONWHITE);
        break;
    case 1:
        ::SetStretchBltMode(Hdc, WHITEONBLACK);
        break;
    default:
        ::SetStretchBltMode(Hdc, HALFTONE);
        break;
    }
}

inline void Gui::OnMouseDown(int X, int Y, short MouseButton)
{
    std::vector<Value> LocalSprites;
    {
        std::lock_guard<std::mutex> Lock(SpritesMutex);
        LocalSprites = Sprites;
    }

    for (auto &Val : LocalSprites)
    {
        if (auto *ObjPtr = std::get_if<ClassObject>(&Val))
        {
            ClassObject &Object = *ObjPtr;

            if (!Object.Members)
                continue;
            if (!Object.Members->HasKey("Clickable"))
                continue;
            Value ClickVal = Object.Members->Get("Clickable");
            if (auto *Cb = std::get_if<bool>(&ClickVal))
            {
                if (!(*Cb))
                    continue;
            }
            else
                continue;

            // compute bounds
            int PosX = 0;
            int PosY = 0;
            if (Object.Members->HasKey("PositionX"))
            {
                Value Px = Object.Members->Get("PositionX");
                if (auto *Pxl = std::get_if<long>(&Px))
                    PosX = static_cast<int>(*Pxl);
                else if (auto *Pxi = std::get_if<int>(&Px))
                    PosX = *Pxi;
            }
            if (Object.Members->HasKey("PositionY"))
            {
                Value Py = Object.Members->Get("PositionY");
                if (auto *Pyl = std::get_if<long>(&Py))
                    PosY = static_cast<int>(*Pyl);
                else if (auto *Pyi = std::get_if<int>(&Py))
                    PosY = *Pyi;
            }

            int WidthRect = 100;
            int HeightRect = 100;
            HBITMAP Hbmp = GetCachedBitmap(Object.Members->Id);
            if (Hbmp)
            {
                BITMAP Bmp;
                ::GetObjectW(Hbmp, sizeof(BITMAP), &Bmp);
                WidthRect = Bmp.bmWidth;
                HeightRect = Bmp.bmHeight;
            }

            RECT Bounds = {PosX, PosY, PosX + WidthRect, PosY + HeightRect};
            if (X >= Bounds.left && X <= Bounds.right && Y >= Bounds.top && Y <= Bounds.bottom)
            {
                // Mark pressed in members
                Object.Members->Set("__pressed", Value(true));

                if (Object.Members->HasKey("MouseButtonPress"))
                {
                    Value FuncVal = Object.Members->Get("MouseButtonPress");
                    // TODO: replace BasicFunction with your actual callable type stored in Value
                    if (auto *Bf = std::get_if<BasicFunction>(&FuncVal))
                    {
                        BasicFunction FuncCopy = *Bf;
                        std::thread([FuncCopy, MouseButton]() mutable
                                    {
                            INTERPRETER Ip;
                            Ip.exec_bfunc(FuncCopy, std::vector<Value>{ Value(static_cast<long>(MouseButton)) }); })
                            .detach();
                    }
                }
            }
        }
    }
}

inline void Gui::OnMouseUp(int X, int Y, short MouseButton)
{
    std::vector<Value> LocalSprites;
    {
        std::lock_guard<std::mutex> Lock(SpritesMutex);
        LocalSprites = Sprites;
    }

    for (auto &Val : LocalSprites)
    {
        if (auto *ObjPtr = std::get_if<ClassObject>(&Val))
        {
            ClassObject &Object = *ObjPtr;

            if (!Object.Members)
                continue;
            if (!Object.Members->HasKey("Clickable"))
                continue;
            Value ClickVal = Object.Members->Get("Clickable");
            if (auto *Cb = std::get_if<bool>(&ClickVal))
            {
                if (!(*Cb))
                    continue;
            }
            else
                continue;

            bool WasPressed = false;
            if (Object.Members->HasKey("__pressed"))
            {
                Value PVal = Object.Members->Get("__pressed");
                if (auto *Pb = std::get_if<bool>(&PVal))
                    WasPressed = *Pb;
            }

            int PosX = 0;
            int PosY = 0;
            if (Object.Members->HasKey("PositionX"))
            {
                Value Px = Object.Members->Get("PositionX");
                if (auto *Pxl = std::get_if<long>(&Px))
                    PosX = static_cast<int>(*Pxl);
                else if (auto *Pxi = std::get_if<int>(&Px))
                    PosX = *Pxi;
            }
            if (Object.Members->HasKey("PositionY"))
            {
                Value Py = Object.Members->Get("PositionY");
                if (auto *Pyl = std::get_if<long>(&Py))
                    PosY = static_cast<int>(*Pyl);
                else if (auto *Pyi = std::get_if<int>(&Py))
                    PosY = *Pyi;
            }

            int WidthRect = 100;
            int HeightRect = 100;
            HBITMAP Hbmp = GetCachedBitmap(Object.Members->Id);
            if (Hbmp)
            {
                BITMAP Bmp;
                ::GetObjectW(Hbmp, sizeof(BITMAP), &Bmp);
                WidthRect = Bmp.bmWidth;
                HeightRect = Bmp.bmHeight;
            }

            RECT Bounds = {PosX, PosY, PosX + WidthRect, PosY + HeightRect};
            if (WasPressed && X >= Bounds.left && X <= Bounds.right && Y >= Bounds.top && Y <= Bounds.bottom)
            {
                if (Object.Members->HasKey("MouseButtonRelease"))
                {
                    Value FuncVal = Object.Members->Get("MouseButtonRelease");
                    // TODO: replace BasicFunction with your actual callable type stored in Value
                    if (auto *Bf = std::get_if<BasicFunction>(&FuncVal))
                    {
                        BasicFunction FuncCopy = *Bf;
                        std::thread([FuncCopy, MouseButton]() mutable
                                    {
                            INTERPRETER Ip;
                            Ip.exec_bfunc(FuncCopy, std::vector<Value>{ Value(static_cast<long>(MouseButton)) }); })
                            .detach();
                    }
                }
            }

            // Clear pressed
            Object.Members->Set("__pressed", Value(false));
        }
    }
}
