/* Copyright 1996-2015 Arch D. Robison 

   Licensed under the Apache License, Version 2.0 (the "License"); 
   you may not use this file except in compliance with the License. 
   You may obtain a copy of the License at 
   
       http://www.apache.org/licenses/LICENSE-2.0 
       
   Unless required by applicable law or agreed to in writing, software 
   distributed under the License is distributed on an "AS IS" BASIS, 
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
   See the License for the specific language governing permissions and 
   limitations under the License.
 */

/******************************************************************************
 OS specific services buit on DirectX
*******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS 1

#undef UNICODE

#define _WIN32_WINNT 0x501 /* Necessary to get WM_MOUSELEAVE and SHGetFolderPath */

#define HAVE_SOUND 1

#include <d3d9.h>       // DirectX 9 includes
#include <CommDlg.h>    // Common dialogs
#include <Ole2.h>
#include <gdiplus.h>
#include <ShlObj.h>
#include <Shellapi.h>
#include <stdio.h>
#include <Commdlg.h>
#include <cderr.h>
#include "AssertLib.h"
#include "BuiltFromResource.h"
#include "Config.h"
#include "Game.h"
#include "Host.h"
#include "NimbleDraw.h"
#include "TraceLib.h"

#define WINDOW_CLASS_NAME "WINXCLASS"   // Class name

//! True iff running in exclusive screen mode
/** Use ExclusiveMode=false for interactive debugging. */

#ifdef EXCLUSIVE_MODE
const bool ExclusiveMode = EXCLUSIVE_MODE;
#elif ASSERTIONS
const bool ExclusiveMode = false;
#else
const bool ExclusiveMode = false;
#endif

static int DisplayWidth;
static int DisplayHeight;

#if SOUND_ONLY
static short WindowWidth = ExclusiveMode ? 1024 : 256;
static short WindowHeight = ExclusiveMode ? 768 : 192;
#else
// Voromoeba "Help" overlay dictates minimum display size
static short WindowWidth = ExclusiveMode ? 1024 : 1024;
static short WindowHeight = ExclusiveMode ? 768 : 1024;
#endif

//! Set to true between time that a WM_SIZE message is received and GameResizeOrMove is called.
static bool ResizeOrMove = false;   

#if CREATE_LOG
FILE* LogFile;
#endif /* CREATE_LOG */

//! Handle for main window
static HWND MainWindowHandle;   
static HINSTANCE MainHinstance;

// The DirectDraw object from which the device is created.
static IDirect3D9* d3d = NULL;

// Device upon which to draw
static IDirect3DDevice9* Device = NULL;

//! Return the greater of i rounded to a multiple of four or 128;
/** Rounds down in the case of a number that is of the form 4k+2 */
static int RoundAndClip( int i ) {
    int j = (i+1)>>2<<2;
    if( j<128 ) j=128;
    return j;
}

//! Decode error code from DirectX (only used during debugging)
static const char* Decode( HRESULT hr ) {
    switch( hr ) {
        case D3DERR_DEVICELOST:
            return "D3DERR_DEVICELOST";
        case D3DERR_INVALIDCALL:
            return "D3DERR_INVALIDCALL";
        case D3DERR_NOTAVAILABLE:
            return "D3DERR_NOTAVAILABLE";
        case D3DERR_OUTOFVIDEOMEMORY:
            return "D3DERR_OUTOFVIDEOMEMORY";
        default:
            return "<unknown>";
    }
}

void HostExit() {
    PostMessage(MainWindowHandle, WM_DESTROY,0,0);
}

static bool RequestedTrackMouseEvent = false;

