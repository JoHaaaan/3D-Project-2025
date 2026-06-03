# Study Guide 1: Overall Win32 and Direct3D 11 Architecture

This guide explains how the application initializes, manages its frame lifecycle, computes frame-rate independent updates, and controls the camera.

---

## 1. System Architecture Overview

Before looking at the code, it is important to understand how the components are organized. The application is a native Windows C++ desktop program that communicates directly with the graphics card via the Direct3D 11 API.

```
+--------------------------------------------------------------------------+
|                                  WIN32 APP                               |
|  +------------------+     +-------------------+     +-----------------+  |
|  |    wWinMain()    | --> |    WindowProc     | --> |    Game Loop    |  |
|  |  (Entry Point)   |     |  (Message Callback) |     |  (Frame Update) |  |
|  +------------------+     +-------------------+     +-----------------+  |
+-----------|--------------------------------------------------|-----------+
            |                                                  |
            v                                                  v
+-----------------------+                         +------------------------+
|      DIRECT3D 11      |                         |      SYSTEM MODULES    |
|  +-----------------+  |                         |  +------------------+  |
|  | ID3D11Device    |  |                         |  | CameraD3D11      |  |
|  | (Allocations)   |  |                         |  +------------------+  |
|  +-----------------+  |                         |  | LightManager     |  |
|  | ID3D11DeviceCtx |  |                         |  +------------------+  |
|  | (Draw Calls)    |  |                         |  | QuadTree         |  |
|  +-----------------+  |                         |  +------------------+  |
|  | IDXGISwapChain  |  |                         |  | ParticleSystem   |  |
|  | (Frame Buffers) |  |                         |  +------------------+  |
|  +-----------------+  |                         |  | GBufferD3D11     |  |
+-----------------------+                         +------------------------+
```

---

## 2. Application Lifecycle and Win32 Window Setup

The entry point of your application is `wWinMain` in [Main.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L180).

### The Win32 Initialization Sequence

1. **Memory Leak Detection**: At the very start of `wWinMain`, `_CrtSetDbgFlag` is configured:
   ```cpp
   _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
   ```
   This instructs the MSVC runtime debug heap allocator to record all allocations. When the program exits, if any memory blocks have not been freed using `delete` or `Release()`, they are dumped to the Output console with their allocation ID and file path.

