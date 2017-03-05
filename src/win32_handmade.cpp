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

static char GlobalRunning;
static win32_offscreen_buffer GlobalBackbuffer;

win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    
    return(Result);
}

typedef DWORD WINAPI x_input_get_state(DWORD dwUserIndex,XINPUT_STATE* pState);
typedef DWORD WINAPI X_INPUT_SET_STATE(DWORDDWUSERINDEX, XINPUT_VIBRATION* PVIBRATION);

STATIC X_INPUT_GET_STATE *XINPUTGETSTATEWRAPPER;
STATIC X_INPUT_SET_STATE *XINPUTSETSTATEWRAPPER;
#DEFINE XINPUTGETSTATE XINPUTGETSTATEWRAPPER
#DEFINE XINPUTSETSTATE XINPUTSETSTATEWRAPPER

STATIC VOID
RENDERWEIRDGRADIENT(WIN32_OFFSCREEN_BUFFER BUFFER, INT BLUEOFFSET, INT GREENOFFSET)
{
    UINT8_T *ROW = (UINT8_T *)BUFFER.MEMORY;    
    FOR(INT Y = 0;
        Y < BUFFER.HEIGHT;
        ++Y)
    {
        UINT32_T *PIXEL = (UINT32_T *)ROW;
        FOR(INT X = 0;
            X < BUFFER.WIDTH;
            ++X)
        {
            UINT8_T BLUE = (X + BLUEOFFSET);
            UINT8_T GREEN = (Y + GREENOFFSET);
            
            *PIXEL++ = ((GREEN << 8) | BLUE);
        }
        
        ROW += BUFFER.PITCH;
    }
}

STATIC VOID
WIN32RESIZEDIBSECTION(WIN32_OFFSCREEN_BUFFER *BUFFER, INT WIDTH, INT HEIGHT)
{
    IF(BUFFER->MEMORY)
    {
        VIRTUALFREE(BUFFER->MEMORY, 0, MEM_RELEASE);
    }
    
    BUFFER->WIDTH = WIDTH;
    BUFFER->HEIGHT = HEIGHT;
    
    INT BYTESPERPIXEL = 4;
    
    BUFFER->INFO.BMIHEADER.BISIZE = SIZEOF(BUFFER->INFO.BMIHEADER);
    BUFFER->INFO.BMIHEADER.BIWIDTH = BUFFER->WIDTH;
    BUFFER->INFO.BMIHEADER.BIHEIGHT = -BUFFER->HEIGHT;
    BUFFER->INFO.BMIHEADER.BIPLANES = 1;
    BUFFER->INFO.BMIHEADER.BIBITCOUNT = 32;
    BUFFER->INFO.BMIHEADER.BICOMPRESSION = BI_RGB;
    
    INT BITMAPMEMORYSIZE = (BUFFER->WIDTH*BUFFER->HEIGHT)*BYTESPERPIXEL;
    BUFFER->MEMORY = VIRTUALALLOC(0, BITMAPMEMORYSIZE, MEM_COMMIT, PAGE_READWRITE);
    BUFFER->PITCH = WIDTH*BYTESPERPIXEL;
}