static LRESULT CALLBACK WindowProc(HWND hwnd,
                            UINT msg, 
                            WPARAM wparam, 
                            LPARAM lparam)
{                       
    // this is the main message handler of the system
    PAINTSTRUCT ps;        // used in WM_PAINT
    HDC         hdc;       // handle to a device context

    // what is the message
    switch(msg) {
#if !SOUND_ONLY
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            NimblePoint p( (int)LOWORD(lparam), (int)HIWORD(lparam) );
            if( msg==WM_LBUTTONDOWN )
                GameMouseButtonDown( p, 0 );
            else
                GameMouseButtonUp( p, 0 );
            break;
        }
        case WM_MOUSELEAVE:
            RequestedTrackMouseEvent = false;
            HostShowCursor(true);
            break;

        case WM_MOUSEMOVE: {

            if (!RequestedTrackMouseEvent) {
                // First time mouse entered my window:
                // Request leave notification
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.hwndTrack = hwnd;
                tme.dwFlags = TME_LEAVE;
                TrackMouseEvent(&tme);
                RequestedTrackMouseEvent = true;
            }

            NimblePoint p( (int)LOWORD(lparam), (int)HIWORD(lparam) );
            GameMouseMove( p );
            break;
        }
#endif /* !SOUND_ONLY */
        case WM_CREATE:
            // Do initialization stuff here
            return 0;

        case WM_PAINT:
            // Start painting
            hdc = BeginPaint(hwnd,&ps);

            // End painting
            EndPaint(hwnd,&ps);
            return 0;
        
        case WM_MOVE:
            return 0;
        
        case WM_SIZE:
            // Extract new window size
            // Round to multiple of four, because some of our loop unrollings depend upon such.
            WindowWidth = RoundAndClip(LOWORD(lparam));
            WindowHeight = HIWORD(lparam);
            ResizeOrMove = true;
            return 0;
        
        case WM_COMMAND:
            break;

        case WM_DESTROY:
            // Kill the application
            PostQuitMessage(0);
            return 0;

        case WM_CHAR: 
            GameKeyDown(wparam);
            return 0;

#if HAVE_DRAG_DROP
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wparam;
            POINT point;
            if( DragQueryPoint(hDrop,&point) ) {
                NimblePoint p;
                p.x = point.x;
                p.y = point.y;
                UINT n = DragQueryFile(hDrop, ~0U, nullptr, 0u);
                for(unsigned i=0; i<n; ++i) {
                    char buffer[MAX_PATH+2];
                    UINT m = DragQueryFile(hDrop, i, buffer, sizeof(buffer));
                    if( m<=MAX_PATH ) {
                        GameDroppedFile( p, buffer );
                    }
                }
            }
            DragFinish(hDrop);
            return 0;
        }
#endif
        default:
            break;

    } // end switch

    // Process any messages that we didn't take care of
    return (DefWindowProc(hwnd, msg, wparam, lparam));

} // end WinProc

static void ProductionAssert( bool predicate, const char * msg ) {
    if( !predicate ) {
        char* lpMsgBuf;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            GetLastError(),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
            (char*)&lpMsgBuf,
            0,
            NULL
            );
        // Display the string.
        MessageBoxA( NULL, msg, lpMsgBuf, MB_OK | MB_ICONINFORMATION );
        // Free the buffer.
        LocalFree( lpMsgBuf );
        exit(1);
    }
}

struct WindowsResource {
    WindowsResource( const char* name, const char* type );
    const void* ptr;
    DWORD size;
};

WindowsResource::WindowsResource( const char* name, const char* type ) {
    HRSRC x = ::FindResourceA(  NULL, name, type );
    ProductionAssert(x,name);
    size = SizeofResource( NULL, x); 
    Assert(size);
    HGLOBAL g = ::LoadResource( NULL, x );
    Assert(g);
    ptr = LockResource(g);
    Assert(ptr!=NULL);
}

void HostLoadResource( BuiltFromResourceWaveform& item ) {
    WindowsResource r(item.resourceName(), "WAV" );
    item.buildFrom( (const char*)r.ptr, r.size );
}

