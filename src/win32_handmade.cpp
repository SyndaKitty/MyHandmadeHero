#include <windows.h>
#include <stdint.h>
#include <xinput.h>

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

// XInput dereferenced GetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return 0;
}
static x_input_get_state *XInputGetStateWrapper = XInputGetStateStub;

// XInput dereference SetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return 0;
}
static x_input_set_state *XInputSetStateWrapper = XInputSetStateStub;

static void Win32LoadXInput()
{
    HMODULE XInputLibrary = LoadLibrary("xinput1_3");
    if (XInputLibrary)
    {
        XInputGetStateWrapper = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetStateWrapper = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
    }
    // TODO(Spencer): Log if we can't get XInput
}

static char GlobalRunning;
static win32_offscreen_buffer GlobalBackbuffer;

static win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    
    return Result;
}

static void
RenderWeirdGradient(win32_offscreen_buffer Buffer, int BlueOffset, int GreenOffset)
{
    uint8_t *Row = (uint8_t *)Buffer.Memory;    
    for(int Y = 0;
        Y < Buffer.Height;
        ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0;
            X < Buffer.Width;
            ++X)
        {
            uint8_t Blue = (X + BlueOffset);
            uint8_t Green = (Y + GreenOffset);
            
            *Pixel++ = ((Green << 8) | Blue);
        }
        
        Row += Buffer.Pitch;
    }
}

static void
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    
    int BytesPerPixel = 4;
    
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapMemorySize = (Buffer->Width*Buffer->Height)*BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width*BytesPerPixel;
}

static void
Win32DisplayBufferInWindow(HDC DeviceContext,
                           int WindowWidth, int WindowHeight,
                           win32_offscreen_buffer Buffer)
{
    StretchDIBits(DeviceContext,
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer.Width, Buffer.Height,
                  Buffer.Memory,
                  &Buffer.Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

static LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{       
    LRESULT Result = 0;
    
    switch(Message)
    {
        case WM_CLOSE:{
            GlobalRunning = 0;
        } break;
        
        case WM_ACTIVATEAPP:{
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        
        case WM_DESTROY:{
            GlobalRunning = 0;
        } break;
        
        case WM_PAINT:{
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                       GlobalBackbuffer);
            EndPaint(Window, &Paint);
        } break;
        
        default:{
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    
    return Result;
}

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    Win32LoadXInput();
    WNDCLASS WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
    
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //    WindowClass.hIcon; TODO(Spencer): Add Icon
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    
    if(RegisterClassA(&WindowClass))
    {
        HWND Window =
            CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "My Handmade Hero",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        if(Window)
        {
            HDC DeviceContext = GetDC(Window);
            
            int XOffset = 0;
            int YOffset = 0;
            
            GlobalRunning = 1;
            while(GlobalRunning)
            {
                // Handle messages
                MSG Message;
                while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = 0;
                    }
                    
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }
                
                // Handle XInput
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetStateWrapper(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                        
                        bool Up            = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool Down          = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN ;
                        bool Left          = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool Right         = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool Start         = Pad->wButtons & XINPUT_GAMEPAD_START;
                        bool Back          = Pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool LeftShoulder  = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool AButton       = Pad->wButtons & XINPUT_GAMEPAD_A;
                        bool BButton       = Pad->wButtons & XINPUT_GAMEPAD_B;
                        bool XButton       = Pad->wButtons & XINPUT_GAMEPAD_X;
                        bool YButton       = Pad->wButtons & XINPUT_GAMEPAD_Y;
                        
                        int16_t StickX = Pad->sThumbLX;
                        int16_t StickY = Pad->sThumbLY;
                        
                        if (Up | Down | Left | Right | Start | Back | LeftShoulder | RightShoulder | AButton | BButton | XButton | YButton)
                        {
                            YOffset += 2;
                        }
                    }
                    else
                    {
                        // TODO(Spencer): Handle unavailable controller
                    }
                    
                    // Render
                    RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);
                    
                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                    Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                               GlobalBackbuffer);
                    
                    ++XOffset;
                }
            }
        }
    }
}