STATIC VOID
WIN32DISPLAYBUFFERINWINDOW(HDC DEVICECONTEXT,
                           INT WINDOWWIDTH, INT WINDOWHEIGHT,
                           WIN32_OFFSCREEN_BUFFER BUFFER)
{
    STRETCHDIBITS(DEVICECONTEXT,
                  0, 0, WINDOWWIDTH, WINDOWHEIGHT,
                  0, 0, BUFFER.WIDTH, BUFFER.HEIGHT,
                  BUFFER.MEMORY,
                  &BUFFER.INFO,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
WIN32MAINWINDOWCALLBACK(HWND WINDOW,
                        UINT MESSAGE,
                        WPARAM WPARAM,
                        LPARAM LPARAM)
{       
    LRESULT RESULT = 0;
    
    SWITCH(MESSAGE)
    {
        CASE WM_CLOSE:
        {
            GLOBALRUNNING = FALSE;
        } BREAK;
        
        CASE WM_ACTIVATEAPP:
        {
            OUTPUTDEBUGSTRINGA("WM_ACTIVATEAPP\N");
        } BREAK;
        
        CASE WM_DESTROY:
        {
            GLOBALRUNNING = FALSE;
        } BREAK;
        
        CASE WM_PAINT:
        {
            PAINTSTRUCT PAINT;
            HDC DEVICECONTEXT = BEGINPAINT(WINDOW, &PAINT);
            WIN32_WINDOW_DIMENSION DIMENSION = WIN32GETWINDOWDIMENSION(WINDOW);
            WIN32DISPLAYBUFFERINWINDOW(DEVICECONTEXT, DIMENSION.WIDTH, DIMENSION.HEIGHT,
                                       GLOBALBACKBUFFER);
            ENDPAINT(WINDOW, &PAINT);
        } BREAK;
        
        DEFAULT:
        {
            RESULT = DEFWINDOWPROC(WINDOW, MESSAGE, WPARAM, LPARAM);
        } BREAK;
    }
    
    RETURN(RESULT);
}

INT CALLBACK
WINMAIN(HINSTANCE INSTANCE,
        HINSTANCE PREVINSTANCE,
        LPSTR COMMANDLINE,
        INT SHOWCODE)
{
    WNDCLASS WINDOWCLASS = {};
    
    WIN32RESIZEDIBSECTION(&GLOBALBACKBUFFER, 1280, 720);
    
    WINDOWCLASS.STYLE = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WINDOWCLASS.LPFNWNDPROC = WIN32MAINWINDOWCALLBACK;
    WINDOWCLASS.HINSTANCE = INSTANCE;
    //    WINDOWCLASS.HICON; TODO(SPENCER): ADD ICON
    WINDOWCLASS.LPSZCLASSNAME = "HANDMADEHEROWINDOWCLASS";
    
    IF(REGISTERCLASSA(&WINDOWCLASS))
    {
        HWND WINDOW =
            CREATEWINDOWEXA(
            0,
            WINDOWCLASS.LPSZCLASSNAME,
            "HANDMADE HERO",
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            INSTANCE,
            0);
        IF(WINDOW)
        {
            HDC DEVICECONTEXT = GETDC(WINDOW);
            
            INT XOFFSET = 0;
            INT YOFFSET = 0;
            
            GLOBALRUNNING = TRUE;
            WHILE(GLOBALRUNNING)
            {
                MSG MESSAGE;
                
                WHILE(PEEKMESSAGE(&MESSAGE, 0, 0, 0, PM_REMOVE))
                {
                    IF(MESSAGE.MESSAGE == WM_QUIT)
                    {
                        GLOBALRUNNING = FALSE;
                    }
                    
                    TRANSLATEMESSAGE(&MESSAGE);
                    DISPATCHMESSAGEA(&MESSAGE);
                }
                
                // TODO(SPENCER): POLLING SPEED OKAY?
                FOR(DWORD CONTROLLERINDEX = 0; CONTROLLERINDEX < XUSER_MAX_COUNT; CONTROLLERINDEX++)
                {
                    XINPUT_STATE CONTROLLERSTATE;
                    IF (XINPUTGETSTATE(CONTROLLERINDEX, &CONTROLLERSTATE) == ERROR_SUCCESS)
                    {
                        XINPUT_GAMEPAD *PAD = &CONTROLLERSTATE.GAMEPAD;
                        
                        CHAR DPADUP = (PAD->WBUTTONS & XINPUT_GAMEPAD_DPAD_UP);
                        CHAR DPADDOWN = (PAD->WBUTTONS & XINPUT_GAMEPAD_DPAD_DOWN);
                        CHAR DPADLEFT = (PAD->WBUTTONS & XINPUT_GAMEPAD_DPAD_LEFT);
                        char DpadRight = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        char Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        char Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        char LeftThumb = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_THUMB);
                        char RightThumb = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_THUMB);
                        char LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        char RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        char AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        char BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        char XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        char YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                        
                        int16_t StickX = Pad->sThumbLX;
                        int16_t StickY = Pad->sThumbLY;
                    }
                    else
                    {
                        // Note(Spencer): The controller is not available
                    }
                }
                
                RenderWeirdGradient(GlobalBackbuffer, XOffset, YOffset);
                
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height,
                                           GlobalBackbuffer);
                
                ++XOffset;
                YOffset += 2;
            }
        }
    }
    
    return(0);
}