#if !SOUND_ONLY
void HostLoadResource( BuiltFromResourcePixMap& item ) {
    WindowsResource r( item.resourceName(), "PNG" );
    HGLOBAL buf = ::GlobalAlloc(GMEM_MOVEABLE, r.size);
    Assert(buf);
    char* png = (char*)::GlobalLock(buf);
    Assert(png);
    memcpy(png,r.ptr,r.size);
    // Following assertion check that it is a PNG resource.
    Assert(memcmp(png+1,"PNG",3)==0 );
    IStream* s = NULL;
    HRESULT streamCreationStatus = CreateStreamOnHGlobal(buf,FALSE,&s);
    Assert( streamCreationStatus==S_OK );
    Assert(s);
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromStream(s,FALSE);
    ProductionAssert(bitmap,"Gdiplus::Bitmap::FromStream returned false");
    Gdiplus::Status fromStreamStatus = bitmap->GetLastStatus();
    ProductionAssert(fromStreamStatus==Gdiplus::Ok,"Gdiplus::Bitmap::FromStream failed");
    s->Release();
    ::GlobalUnlock(buf);
    ::GlobalFree(buf);

    int w=bitmap->GetWidth();
    int h=bitmap->GetHeight();
    const Gdiplus::Rect rect(0,0,w,h);
    Gdiplus::BitmapData lockedBits;
    Gdiplus::Status lockStatus = bitmap->LockBits(&rect,0,PixelFormat32bppARGB,&lockedBits);
    Assert( lockStatus==Gdiplus::Ok);
    NimblePixMap map(w,h,8*sizeof(NimblePixel),lockedBits.Scan0,lockedBits.Stride);
    item.buildFrom(map);
    Gdiplus::Status unlockStatus = bitmap->UnlockBits(&lockedBits);
    delete bitmap;
    Assert(unlockStatus==Gdiplus::Ok);
    return;
}

static void LoadResourceBitmaps(HINSTANCE hInstance) {
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    GameInitialize();
    Gdiplus::GdiplusShutdown(gdiplusToken);
}
#endif /* !SOUND_ONLY */

#include <float.h>

//! Initialize floating-point control state.
/** Disables floating point exceptions */
static void InitializeFloatingPoint(void)
{
    _control87(MCW_EM,MCW_EM);
}

//! Initially recorded time from QueryPerformanceCounter
/** This value is subtracted from subsequent calls to QueryPerformanceCounter,
    in order to increase the number of signficicant bits returned by HostClockTime(). */
static LARGE_INTEGER ClockBase;

static LARGE_INTEGER ClockFreq;

static void InitializeClock() {
    BOOL countStatus = QueryPerformanceCounter(&ClockBase); 
    Assert(countStatus);
    BOOL freqStatus = QueryPerformanceFrequency(&ClockFreq);  
    Assert(freqStatus);
};

double HostClockTime() {
    LARGE_INTEGER count;
    BOOL counterStatus = QueryPerformanceCounter(&count); 
    Assert(counterStatus);
    return double(count.QuadPart-ClockBase.QuadPart)/ClockFreq.QuadPart; 
}

bool HostIsKeyDown( int key ) {
    switch( key ) {
        case HOST_KEY_LEFT:  key=VK_LEFT;  break;
        case HOST_KEY_RIGHT: key=VK_RIGHT; break;
        case HOST_KEY_UP:    key=VK_UP;    break;
        case HOST_KEY_DOWN:  key=VK_DOWN;  break;
        default:
            if( 'a'<=key && key<='z' ) 
                key=toupper(key);
            break;
    }
    return GetKeyState(key)<0;
}

static void SetPresentParameters( D3DPRESENT_PARAMETERS& present, bool exclusiveMode=ExclusiveMode ) {
    ZeroMemory( &present, sizeof(D3DPRESENT_PARAMETERS) );
    present.BackBufferCount           = 2;
    present.SwapEffect                = D3DSWAPEFFECT_DISCARD;
    present.BackBufferFormat          = D3DFMT_X8R8G8B8;
    present.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    present.Windowed                  = !exclusiveMode;

    if( exclusiveMode ) {
        present.BackBufferHeight = DisplayHeight;
        present.BackBufferWidth = DisplayWidth;
    }
}

void HostSetFrameIntervalRate( int limit ) {
    D3DPRESENT_PARAMETERS present;
    SetPresentParameters(present);
    present.PresentationInterval = limit==0 ? D3DPRESENT_INTERVAL_IMMEDIATE : 
                                   limit==1 ? D3DPRESENT_INTERVAL_DEFAULT : 
                                              D3DPRESENT_INTERVAL_TWO;
    Device->Reset(&present);
}