2. **Window Creation**: Calling `SetupWindow()` in [WindowHelper.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/WindowHelper.cpp#L20) registers a Win32 window class with a custom callback `WindowProc`.
   * **`WNDCLASSEX` Registration**: The structure describes the window style, icon, cursor, background brush, and points to our message handler function `WindowProc`.
   * **`CreateWindowEx`**: Spawns an overlapping window with client area dimensions $1024 \times 576$.
   * **`WindowProc` Callback**: Handles system messages (like `WM_DESTROY`, `WM_SIZE`, `WM_INPUT`). If it receives `WM_DESTROY` (when the window is closed), it calls `PostQuitMessage(0)` to push a `WM_QUIT` message into the thread's queue.

3. **Mouse Centering & Locking**:
   The cursor is moved to the center of the window and hidden via `ShowCursor(FALSE)` to enable smooth first-person camera mouse tracking. To keep the cursor within the boundaries of the window, we use `ClipCursor(&rect)`. This prevents the mouse from clicking off-screen or onto other windows when playing.

4. **D3D11 Initialization**: `SetupD3D11` in [D3D11Helper.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/D3D11Helper.cpp#L127) initializes the hardware communication:
   * **`ID3D11Device`**: Represents the virtual graphics card. Used to allocate GPU resources (textures, constant buffers, vertex buffers, index buffers, and compile shaders). It is thread-safe.
   * **`ID3D11DeviceContext`**: Represents the pipeline controller. Used to bind resources (buffers, textures, shaders) to pipeline stages and dispatch draw commands to the GPU. It is not thread-safe and is optimized for fast, single-threaded graphics command submission.
   * **`IDXGISwapChain`**: Handles double buffering. It manages the swap chain textures: the **Front Buffer** (active on monitor) and the **Back Buffer** (being written to by the GPU). We swap them using `Present()`.
   * **`ID3D11RenderTargetView` (RTV)**: Wraps the swapchain's back-buffer texture so the Output Merger stage of the graphics pipeline can write color pixels to it.

---

## 3. The Main Game Loop

Once initialized, the program enters a message loop in `Main.cpp` that continues until `VK_ESCAPE` is pressed or a `WM_QUIT` window message is received:

```cpp
while (!(GetKeyState(VK_ESCAPE) & 0x8000) && msg.message != WM_QUIT)
{
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    else
    {
        // Update physics, update camera, run animations, and render
        UpdateAndRenderFrame();
    }
}
```

* **`PeekMessage` vs `GetMessage`**:
  * **`GetMessage`** is blocking; if there are no window messages (keyboard input, mouse moves) in the queue, it puts the thread to sleep. This is fine for word processors but pauses game loops.
  * **`PeekMessage`** is non-blocking. It checks the queue. If a message exists (e.g., the window is being resized or dragged), it processes it using `TranslateMessage` and `DispatchMessage` (which routes it to our `WindowProc` callback). If no messages are queued, it returns `false` immediately, running our game update logic.
* **`PM_REMOVE` Flag**: Tells the OS to delete processed messages from the queue.

---

## 4. Frame Delta Time Calculation

To ensure camera movement, rotating demo cubes, and particles update at the same speed regardless of the player's frame rate (FPS), the application calculates a **Delta Time ($dt$)** using high-resolution hardware timers:

```cpp
auto currentTime = std::chrono::high_resolution_clock::now();
float dt = std::chrono::duration<float>(currentTime - previousTime).count();
previousTime = currentTime;
```

* **Why it's crucial**: High-end computers might run the game at 500 FPS ($dt \approx 0.002$s), whereas low-end machines might run it at 30 FPS ($dt \approx 0.033$s). Without delta time, moving a camera forward by a static distance `0.1` per frame would translate to $50$ units per second on the 500 FPS computer and only $3$ units per second on the 30 FPS computer.
* **Application**: In `Main.cpp`, dynamic transformations use $dt$:
  * Movement speed: `camera.MoveForward(camSpeed * dt)`
  * Rotations: `rotationAngle += XMConvertToRadians(30.f) * dt`
  * Particle updates: `particleSystem.Update(context, dt)`
* **Delta Clamping**: If the game window is dragged or frozen, $dt$ can spike (e.g., $dt = 2.0$s). If we run Euler integration with a huge timestep, physics calculations explode. We clamp delta time to a maximum threshold (like `0.1f` seconds) to protect simulation stability.

---

## 5. The Camera Coordinate System

The camera class `CameraD3D11` in [CameraD3D11.h](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.h) manages the viewer's orientation in 3D world space.

### 1. Camera Vectors (Orthonormal Basis)
The camera maintains a local orthonormal basis (three mutually perpendicular unit vectors):
* **`position`**: Coordinates in 3D world space.
* **`forward`**: The direction the camera is looking.
* **`right`**: The vector pointing to the camera's right side.
* **`up`**: The vector pointing straight up relative to the camera.

```
       +Y (up)
        |   -Z (forward)
        |  /
        | /
        |/___ +X (right)
```

### 2. Camera Rotations (Pitch and Yaw)
Rotations are computed in [CameraD3D11.cpp:L75](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.cpp#L75) by rotating vectors around specific axes using quaternions:
* **Yaw (Look Left/Right)**: Rotates the `forward`, `right`, and `up` vectors around the global up axis `(0, 1, 0)`.
* **Pitch (Look Up/Down)**: Rotates vectors around the camera's local `right` vector. It is clamped between $-\frac{\pi}{2}$ and $+\frac{\pi}{2}$ (radians) to prevent the camera from flipping upside down.

### 3. View-Projection Matrices
Every frame, the GPU needs to transform 3D world positions into 2D clip-space coordinates. This requires two matrices:

#### The View Matrix
The View Matrix transforms coordinates from global **World Space** to **Camera Space** (putting the camera at origin `(0,0,0)` looking down the Z axis). It is constructed using `XMMatrixLookAtLH(pos, pos + forward, up)`.
Mathematically, given a camera position $\vec{P}$ and its orthonormal basis axes $\vec{R}$ (right), $\vec{U}$ (up), and $\vec{F}$ (forward), the View Matrix $V$ is represented as:
$$V = \begin{bmatrix} R_x & U_x & F_x & 0 \\ R_y & U_y & F_y & 0 \\ R_z & U_z & F_z & 0 \\ -(\vec{P} \cdot \vec{R}) & -(\vec{P} \cdot \vec{U}) & -(\vec{P} \cdot \vec{F}) & 1 \end{bmatrix}$$

#### The Projection Matrix
The Projection Matrix transforms coordinates from **Camera Space** to **Clip Space** ($[-1, 1]$ in X/Y, $[0, 1]$ in Z). It scales coordinates to simulate perspective (farther objects look smaller) and is built using `XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ)`:
$$P = \begin{bmatrix} \frac{d}{aspect} & 0 & 0 & 0 \\ 0 & d & 0 & 0 \\ 0 & 0 & \frac{f}{f - n} & 1 \\ 0 & 0 & \frac{-n \cdot f}{f - n} & 0 \end{bmatrix}$$
where:
* $d = \cot(\frac{\text{fov}}{2})$ represents the focal length.
* $aspect$ is the viewport width divided by height.
* $n$ and $f$ are the near and far clip plane distances.
* Note that the coordinate $W'$ in clip space holds the original camera-space depth $Z$, which is used in the perspective divide stage ($X'/W', Y'/W', Z'/W'$).

---

## Teacher Presentation Tips 🎓

* **Why is `GetFocus() == window` checked before updating camera rotation?**:
  * *Answer*: This ensures we only capture mouse inputs if the game window is currently active and focused by the OS. If the user Alt-Tabs out of the game, focus is lost, and we disable mouse-centering. If we did not check this, the game would continue locking the cursor to the center of the screen, preventing the user from using other programs, and the camera would spin wildly due to invalid mouse coordinate updates.
* **Explain how COM interfaces manage resource lifetimes**:
  * *Answer*: Direct3D 11 objects (like textures, buffers, and shaders) are Component Object Model (COM) interfaces. They manage their lifetimes using Reference Counting. When we create a COM interface, its ref count is 1. If we bind or copy it, the count increments. To prevent memory leaks, we must call `.GetAddressOf()` and `Release()` (or use `Microsoft::WRL::ComPtr` wrappers which handle this automatically via RAII, releasing when they fall out of scope).
* **Why do we use Left-Handed coordinate conventions instead of Right-Handed?**:
  * *Answer*: Both work, but Direct3D 11 uses a Left-Handed system by default in its projection mathematics (`LH` functions). In Left-Handed space, the $+Z$ axis points forward *into* the screen. This is intuitive for depth buffer management, where depth increases from $0.0$ (near plane) to $1.0$ (far plane) moving away from the camera.
* **What is the purpose of the Perspective Divide stage, and is it handled in shaders?**:
  * *Answer*: The perspective divide projects 3D clip-space coordinates into 2D Normalized Device Coordinates (NDC) by dividing $X, Y, Z$ by the homogeneous coordinate $W$. This division ($X/W$, $Y/W$) scales coordinates based on distance, creating perspective. It is **not** handled in shaders; it is a fixed-function hardware step executed by the GPU rasterizer unit immediately after the vertex (or domain) shader and clip checking.
* **Why does VSync prevent screen tearing, and how do you toggle it in D3D11?**:
  * *Answer*: Screen tearing occurs when the GPU writes to the backbuffer and swaps it to the screen while the monitor is mid-refresh, displaying parts of two different frames simultaneously. VSync forces the swapchain present call to block until the monitor enters its vertical blanking interval (Vsync scanline). In D3D11, we toggle it by passing `1` (VSync active) or `0` (VSync disabled) as the first argument in `IDXGISwapChain::Present(SyncInterval, Flags)`.
