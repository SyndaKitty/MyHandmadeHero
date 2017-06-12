#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

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
    return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state *XInputGetStateWrapper = XInputGetStateStub;
#define XInputGetState XInputGetStateWrapper

// XInput dereference SetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state *XInputSetStateWrapper = XInputSetStateStub;
#define XInputSetState XInputSetStateWrapper

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static void Win32LoadXInput()
{
    HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibrary("xinput1_3.dll");
    }
    
    if (XInputLibrary)
    {
        XInputGetStateWrapper = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetStateWrapper = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
    }
    // TODO(Spencer): Log if we can't get XInput
}

static void Win32InitDSound(HWND Window, int32_t SamplesPerSecond, int32_t BufferSize)
{
    // Load the library
    HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
    
    if (DSoundLibrary){
        // Get a DirectSound object
        direct_sound_create *DirectSoundCreate = (direct_sound_create*)GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        LPDIRECTSOUND DirectSound;
        
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.channels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.nBlockAlign = WaveFormat.nChannels * WaveFormat.wBitsPerSample / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.cbSize = 0;
            
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                // Create primary buffer
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat))){
                        
                    }
                    else 
                    {
                        // TODO(Spencer): Log failure
                    }
                }
                else
                {
                    // TODO(Spencer): Log failure
                }
            }
            else
            {
                // TODO(Spencer): Log failure
            }
            
            // Create a secondary buffer
            DSBUFFERDESC BufferDescription = {};
            
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            LPDIRECTIONSOUNDBUGGER SecondBuffer;
            
            if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &SecondaryBuffer, 0)))
            {
                
            }
            else
            {
                // TODO(Spencer): Log failure
            }
            // Start
        }
        else
        {
            // TODO(Spencer): Log failure
        }
    }
    else
    {
        // TODO(Spencer): Log failure
    }
}

// Globals
static int XOffset;
static int YOffset;
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
RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    uint8_t *Row = (uint8_t *)Buffer->Memory;
    for(int Y = 0;
        Y < Buffer->Height;
        ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for(int X = 0;
            X < Buffer->Width;
            ++X)
        {
            uint8_t Blue = (X + BlueOffset);
            uint8_t Green = (Y + GreenOffset);
            
            *Pixel++ = ((Green << 8) | Blue);
        }
        
        Row += Buffer->Pitch;
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
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer,
                           HDC DeviceContext,
                           int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory,
                  &Buffer->Info,
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
        
        case WM_DESTROY:{
            GlobalRunning = 0;
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:{
            uint32_t VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown  = ((LParam & (1 << 31)) != 0);
            bool AltWasDown = ((LParam & (1 << 29)) != 0);
            if (WasDown != IsDown)
            {
                if(VKCode == 'W')
                {
                }
                else if (VKCode == 'A')
                {
                }
                else if (VKCode == 'S')
                {
                }
                else if (VKCode == 'D')
                {
                }
                else if (VKCode == 'Q')
                {
                }
                else if (VKCode == 'E')
                {
                }
                else if (VKCode == VK_UP)
                {
                }
                else if (VKCode == VK_DOWN)
                {
                }
                else if (VKCode == VK_LEFT)
                {
                }
                else if (VKCode == VK_RIGHT)
                {
                }
                else if (VKCode == VK_ESCAPE)
                {
                }
                else if (VKCode == VK_SPACE)
                {
                }
            }
            
            bool AltKeyWasDown = (LParam & (1 << 29)) != 0;
            if (VKCode == VK_F4 && AltKeyWasDown)
            {
                GlobalRunning = 0;
            }
        } break;
        case WM_PAINT:{
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
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
            
            // int XOffset = 0;
            // int YOffset = 0;
            
            Win32InitDSound(Window, 48000, 48000 * sizeof(int16) * 2);
            
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
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
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
                        
                        
                        if (AButton)
                        {
                            XOffset += StickX / 3000;
                            YOffset -= StickY / 3000;
                        }
                    }
                    else
                    {
                        // TODO(Spencer): Handle unavailable controller
                    }
                    
                }
                // Render
                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);
                
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
            }
        }
    }
}