class StringBuilder {
    char* ptr;
public:
    StringBuilder( char * dst ) : ptr(dst) {}
    StringBuilder& append( const char* src, bool skipnull=false );
};

StringBuilder& StringBuilder::append(const char* src, bool skipnull) {
    while( *ptr = *src++ )
        ++ptr;
    if( skipnull ) 
        ++ptr;
    return *this;
}

std::string HostGetFileName(GetFileNameOp op, const char* fileType, const char* fileSuffix) {
    std::string result;
    char buffer[MAX_PATH+1] = "";
    OPENFILENAME ofn;     
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = MainWindowHandle;
    ofn.nMaxFile = sizeof(buffer);
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY;
    ofn.lpstrFile = buffer;
    char filter[128+5];                                 //+5 is for 3 nulls and "*."
    Assert(strlen(fileType)+strlen(fileSuffix)+3 <= sizeof(filter));
    StringBuilder(filter).append(fileType,true).append("*.").append(fileSuffix,true).append(""); 
    ofn.lpstrFilter = filter;
    switch(op) {
        default: 
            Assert(false);  // Illegal
            break;
        case GetFileNameOp::open:
            ofn.lpstrTitle = "Open";
            ofn.Flags |= OFN_FILEMUSTEXIST;
            if( GetOpenFileName(&ofn) ) {
                result = buffer;
            }
            break;
        case GetFileNameOp::create:
        case GetFileNameOp::saveAs:
            ofn.lpstrTitle = op==GetFileNameOp::create ? "Create" : nullptr;
            if( GetSaveFileName(&ofn) ) {
                result = buffer;
            } else {
#if 0
                // Sometimes helpfule for debugging.
                DWORD x = CommDlgExtendedError();
                switch(x) {
                    case CDERR_DIALOGFAILURE:
                        break;
                    case CDERR_INITIALIZATION:
                        break;
                }
#endif
            }
            break;
    }
    return result;
}

static bool InitializeDirectX( HWND hwnd ) {
    HRESULT hr;

    // Create the Direct3D interface object.
    d3d = Direct3DCreate9( D3D_SDK_VERSION );
    Assert(d3d);

    // Get device capabilities
    D3DCAPS9 displayCaps;
    hr = d3d->GetDeviceCaps(0,D3DDEVTYPE_HAL,&displayCaps);
    Assert(hr==D3D_OK);

    // Get resolution of current display mode
    D3DDISPLAYMODE displayMode;
    hr = d3d->GetAdapterDisplayMode(0,&displayMode);
    Assert(hr==D3D_OK);
    DisplayHeight = displayMode.Height;
    DisplayWidth = displayMode.Width;
    if( DisplayHeight>DISPLAY_HEIGHT_MAX || DisplayWidth>DISPLAY_WIDTH_MAX ) {
        char buffer[200];
        sprintf_s(buffer,sizeof(buffer),
                  "Display is too big (%dx%d).\n"
                  "Please set your display to a 32-bit color mode no bigger than %dx%d.\n"
                  "Then restart Voromoeba.\n",
                  DisplayWidth,DisplayHeight,DISPLAY_WIDTH_MAX,DISPLAY_HEIGHT_MAX);
        MessageBoxA(hwnd,buffer,NULL,MB_OK|MB_ICONWARNING);
        return false;
    }

    if( DisplayHeight<DISPLAY_HEIGHT_MIN || DisplayWidth<DISPLAY_WIDTH_MIN ) {
        char buffer[200];
        sprintf_s(buffer,sizeof(buffer),
                  "Display is too small (%dx%d).\n"
                  "Please set your display to a 32-bit color mode no smaller than %dx%d.\n"
                  "Then restart Voromoeba.\n",
                  DisplayWidth,DisplayHeight,DISPLAY_WIDTH_MIN,DISPLAY_HEIGHT_MIN);
        MessageBoxA(hwnd,buffer,NULL,MB_OK|MB_ICONWARNING);
        return false;
    }

    // Create the device
    D3DPRESENT_PARAMETERS present;
    SetPresentParameters(present);
    hr = d3d->CreateDevice( 
        D3DADAPTER_DEFAULT, 
        D3DDEVTYPE_HAL, 
        hwnd, 
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &present,
        &Device
    );
    if ( hr!=D3D_OK  ) {
        char buffer[200];
        sprintf_s(buffer,200,"Internal error: d3d->CreateDevice returned %s\n",Decode(hr));
        MessageBoxA(hwnd,buffer,NULL,MB_OK|MB_ICONWARNING);
        return false;
    }

    return true;
}

