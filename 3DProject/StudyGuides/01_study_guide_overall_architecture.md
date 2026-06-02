# Study Guide 1: Overall Win32 and Direct3D 11 Architecture

This guide explains how the application initializes, manages its frame lifecycle, computes frame-rate independent updates, and controls the camera.

---

## 1. Application Lifecycle and Win32 Window Setup

The entry point of your application is `wWinMain` in [Main.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L180).

### The Win32 Initialization Sequence
1. **Memory Leak Detection**: At the very start of `wWinMain`, `_CrtSetDbgFlag` is configured. This instructs the MSVC runtime to check for memory leaks at program exit and print allocations that weren't released.
2. **Window Creation**: Calling `SetupWindow()` in [WindowHelper.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/WindowHelper.cpp#L20) registers a Win32 window class with a custom callback `WindowProc` (handling messages like `WM_DESTROY` to post exit signals) and spawns an overlapping window of $1024 \times 576$ resolution.
3. **Mouse Centering**: The cursor is moved to the center of the screen, and hidden via `ShowCursor(FALSE)` to enable smooth first-person camera mouse tracking.
4. **D3D11 Initialization**: `SetupD3D11` in [D3D11Helper.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/D3D11Helper.cpp#L127) creates the D3D11 core interfaces:
   * **`ID3D11Device`**: Represents the virtual graphics adapter. Used to allocate GPU resources (buffers, textures, shaders).
   * **`ID3D11DeviceContext`**: Represents the pipeline controller. Used to bind resources and dispatch draw commands to the GPU.
   * **`IDXGISwapChain`**: Handles double buffering. It holds the back buffer texture that we render to and swaps it to the monitor when presenting.
   * **`ID3D11RenderTargetView` (rtv)**: Represents the back-buffer render target, allowing the Output Merger stage to write finalized color pixels to the swap chain.

---

## 2. The Main Game Loop

Once initialized, the program enters a message loop in `Main.cpp` that continues until `VK_ESCAPE` is pressed or a `WM_QUIT` window message is received:
```cpp
while (!(GetKeyState(VK_ESCAPE) & 0x8000) && msg.message != WM_QUIT)
{
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    // Update and render frame...
}
```
* **`PeekMessage`** handles incoming operating system events (like keyboard input or resizing) without blocking the thread. If no window messages are queued, it proceeds to update the physics, update particles, and draw the frame.

---

## 3. Frame Delta Time Calculation

To ensure camera movement, rotating demo cubes, and particles update at the same speed regardless of the player's frame rate (FPS), the application calculates a **Delta Time ($dt$)** using high-resolution hardware timers:
```cpp
auto currentTime = std::chrono::high_resolution_clock::now();
float dt = std::chrono::duration<float>(currentTime - previousTime).count();
previousTime = currentTime;
```
* **Why it's crucial**: High-end computers might run the game at 500 FPS ($dt \approx 0.002$s), whereas low-end machines might run it at 30 FPS ($dt \approx 0.033$s).
* **Application**: In `Main.cpp`, dynamic transformations use $dt$:
  * Movement speed: `camera.MoveForward(camSpeed * dt)`
  * Rotations: `rotationAngle += XMConvertToRadians(30.f) * dt`
  * Particle updates: `particleSystem.Update(context, dt)`

---

## 4. The Camera Coordinate System

The camera class `CameraD3D11` in [CameraD3D11.h](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.h) manages the viewer's orientation in 3D world space.

### 1. Camera Vectors
The camera maintains a local orthonormal basis (three mutually perpendicular vectors):
* **`position`**: Coordinates in 3D world space.
* **`forward`**: The direction the camera is looking.
* **`right`**: The vector pointing to the camera's right side.
* **`up`**: The vector pointing straight up relative to the camera.

### 2. Camera Rotations (Pitch and Yaw)
Rotations are computed in [CameraD3D11.cpp:L75](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.cpp#L75) by rotating vectors around specific axes using quaternions:
* **Yaw (Look Left/Right)**: Rotates the `forward`, `right`, and `up` vectors around the global up axis `(0, 1, 0)`.
* **Pitch (Look Up/Down)**: Rotates vectors around the camera's local `right` vector. It is clamped between $-\frac{\pi}{2}$ and $+\frac{\pi}{2}$ (radians) to prevent the camera from flipping upside down.

### 3. View-Projection Matrices
Every frame, the GPU needs to transform 3D world positions into 2D clip-space coordinates. This requires two matrices:
1. **View Matrix**: Built in [CameraD3D11.cpp:L145](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.cpp#L145) using `XMMatrixLookAtLH(pos, pos + forward, up)`. It transforms coordinates from global **World Space** to **Camera Space** (where the camera is at origin `(0,0,0)` looking down the Z axis).
2. **Projection Matrix**: Built in [CameraD3D11.cpp:L146](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.cpp#L146) using `XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ)`. It scales geometry to simulate perspective (objects farther away look smaller) and maps coordinate bounds to **Clip Space** ($[-1, 1]$ in X/Y, $[0, 1]$ in Z).
3. **Combination**: Multiplying these matrices `View * Projection` creates the camera view-projection matrix used to transform vertices in vertex shaders.

---

## Teacher Presentation Tips 🎓
* **Be prepared to explain why `GetFocus() == window` is checked**: Point out that it prevents mouse-locking and camera spinning when the user Alt-Tabs out of the application.
* **Explain high-resolution clocks**: High-resolution clock uses the CPU's hardware performance counter (underlying Win32 API `QueryPerformanceCounter`), which is far more accurate than simple Millisecond timers like `timeGetTime` or `GetTickCount`.