//! Release all resources allocated to DirectX
static void ShutdownDirectX() {
    if( Device ) 
        Device->Release();
    if( d3d )
        d3d->Release();
} 

static void UpdateAndDraw( HWND hwnd ) {
    HRESULT hr;
    Device->BeginScene();
    IDirect3DSurface9* backBuffer=0;
    hr = Device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&backBuffer);
    Assert(hr==D3D_OK);
    D3DSURFACE_DESC desc;

    hr = backBuffer->GetDesc(&desc);
    Assert(hr==D3D_OK);
    D3DLOCKED_RECT lockedRect;
    hr = backBuffer->LockRect(&lockedRect,NULL,D3DLOCK_NOSYSLOCK|D3DLOCK_DISCARD);
    Assert(hr==D3D_OK);
    NimblePixMap pixmap(desc.Width,desc.Height,32,lockedRect.pBits,lockedRect.Pitch);
    if( ResizeOrMove ) {
        GameResizeOrMove( pixmap );
        ResizeOrMove = false;
    }
    GameUpdateDraw( pixmap, NimbleUpdate|NimbleDraw );
    hr = backBuffer->UnlockRect();
    Assert(hr==D3D_OK);
    backBuffer->Release();
    Device->EndScene();
    hr = Device->Present( 0, 0, 0, 0 );
    if( hr!=D3D_OK ) {
        Decode(hr);
    }
}

void HostShowCursor( bool show ) {
    static bool CursorIsVisible = true;
    if( show!=CursorIsVisible ) {
        CursorIsVisible = show;
        ShowCursor(show);
    }
}

void HostWarning( const char* message ) {
    D3DPRESENT_PARAMETERS present;
    if( ExclusiveMode ) {
        SetPresentParameters(present,/*exclusiveMode=*/false);
        Device->Reset(&present);
    }
	MessageBoxA(MainWindowHandle,message, ExclusiveMode ? "FATAL ERROR" : "WARNING",MB_OK|MB_ICONWARNING);
    if( ExclusiveMode ) {
        // Cannot recover, because call chain may be locking a rectangle.
        exit(1);
    }
}

const char* HostGetCommonAppData( const char*pathSuffix ) {
    Assert(pathSuffix);
    static char* path;
    if( path ) {
        return path;
    } else {
        char env[MAX_PATH+1];
        HRESULT status = SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, env );
        if( status==S_OK ) {
            path = new char[strlen(env)+strlen(pathSuffix)+1];
            strcpy(path,env);
            strcat(path,pathSuffix);
        } else {
            HostWarning("SHGetFolderPathA(...CSIDL_COMMON_APPDATA...) failed");
        }
        return path;
    } 
}

static const char* OriginalCommandLine;

std::string HostGetAssociatedFileName() {
    std::string result;
    int argc;
    wchar_t** argv = CommandLineToArgvW( GetCommandLineW(), &argc );
    if( argc>1 ) {
        size_t n = wcslen(argv[1]);
        result.resize(wcslen(argv[1]));
        for(size_t i=0; i<n; ++i) {
            unsigned c = argv[1][i];
            if(c>=128) {
                // FIXME - support wide-char filenames
                result.resize(0);
                break;
            }
            result[i] = argv[1][i];
        }
    }
    return result;
}

int WINAPI WinMain( HINSTANCE hinstance,
                    HINSTANCE hprevinstance,
                    LPSTR lpcmdline,
                    int ncmdshow)
{
#if CREATE_LOG
    LogFile = fopen("log.txt","w");
    fprintf(LogFile,"enter WinMain\n");
    fflush(LogFile);
#endif /* CREATE_LOG */
    MainHinstance = hinstance;

    InitializeFloatingPoint();
    InitializeClock();
    unsigned seed = ClockBase.QuadPart % 1000000007u;
    srand( seed );

    WNDCLASS winclass;  // this will hold the class we create
    HWND     hwnd;      // generic window handle
    MSG      msg;       // generic message

    // first fill in the window class stucture
    winclass.style          = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    winclass.lpfnWndProc    = WindowProc;
    winclass.cbClsExtra     = 0;
    winclass.cbWndExtra     = 0;
    winclass.hInstance      = hinstance;
    winclass.hIcon          = LoadIcon(NULL, IDI_APPLICATION);
    winclass.hCursor        = LoadCursor(NULL, IDC_ARROW);
    winclass.hbrBackground  = (HBRUSH)GetStockObject(BLACK_BRUSH);
    winclass.lpszMenuName   = NULL;
    winclass.lpszClassName  = WINDOW_CLASS_NAME;


    // Register the window class
    if (!RegisterClass(&winclass)) {
#if CREATE_LOG
        fprintf(LogFile,"cannot register class\n");
        fclose(LogFile);
#endif /* CREATE_LOG */
        return 0;
    }
    
#if CREATE_LOG
    fprintf(LogFile,"loaded MainMenuHandle=%p GameMenuHandle=%p PracticeMenuHandle=%p\n",
            MainMenuHandle, GameMenuHandle, PracticeMenuHandle );
    fflush(LogFile);
#endif /* CREATE_LOG */

    // Create the window, note the use of WS_POPUP
    hwnd = CreateWindow(WINDOW_CLASS_NAME,                  // class
                              GameTitle(),      // title
                              WS_POPUP|(ExclusiveMode?0:WS_VISIBLE)|(ExclusiveMode?0:WS_SIZEBOX|WS_BORDER|WS_CAPTION)/*WS_OVERLAPPEDWINDOW | WS_VISIBLE*/,
                              0,                        // x
                              0,                        // y
                              WindowWidth,              // width
                              WindowHeight,             // height
                              NULL,                     // handle to parent
                              NULL,                     // handle to menu
                              hinstance,                // instance
                              NULL);                    // creation parms
    if( !hwnd ) {
#if CREATE_LOG
        fprintf(LogFile,"cannot create main window\n");
        fclose(LogFile);
#endif /* CREATE_LOG */
        return 0;
    }
#if CREATE_LOG
    fprintf(LogFile,"registered main window\n");
    fflush(LogFile);
#endif /* CREATE_LOG */
    MainWindowHandle = hwnd;
#if USE_HOST_MENU_SYSTEM
    HostSetMenuBar(0);
    UpdateMenu();
#endif
#if USE_CILK
    // If using Cilk, start run-time now, because if started later, it causes a D3DERR_DEVICELOST issue.
    cilk_for( int i=0; i<1; ++i ) {};
#endif

    // Perform DirectX intialization
    if( !InitializeDirectX(hwnd) )
        return 0;

#if HAVE_SOUND
    bool InitializeDirectSound( HWND hwnd ); 
    if( !InitializeDirectSound(hwnd) ) 
        return 0;
#endif /* HAVE_SOUND */

#if !SOUND_ONLY
    LoadResourceBitmaps(hinstance);
#endif /* SOUND_ONLY */

#if CREATE_LOG
    fprintf(LogFile,"loaded resource bitmap\n");
    fflush(LogFile);
#endif /* CREATE_LOG */

#if HAVE_DRAG_DROP
    DragAcceptFiles(hwnd,true);
#endif /* HAVE_DRAG_DROP */

    // Enter main event loop
    for(;;) {
        if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) { 
            // Test if this is a quit
            if (msg.message == WM_QUIT)
               break;
        
            // Translate any accelerator keys
            TranslateMessage(&msg);
    
            // Send the message to the window proc
            DispatchMessage(&msg);
        }
        
        // Main game processing goes here
        UpdateAndDraw(hwnd);
    }
    
    // Shutdown game and release all resources
#if HAVE_SOUND
    extern void ShutdownDirectSound();
    ShutdownDirectSound();
#endif /* HAVE_SOUND */
    ShutdownDirectX();

    // Return to Windows 
    return msg.wParam;

} // end WinMain
