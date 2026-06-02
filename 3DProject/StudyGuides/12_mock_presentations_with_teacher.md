# Study Guide 12: Mock 1-on-1 Presentations with a Teacher (Ultimate In-Depth Edition)

This guide contains three detailed mock presentation exam sessions designed to prepare you for a rigorous, system-level 1-on-1 review. Each session contains exactly **4 core topics**. Every topic features a complete, four-step dialogue trace:
1. **Main Question**: The teacher asks you to explain the core implementation and trace specific C++ or HLSL lines.
2. **Main Answer**: You walk through the code, explaining logic, passes, API calls, and pipeline stages.
3. **Teacher Follow-Up 1**: Probes pipeline hazards, D3D11 validation layers, state binding conflicts, or viewport changes.
4. **Follow-Up Answer 1**: You resolve the D3D11 runtime state lifecycle details.
5. **Teacher Follow-Up 2**: Probes hardware scheduling, memory alignment, Euler integration failures, or geometric math.
6. **Follow-Up Answer 2**: You explain GPU execution and mathematical limits (e.g., fractional tessellation, projection coordinate distortions).
7. **Teacher Follow-Up 3**: Probes advanced low-level topics (VRAM bandwidth bounds, cache layouts, CPU-GPU sync stalls, View Matrix derivations, or bounding box calculations).
8. **Follow-Up Answer 3**: You deliver a senior-level engineering analysis with full mathematical equations and GPU cache profiles.

---

## 🎭 Session 1: Loop Lifecycle + Shadow Slices + Structured Buffers + Buffer Bindings

### Topic 1: Tracing the Frame Lifecycle

**T**: "Let's start by looking at your main loop. Walk me through the exact rendering sequence of a single frame in [Main.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp). Trace the sequence of passes, naming the C++ lines where they start, and explain how the final image ends up on the screen."

**S**: "Every frame in our rendering loop in [Main.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp) runs through a sequence of distinct CPU and GPU passes to coordinate deferred shading, shadows, reflections, and transparent particles:
1. **Matrix & Physics Update**: We update keyboard/mouse input, modify the camera matrices, and update particle positions on the CPU.
2. **Shadow Pass (L571)**: We disable pixel and tessellation shaders (`context->PSSetShader(nullptr, nullptr, 0)`), bind the shadow map viewport ($2048 \times 2048$), and render the scene depth from the perspective of each light into the `DepthBufferD3D11 shadowMap` slices.
3. **Environment Map Pass (L620)**: We position six virtual cameras at the center of the reflective sphere and render the surrounding scene into the six faces of the dynamic cubemap using forward shaders (`CubeMapPS.hlsl`).
4. **Geometry Pass (L636)**: We bind our G-Buffer RTVs (`albedoRT`, `normalRT`, `positionRT`) and the main depth buffer DSV, and render visible meshes to pack raw material properties into the G-Buffer.
5. **Lighting Pass (CS) (L843)**: We unbind the G-Buffer RTVs and bind them as input SRVs. We bind the shadow map array (slot `t3`), light buffer (slot `t4`), and our output UAV (`lightingUAV`, slot `u0`), and dispatch `LightingCS.hlsl`.
6. **Particle Pass (L873)**: We bind `lightingRTV` and `myDSV` to the Output Merger, set the alpha blend state, and forward-render transparent particles.
7. **Present (L886)**: We extract the swapchain backbuffer texture, call `context->CopyResource(backBuffer, lightingTex)` to copy the final composite image, and present the frame."

**T (Follow-Up 1)**: "You mentioned using `CopyResource` to transfer `lightingTex` to the backbuffer in step 7. Why don't you render the deferred compute shader directly to the swapchain's backbuffer texture to save a copy?"

**S (Follow-Up Answer 1)**: "The swapchain's backbuffer texture is managed by DXGI and the OS display kernel. Depending on the user's hardware and swap chain configuration, it might not support Unordered Access Views (`D3D11_BIND_UNORDERED_ACCESS`). By rendering the compute shader to our intermediate `lightingTex` texture (which is allocated with UAV bind flags) and copying it using the GPU-optimized `context->CopyResource`, we guarantee hardware compatibility. This also allows us to bind `lightingTex` as a render target in the particle pass to draw transparency effects directly on top of the deferred lighting results before presenting."

**T (Follow-Up 2)**: "Where exactly in your frame loop is depth testing configured for the deferred geometry pass vs. the transparent particle pass? What happens if you forget to reset the viewport or depth-stencil state after the shadow pass?"

**S (Follow-Up Answer 2)**: "In [Main.cpp:L638-642](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L638), before drawing geometry, we bind `myDSV` (our main depth-stencil view) to `SetAsRenderTargets` and reset the viewport to screen dimensions ($1024 \times 576$). 
During the particle pass in [Main.cpp:L876](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L876), we bind the same `myDSV` depth view alongside `lightingRTV` but configure alpha blending. This allows particles to perform standard depth testing (so they are correctly culled by solid walls stored in `myDSV`) without writing new depth values (since particles are transparent). 
If we forgot to reset the viewport after the shadow pass, the GPU would continue rendering the geometry pass using the shadow viewport coordinates ($2048 \times 2048$), compressing the entire scene into the bottom-left corner of the G-Buffer."

**T (Follow-Up 3)**: "Explain the synchronization/latency implications of swap chain presentation modes (e.g. `DXGI_SWAP_EFFECT_FLIP_DISCARD` vs sequential, vs VSync `Present(1, 0)`). How does the CPU write command buffers while the GPU is executing previous frames, and how do we prevent CPU-GPU synchronization stalls?"

**S (Follow-Up Answer 3)**: "Using flip-model swap chains (`DXGI_SWAP_EFFECT_FLIP_DISCARD`) bypasses the Desktop Window Manager (DWM) redirection surface copy, allowing the GPU to flip its memory pointer directly to the display controller, reducing latency. 
When we call `SwapChain->Present(1, 0)`, the '1' enables VSync, aligning frame delivery with the monitor's refresh rate. 
The CPU and GPU execute asynchronously; the CPU writes commands to a Ring Buffer (Command Queue) while the GPU reads and executes them. If the CPU is faster, it can build commands up to 3 frames ahead. 
To prevent the CPU from running too far ahead (which increases input lag) or stalling the GPU (which causes frame drops), we use DXGI frame latency controls via `IDXGIDevice1::SetMaximumFrameLatency(1)` or query objects. This blocks the CPU thread inside `Present` if the GPU is more than one frame behind, keeping input latency low and frame rates stable."

---

### Topic 2: Shadow Mapping Pipeline Integration

**T**: "In [Main.cpp:L597-598](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L597), you bind a `null` Render Target View but bind a Depth Stencil View during the shadow pass:
```cpp
ID3D11RenderTargetView* nullRTV = nullptr;
context->OMSetRenderTargets(1, &nullRTV, shadowDSV);
```
Why does this not trigger a Direct3D 11 validation warning? How is depth written without a pixel shader, and how does the compute shader sample it?"

**S**: "In D3D11, rendering depth without a bound RTV is a valid and optimized state called **depth-only rendering**. We disable the pixel shader stage:
`context->PSSetShader(nullptr, nullptr, 0);`
Since the pixel shader is null, the graphics pipeline stops after rasterization. The rasterizer generates fragments and interpolates their depth values ($Z$) from the vertices output by the vertex shader. The GPU's hardware **Early-Z / Late-Z testing** writes these depth values directly into the bound `shadowDSV`, bypassing the pixel shading stage completely.
In the lighting pass [LightingCS.hlsl:L88](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L88), we sample this depth array using:
`float shadow = shadowMaps.SampleCmpLevelZero(shadowSampler, float3(shadowUV, lightIndex), depth);`
The comparison sampler compares the current fragment's light-space depth against the shadow map depth and returns a filtered value between $0.0$ (in shadow) and $1.0$ (fully lit)."

**T (Follow-Up 1)**: "If the pixel shader is disabled during the shadow pass, does that mean the GPU skips the rasterizer stage entirely? How does the hardware know which triangles face away from the light?"

**S (Follow-Up Answer 1)**: "No, the GPU does not skip the rasterizer. The rasterizer is still responsible for primitive assembly, clipping, and face culling. In [Main.cpp:L580](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L580), we bind `shadowRasterizerState`:
`context->RSSetState(shadowRasterizerState);`
This state is configured with back-face culling (`D3D11_CULL_BACK`). The rasterizer performs edge equation testing to determine triangle winding order (clockwise vs. counter-clockwise) relative to the light perspective. Any triangles facing away from the light source are culled before depth testing, saving GPU cycles."

**T (Follow-Up 2)**: "Your lights are spotlights and point lights. How does the projection projection frustum differ for a spotlight vs a directional light, and how does that affect the view-projection matrix calculation in C++?"

**S (Follow-Up Answer 2)**: "Spotlights emit light outward from a point in a cone shape, requiring a **Perspective Projection** to simulate divergence:
`XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);`
Directional lights emit parallel rays from an infinite distance (like the sun), which requires an **Orthographic Projection**:
`XMMatrixOrthographicLH(width, height, nearZ, farZ);`
For spotlights, we use perspective math inside [LightManager.cpp:L73](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightManager.cpp#L73) to scale depth values non-linearly ($1/Z$). For directional lights, depth scales linearly ($Z$), which means the hardware depth bias settings in the rasterizer must be configured differently for each projection type to prevent shadow acne."

**T (Follow-Up 3)**: "How do you address shadow acne and light bleeding? Explain the mathematics of slope-scaled depth bias (e.g. `DepthBias`, `SlopeScaledDepthBias`, `DepthBiasClamp` in `D3D11_RASTERIZER_DESC`) and why we choose those values based on the shadow map resolution."

**S (Follow-Up Answer 3)**: "Shadow acne occurs because of precision limits in the shadow map texture. Multiple screen pixels map to a single shadow map texel. When checking depth, the floating-point values alternate due to precision, creating dark banding lines. 
To resolve this, we apply a bias to the depth values during shadow rendering. The D3D11 formula is:
$$\text{Bias} = (\text{DepthBias} \times r) + (\text{SlopeScaledDepthBias} \times \text{MaxSlope})$$
where $r$ is the minimum resolvable depth difference depending on the depth buffer format. 
* **`DepthBias`** shifts all triangles slightly away from the light.
* **`SlopeScaledDepthBias`** scales the bias based on the angle of the triangle relative to the light. Steeper slopes require a larger bias.
* **`DepthBiasClamp`** prevents this bias from scaling too high on extremely steep angles, which would cause 'Peter Panning' (where shadows appear detached from the object base). 
In our project, we tune `DepthBias = 100` and `SlopeScaledDepthBias = 1.5` to match our $2048 \times 2048$ resolution and prevent both acne and Peter Panning."

---

### Topic 3: Structured Buffer Allocation & GPU Pipeline Barriers

**T**: "Your particle system in [ParticleSystemD3D11.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleSystemD3D11.cpp) uses a structured buffer. How is this buffer allocated in C++, and why must you explicitly call `context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr)` in the update pass?"

**S**: "The structured buffer is allocated in [StructuredBufferD3D11.cpp:L25-63](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/StructuredBufferD3D11.cpp#L25) by describing the buffer with `D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE` and the flag `D3D11_RESOURCE_MISC_BUFFER_STRUCTURED`.
The unbinding step with `nullUAV` in [ParticleSystemD3D11.cpp:L120](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleSystemD3D11.cpp#L120) is required due to D3D11's **pipeline binding barriers**. 
A resource cannot be bound as an input (SRV) in the vertex shader while it is still bound as an active output (UAV) in the compute shader. If we do not clear the UAV slot by binding `nullUAV`, the D3D11 runtime detects a read-write conflict when we bind it as an SRV during the particle render pass, and it will force the SRV slot to `null`, causing the particles to render invisible."

**T (Follow-Up 1)**: "If you violate this pipeline barrier, what happens at the hardware level on the GPU? Does the application crash?"

**S (Follow-Up Answer 1)**: "It will not crash the physical GPU or the OS. However, the Direct3D runtime will output a warning/error in the debug layer stating that a resource bound as a UAV is being bound as an SRV. To prevent memory hazards, the driver will automatically bind the conflicting input slot (the Vertex Shader SRV) to `nullptr`. The vertex shader will pull empty data, and the particles will not be drawn, resulting in a black screen for the particle pass."

**T (Follow-Up 2)**: "In C++, your StructuredBuffer is initialized with `D3D11_USAGE_DEFAULT`. Why did you choose this memory usage flag over `D3D11_USAGE_DYNAMIC` or `D3D11_USAGE_STAGING` for a particle simulation buffer?"

**S (Follow-Up Answer 2)**: "
* **`D3D11_USAGE_DEFAULT`** allows the GPU to perform read and write operations directly on the buffer. Since our Compute Shader updates particle physics and our Vertex Shader reads them for rendering, the entire lifecycle occurs on the GPU. Default usage is the only memory type that allows UAV writes.
* **`D3D11_USAGE_DYNAMIC`** is write-only from the CPU (using `Map`/`Unmap`), which would force the CPU to compute physics and write the updates over the PCIe bus, creating a performance bottleneck.
* **`D3D11_USAGE_STAGING`** supports CPU read/writes but cannot be bound to the graphics pipeline for drawing. Default usage is the only choice that keeps the simulation entirely on the GPU."

**T (Follow-Up 3)**: "In [StructuredBufferD3D11.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/StructuredBufferD3D11.cpp), what is the difference between a Structured Buffer and a Constant Buffer (`cbuffer`) in terms of GPU register limits, access latency, and memory caching (e.g., Constant Buffer Cache vs L1/L2 cache for SRVs)? Why not store light properties in a Structured Buffer?"

**S (Follow-Up Answer 3)**: "
* **Constant Buffers** are capped at 64KB per buffer and bound directly to fast constant registers (like `cb0` to `cb15`). They have a dedicated high-speed **Constant Cache (CBC)** on the GPU. Constant buffers are optimized for low-latency uniform access, meaning every shader thread in a warp reads the exact same address at the same time.
* **Structured Buffers** are virtually unlimited in size, stored in general VRAM, and read via the texture cache hierarchy (L1 and L2 caches). Since they are indexed dynamically using thread IDs, thread access patterns can vary. If threads in a warp access non-contiguous elements in a structured buffer, it causes **memory divergence**, thrashing the L1 cache and requiring high-latency reads from VRAM.
We store light properties in a Constant Buffer because our light count is small, and all shader threads read the same light array uniformly, making the constant cache much faster."

---

### Topic 4: Static Mesh Buffers & Assembler Formats

**T**: "Show me how you draw a standard mesh. How do you bind vertex and index buffers in C++?"

**S**: "In [MeshD3D11.cpp:L82](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/MeshD3D11.cpp#L82), we bind the vertex and index buffers to the Input Assembler stage using the device context:
```cpp
UINT stride = vertexBuffer.GetVertexSize(); // size of Vertex struct (32 bytes)
UINT offset = 0;
ID3D11Buffer* vb = vertexBuffer.GetBuffer();
context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
context->IASetIndexBuffer(indexBuffer.GetBuffer(), DXGI_FORMAT_R32_UINT, 0);
```
Once bound, we draw submeshes using `DrawIndexed`, passing the index count and starting index:
`context->DrawIndexed(numIndices, startIndex, 0);`"

**T (Follow-Up 1)**: "Why do we use `DXGI_FORMAT_R32_UINT` for the index buffer format? What would change in C++ and HLSL if we used 16-bit indices (`DXGI_FORMAT_R16_UINT`)?"

**S (Follow-Up Answer 1)**: "
* In C++, if we changed the format to `DXGI_FORMAT_R16_UINT`, we would have to modify the OBJ Parser to output `uint16_t` arrays instead of `uint32_t` (which is represented by `unsigned int`). This reduces the index buffer size in GPU memory by 50% (2 bytes per index instead of 4 bytes).
* In HLSL, no shader code would need to change. The GPU's hardware Input Assembler handles index parsing automatically based on the format parameter we pass in C++. 
* However, 16-bit indices limit our meshes to a maximum of 65,535 vertices. Since some of our larger OBJ meshes exceed this vertex count, we use 32-bit indices (`R32_UINT`) to support up to 4.29 billion vertices."

**T (Follow-Up 2)**: "What is the difference between `IASetVertexBuffers` and `IASetIndexBuffer` parameters? Why does the vertex buffer call take arrays of buffers, strides, and offsets, while the index buffer call takes only a single buffer?"

**S (Follow-Up Answer 2)**: "Direct3D 11 supports **multi-stream vertex rendering**, which allows binding multiple vertex buffers to different slots at the same time (e.g., storing positions in slot 0, normals in slot 1, and UVs in slot 2). Because of this, `IASetVertexBuffers` takes arrays of buffers, strides, and offsets. 
Conversely, the GPU Input Assembler can only parse a single index layout at a time to assemble primitives (triangles), so `IASetIndexBuffer` only accepts a single buffer pointer, a single format, and a single offset."

**T (Follow-Up 3)**: "Explain the performance impact of Vertex Cache Optimization (like Post-Transform Cache). How does the index order in the index buffer affect GPU vertex shading reuse, and how does the Input Assembler handle vertex cache hits?"

**S (Follow-Up Answer 3)**: "Before rasterization, the GPU runs the vertex shader on vertex data. To avoid shading the same vertex multiple times if it is shared by multiple triangles, the GPU stores the shaded output in a small **Post-Transform Vertex Cache** (usually holding 16 to 32 vertices). 
The Input Assembler reads incoming indices. If an index refers to a vertex already in the cache, the GPU skips the vertex shader and reuses the cached result (a cache hit). 
If our indices are ordered randomly, the cache misses constantly, forcing the GPU to run the vertex shader up to $3\times$ more than necessary. We use index optimization algorithms (such as the Forsyth algorithm) to order our index buffer for high cache locality, reducing vertex shader executions closer to the theoretical minimum of 0.5 shader runs per triangle."

---

## 🎭 Session 2: Core Matrix Math + Deferred G-Buffer Formats + Dynamic Cubemap Multi-Passes

### Topic 1: Delta Time & Frame-rate Independence

**T**: "Explain the importance of delta time ($dt$) and show me how it is calculated and applied in your loop."

**S**: "Delta time represents the exact duration of the previous frame, calculated using `std::chrono::high_resolution_clock` in [Main.cpp:L491](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L491). Without delta time, physics and movements would run at different speeds depending on the computer's frame rate. We multiply all movement speeds by `dt` to make them frame-rate independent:
`camera.MoveForward(camSpeed * dt);`"

**T (Follow-Up 1)**: "What happens to your physics and particle simulation if your game freezes for 2 seconds (e.g. due to window movement or a CPU spike)? How does a huge `dt` affect your Euler integration in `ParticleUpdateCS.hlsl`?"

**S (Follow-Up Answer 1)**: "If the game freezes for 2 seconds, the calculated delta time (`dt`) spikes to `2.0`. 
Inside `ParticleUpdateCS.hlsl`, we update particle positions using:
`particle.position += particle.velocity * deltaTime;`
If `deltaTime` is `2.0`, the particle position jumps forward by a huge distance in a single frame. This can cause particles to overshoot their target coordinates, pass through solid boundaries (collision tunneling), or scatter randomly."

**T (Follow-Up 2)**: "How do you mitigate this issue in production-ready games? How can you modify the C++ frame timer code to prevent physics explosions during stutters?"

**S (Follow-Up Answer 2)**: "We implement **Delta Time Clamping** or a **Fixed Timestep** accumulator. In C++, we clamp `dt` to a maximum threshold (e.g., `0.1` seconds):
`float dt = std::min(measuredDeltaTime, 0.1f);`
This limits the physics update step size during stutters. If the frame rate drops below 10 FPS, the simulation slows down instead of breaking, preventing collision tunneling and particle system explosions."

**T (Follow-Up 3)**: "How do high-precision timers (like `QueryPerformanceCounter` or `chrono::high_resolution_clock`) avoid drift or precision loss over long play sessions? What is the impact of floating-point precision limitations (e.g., 32-bit float vs 64-bit double) when accumulating time or positions in a large world?"

**S (Follow-Up Answer 3)**: "High-precision timers query hardware registers (like the CPU's TSC or HPET) that run at high frequencies. We convert these ticks to seconds using double-precision calculations to prevent precision loss. 
If we use a 32-bit float to accumulate delta time over a long session, we run into a precision issue called **float underflow**. A 32-bit float has only 24 bits of mantissa precision ($\approx 7$ decimal digits). 
If the total accumulated run time reaches $100,000$ seconds, adding a small delta time (e.g., $0.016$ seconds for 60 FPS) will fail because the difference in scale is too large:
$$100,000.0 + 0.016 = 100,000.0$$
The tick is lost, causing the timer to freeze. We avoid this by accumulating total elapsed time in a `double` (64-bit float, with $\approx 15-17$ digits of precision) and shifting coordinates to the camera's local origin in large worlds."

---

### Topic 2: Matrix Transpositions and Coordinate Spaces

**T**: "In [CameraD3D11.cpp:L149](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.cpp#L149), why do you transpose the View-Projection matrix before writing it to the constant buffer? What is the difference between row-major and column-major matrix multiplication in HLSL?"

**S**: "C++ classes using DirectXMath (like `XMMATRIX`) store matrices in **row-major** order (consecutive rows in contiguous memory). In HLSL, the GPU default layout expects **column-major** order (consecutive columns in contiguous memory).
* In row-major, vector-matrix multiplication is: $\vec{v}' = \vec{v} \cdot M$.
* In column-major, vector-matrix multiplication is: $\vec{v}' = M \cdot \vec{v}$.
If we upload a row-major matrix to a constant buffer without transposing, the GPU reads the columns as rows, scrambling the matrix values. We transpose the matrix using `XMMatrixTranspose()` before uploading it to align the C++ memory layout with the column-major layout expected by HLSL."

**T (Follow-Up 1)**: "Is there a way to avoid calling `XMMatrixTranspose` in C++ for every constant buffer update? How can you configure HLSL or compile settings to bypass this?"

**S (Follow-Up Answer 1)**: "Yes, there are two ways:
1. **Shader decorations**: We can prefix the matrix variable declaration in HLSL with the `row_major` keyword (e.g., `row_major float4x4 worldMatrix;`). This tells the HLSL compiler to generate instructions that assume row-major ordering.
2. **Compiler Flags**: We can pass the `/Zpr` compiler flag to the HLSL compiler (`fxc.exe`) when compiling our shaders, which changes the default matrix packing convention to row-major for all matrices. Since our codebase compiles with the default column-major settings, we must transpose on the CPU side."

**T (Follow-Up 2)**: "In [CameraD3D11.cpp:L145-146](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.cpp#L145), you use `XMMatrixLookAtLH` (Left-Handed) and `XMMatrixPerspectiveFovLH`. What does Left-Handed coordinate space mean, and how does it affect the Z-axis orientation in your clip space?"

**S (Follow-Up Answer 2)**: "In a left-handed coordinate system, the positive Z-axis points **forward** into the screen (away from the viewer). If you align your left hand's thumb along $+X$ (right) and index finger along $+Y$ (up), your middle finger points along $+Z$ (forward). 
This differs from right-handed coordinate systems (like OpenGL) where positive Z points out of the screen (toward the viewer). In our left-handed clip space, depth values are stored in the range $[0.0, 1.0]$, where `0.0` represents the near plane and `1.0` represents the far plane."

**T (Follow-Up 3)**: "Walk me through the mathematical derivation of the View Matrix. Given a camera's position $\vec{P}$, target vector $\vec{T}$, and world up vector $\vec{U}$, how do you construct the orthonormal basis ($\vec{R}$, $\vec{U}'$, $\vec{F}$) and how does that form the translation-rotation inversion view matrix?"

**S (Follow-Up Answer 3)**: "The View Matrix transforms coordinates from world space to view space, acting as the inverse of the camera's world transform.
1. First, we compute the forward vector $\vec{F}$ pointing from camera to target:
$$\vec{F} = \text{normalize}(\vec{T} - \vec{P})$$
2. Next, we compute the right vector $\vec{R}$ perpendicular to the forward and world up vectors:
$$\vec{R} = \text{normalize}(\vec{U} \times \vec{F})$$
3. Then, we calculate the orthogonal camera up vector $\vec{U}'$ to complete our basis:
$$\vec{U}' = \vec{F} \times \vec{R}$$
The Camera World matrix $W_C$ is constructed from these basis vectors and position:
$$W_C = \begin{bmatrix} R_x & R_y & R_z & 0 \\ U'_x & U'_y & U'_z & 0 \\ F_x & F_y & F_z & 0 \\ P_x & P_y & P_z & 1 \end{bmatrix}$$
The View Matrix $V$ is the inverse of $W_C$. Since the rotation submatrix is orthogonal, its inverse is its transpose ($R^{-1} = R^T$). The translation inverse is $-P$. Multiplying these gives the View Matrix:
$$V = \begin{bmatrix} R_x & U'_x & F_x & 0 \\ R_y & U'_y & F_y & 0 \\ R_z & U'_z & F_z & 0 \\ -(\vec{P} \cdot \vec{R}) & -(\vec{P} \cdot \vec{U}') & -(\vec{P} \cdot \vec{F}) & 1 \end{bmatrix}$$
This is the matrix generated by `XMMatrixLookAtLH`."

---

### Topic 3: Deferred G-Buffer Formatting & Hazards

**T**: "Explain your Deferred Rendering pipeline. What is a G-Buffer, and what formats and data do you store in it?"

**S**: "In our deferred pipeline, the geometry pass renders material properties into three screen-sized textures (our G-Buffer) instead of calculating lighting immediately:
* `albedoRT`: Stores Diffuse Color (RGB) + Ambient Strength (A).
* `normalRT`: Stores packed World Normal (RGB) + Specular Strength (A).
* `positionRT`: Stores World Position (RGB) + Specular Exponent (A).
We use the **`DXGI_FORMAT_R16G16B16A16_FLOAT`** format for all three render targets to support high precision, negative normal vectors, and positions outside the $[0,1]$ range without banding."

**T (Follow-Up 1)**: "When you unbind the G-Buffer RTVs using `OMSetRenderTargets(3, nullRTVs, nullptr)`, you also pass `nullptr` for the Depth Stencil View. Why do you unbind the DSV here? Does the compute shader read the depth buffer?"

**S (Follow-Up Answer 1)**: "Our current lighting compute shader (`LightingCS.hlsl`) does not read the G-Buffer depth buffer as a shader input (it only reads the position G-Buffer `t2`). However, unbinding the DSV is good practice. If we were to implement depth-based post-processing (such as Screen Space Ambient Occlusion or Depth of Field) in the compute shader, we would need to bind the depth buffer as an SRV. If the DSV remained bound to the Output Merger, D3D11 would block us from binding it as a Shader Resource View."

**T (Follow-Up 2)**: "If you want to render semi-transparent meshes (like glass windows or your particles) in a deferred rendering engine, how do you handle them? Why can't they be rendered directly to the G-Buffer?"

**S (Follow-Up Answer 2)**: "Deferred G-Buffers can only store a single layer of depth and material properties per pixel. If we rendered a semi-transparent mesh to the G-Buffer, it would overwrite the solid surface data behind it. 
To draw transparent objects:
1. We run the deferred geometry and lighting passes first, rendering all solid surfaces.
2. We bind the final deferred color texture as our Render Target.
3. We bind our G-Buffer depth buffer (`myDSV`) as read-only.
4. We render transparent meshes on top using standard forward rendering with alpha blending enabled."

**T (Follow-Up 3)**: "If we switch G-Buffer render targets from `DXGI_FORMAT_R16G16B16A16_FLOAT` to a more packed format like `R8G8B8A8_UNORM` for albedo and oct-encoded normals in `R10G10B10A2_UNORM`, how much memory bandwidth do we save per pixel? What are the trade-offs regarding precision loss or decoding overhead in the pixel shader?"

**S (Follow-Up Answer 3)**: "
* **`R16G16B16A16_FLOAT`** uses $4 \times 16 \text{ bits} = 64 \text{ bits}$ (8 bytes) per render target. With 3 targets, we write $8 \times 3 = 24 \text{ bytes}$ per pixel.
* **Packed Layout**: Storing albedo in `R8G8B8A8_UNORM` (4 bytes), normals in octahedral format in `R10G10B10A2_UNORM` (4 bytes), and reconstructing position from the depth buffer instead of writing positions directly (depth is 4 bytes) saves significant space. This layout uses only 12 bytes per pixel, reducing bandwidth by 50%.
For a $1920 \times 1080$ display at 60 FPS, this saves over 248MB/sec of write bandwidth. 
The trade-offs are:
1. **Precision loss**: Octahedral normal encoding can introduce visual errors (specular artifacts) on low-curvature surfaces.
2. **ALU overhead**: The pixel shader must run extra math to reconstruct positions from depth and decode packed normal vectors. However, modern GPUs are usually bound by memory bandwidth rather than math calculations, so this trade-off improves performance."

---

### Topic 4: Environment Mapping & Cubemaps

**T**: "Let's discuss environment mapping. How do you implement real-time reflections on the sphere using cubemaps?"

**S**: "In [EnvironmentMapRenderer.cpp:L54](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/EnvironmentMapRenderer.cpp#L54), we create a dynamic cubemap texture by rendering the scene six times from the perspective of the reflective object. We set up six virtual cameras pointing in the cardinal directions (+X, -X, +Y, -Y, +Z, -Z), each with a $90^\circ$ FOV and a $1:1$ aspect ratio. We render the scene into each of the six faces, skipping the reflective object itself to avoid self-reflection feedback loops."

**T (Follow-Up 1)**: "Since you render to six different faces, do you clear the depth buffer `m_dsv` once before the loop, or at the start of rendering each face?"

**S (Follow-Up Answer 1)**: "We must clear `m_dsv` at the start of rendering **each** face slice. 
In [TextureCubeD3D11.cpp:L136](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/TextureCubeD3D11.cpp#L136):
`context->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);`
Because all six faces share the same depth buffer, the depth values written when rendering the first face are still in the buffer when we switch to the second face. If we didn't clear the depth buffer before drawing the second face, the old depth values would cause depth testing to fail, culling geometry on the new face."

**T (Follow-Up 2)**: "In [ReflectionPS.hlsl](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ReflectionPS.hlsl), we sample the texture cube using a 3D reflection direction vector $\vec{R}$. How does the GPU texture mapping unit know which of the 6 faces to sample using a 3D vector?"

**S (Follow-Up Answer 2)**: "The GPU identifies the face to sample by finding the component of the 3D direction vector $\vec{R} = (x,y,z)$ with the largest absolute value:
* If $|x|$ is largest and positive, it samples the $+X$ face.
* If $|y|$ is largest and negative, it samples the $-Y$ face.
Once the face is selected, the GPU projects the remaining two coordinates onto that face to calculate standard 2D UV coordinates:
$$U = \frac{c_1}{2 \times |c_{\text{max}}|} + 0.5, \quad V = \frac{c_2}{2 \times |c_{\text{max}}|} + 0.5$$
This calculation is handled entirely in hardware by the texture sampler unit."

**T (Follow-Up 3)**: "How do we prevent rendering seams at the borders of the cubemap faces when sampling with trilinear filtering? Explain how `D3D11_SAMPLER_DESC` address modes (e.g., `D3D11_TEXTURE_ADDRESS_CLAMP`) or hardware-level seamless cubemap filtering handle texel interpolation across face boundaries."

**S (Follow-Up Answer 3)**: "Standard texture sampling clamps coordinates to the edge of the texture (`D3D11_TEXTURE_ADDRESS_CLAMP`), which operates within a single face of the cubemap. When filtering texels at the boundary of a face, the sampler would interpolate with the clamped border pixels of that face, resulting in visible seams at the cubemap edges. 
Direct3D 11 handles this using **Seamless Cubemap Filtering**, which is enabled by default on modern hardware. When sampling near a face edge, the GPU hardware reads the neighboring texels from the adjacent faces, interpolating across the cube seam to ensure smooth transitions across all faces."

---

## 🎭 Session 3: Quadtree Memory Rebuilds + Tessellation LOD Equations + DS Tangent Planes

### Topic 1: Win32 Message Loop & Camera Vectors

**T**: "Explain the structure of your Win32 message loop. How does `PeekMessage` work compared to `GetMessage`?"

**S**: "In `Main.cpp`, our message loop checks for operating system events like key presses or window resizing:
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
* **`GetMessage`** is blocking; if there are no window messages, it puts the thread to sleep, pausing the game.
* **`PeekMessage`** is non-blocking. It checks if a message exists. If it does, it processes it. If it doesn't, it returns immediately, allowing the game loop to continue updating physics, updating particles, and rendering the frame."

**T (Follow-Up 1)**: "Your camera class clamps pitch rotation in [CameraD3D11.cpp:L79](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/CameraD3D11.cpp#L79). Why is it clamped between $-\frac{\pi}{2}$ and $+\frac{\pi}{2}$ radians?"

**S (Follow-Up Answer 1)**: "Pitch represents looking straight up or straight down. If pitch went beyond $\pm\frac{\pi}{2}$ ($90^\circ$), the camera would flip upside down. 
This causes the camera's local `up` vector to invert, which flips the View Matrix calculation. Clamping the pitch prevents this inversion and keeps the camera controls intuitive."

**T (Follow-Up 2)**: "In your Win32 window callback `WindowProc` in [WindowHelper.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/WindowHelper.cpp), you handle the `WM_DESTROY` message. Why must you call `PostQuitMessage(0)` inside it? What happens if you destroy the window but omit this call?"

**S (Follow-Up Answer 2)**: "When the user clicks the close button, the OS destroys the window, sending a `WM_DESTROY` message. 
Calling `PostQuitMessage(0)` posts a `WM_QUIT` message to the message queue. 
If we omit this call, the window is closed, but the message loop never receives `WM_QUIT`. The game loop will continue running in the background indefinitely, consuming CPU resources even though the window is gone. This is a common source of ghost processes."

**T (Follow-Up 3)**: "How does the camera class extract its forward, right, and up vectors from the View Matrix rotation component, and how do we use them to construct movements in world space? Give me the exact matrix components."

**S (Follow-Up Answer 3)**: "The View Matrix transforms world coordinates to view coordinates. The top-left $3 \times 3$ portion of the View Matrix represents the camera's orientation. 
Since the View Matrix is the transpose of the camera's rotation basis, the world-space camera vectors are stored in the **rows** of the matrix:
$$V = \begin{bmatrix} R_x & R_y & R_z & 0 \\ U_x & U_y & U_z & 0 \\ F_x & F_y & F_z & 0 \\ T_x & T_y & T_z & 1 \end{bmatrix}$$
* **Row 0** ($V_{00}, V_{01}, V_{02}$) is the camera's **Right** vector $\vec{R}$.
* **Row 1** ($V_{10}, V_{11}, V_{12}$) is the camera's **Up** vector $\vec{U}$.
* **Row 2** ($V_{20}, V_{21}, V_{22}$) is the camera's **Forward** vector $\vec{F}$.
To move the camera forward in world space based on keyboard input, we scale this vector by speed and delta time:
$$\vec{P}_{\text{new}} = \vec{P}_{\text{old}} + (\vec{F} \times \text{Speed} \times dt)$$
This keeps movement aligned with where the camera is looking."

---

### Topic 2: Quadtree Traversal & Memory Lifecycle

**T**: "In [QuadTree.h:L215](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/QuadTree.h#L215), you clear the tree and rebuild it from scratch every frame. Why do you do this instead of updating object positions incrementally? Explain the performance implications."

**S**: "Rebuilding the tree every frame is a conscious performance trade-off. 
If we updated object positions incrementally, we would have to check if an object has crossed a quadrant boundary, remove it from the old node, and insert it into a new node. This requires complex tree traversal and memory re-allocations on the CPU.
Because our scene contains a small number of dynamic game objects, clearing the tree (`Clear()`) and re-inserting them (`Insert()`) takes under a microsecond. This is much faster than running incremental updates, ensures the tree is always structurally correct, and avoids memory leaks."

**T (Follow-Up 1)**: "If clearing the tree causes vectors in the child nodes to resize and reallocate memory frequently, does that lead to CPU memory fragmentation?"

**S (Follow-Up Answer 1)**: "Yes, frequent vector de-allocations can cause memory fragmentation. To mitigate this, `Clear(Node* node)` in our codebase only calls `elements.clear()`. This empties the vector but keeps its allocated capacity intact. When objects are re-inserted in the next frame, the vector reuses the same memory block without triggering new heap allocations. New allocations only occur if a node receives more elements than its previous peak capacity."

**T (Follow-Up 2)**: "What is the computational complexity of querying your Quadtree compared to checking all objects against the frustum individually? When does the Quadtree become slower than brute force?"

**S (Follow-Up Answer 2)**: "
* The complexity of brute-force frustum culling is $\mathcal{O}(N)$, where we check all $N$ objects every frame.
* The average complexity of a Quadtree query is $\mathcal{O}(\log N)$, as we cull entire branches at once.
However, if the camera frustum is very wide and covers the entire scene, or if the scene is extremely dense and all objects are concentrated in a single node, the Quadtree must check all nodes and elements anyway. In this case, the quadtree is slower than brute force due to the overhead of tree traversal and recursive function calls."

**T (Follow-Up 3)**: "How do you perform frustum-to-Quadtree node intersection testing? Explain the mathematics of checking a 2D Axis-Aligned Bounding Box (AABB) of a quadtree node against the 3D frustum projected onto the horizontal plane ($XZ$-plane)."

**S (Follow-Up Answer 3)**: "The 3D camera frustum is defined by 6 planes. Each plane is represented by a normal vector $\vec{N} = (A,B,C)$ and a distance constant $D$:
$$A \cdot x + B \cdot y + C \cdot z + D = 0$$
Since our Quadtree partitions the horizontal $XZ$-plane, we project the box of each node onto the horizontal plane, converting it to a 2D AABB defined by minimum coordinates $(x_{\text{min}}, z_{\text{min}})$ and maximum coordinates $(x_{\text{max}}, z_{\text{max}})$. 
To check if the AABB is outside a frustum plane, we locate the **p-vertex** (the corner of the AABB furthest along the direction of the plane normal $\vec{N}$):
$$\vec{P}_{\text{corner}.x} = (N_x \ge 0) ? x_{\text{max}} : x_{\text{min}}$$
$$\vec{P}_{\text{corner}.z} = (N_z \ge 0) ? z_{\text{max}} : z_{\text{min}}$$
We evaluate the plane equation for this corner. If:
$$N_x \cdot \vec{P}_{\text{corner}.x} + N_z \cdot \vec{P}_{\text{corner}.z} + D < 0$$
the entire node is outside the frustum plane. We can immediately cull the node and all its children."

---

### Topic 3: Tessellation HS LOD Calculations

**T**: "In [TessellationHS.hlsl:L39](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/TessellationHS.hlsl#L39), how are the edge and inside factors calculated? Show me the exact math used to scale the factors. What happens if adjacent patches calculate different edge factors?"

**S**: "In the patch constant phase, we compute the midpoint of each edge of the patch. We calculate the distance from the camera to the midpoint, clamp it to our range ($[1.0, 10.0]$ meters), and linearly interpolate to find the factor:
```hlsl
float t = (distance - MIN_TESS_DISTANCE) / (MAX_TESS_DISTANCE - MIN_TESS_DISTANCE);
float tessellationFactor = lerp(MAX_TESS_FACTOR, MIN_TESS_FACTOR, t);
```
If two adjacent patches calculated different edge factors at their shared boundary, the Tessellator would generate mismatching vertices along that boundary, resulting in visible holes in the mesh (**edge cracking**).
By calculating factors at the **edge midpoints** instead of the patch centers, both patches share the same midpoint coordinate and normal. They will calculate the exact same edge factor, preventing edge cracking."

**T (Follow-Up 1)**: "What partitioning mode did you select in your Hull Shader to prevent geometry popping as these factors change?"

**S (Follow-Up Answer 1)**: "We configured our Hull Shader with `[partitioning("fractional_odd")]` in [TessellationHS.hlsl:L76](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/TessellationHS.hlsl#L76). 
Under fractional partitioning, the Tessellator generates vertices along the edges at fractional positions. As the tessellation factor changes (due to camera movement), new vertices are not introduced suddenly. Instead, they morph smoothly from the existing vertices, preventing sudden pops in geometry detail."

**T (Follow-Up 2)**: "How does the Hull Shader pass control points to the Domain Shader? In `TessellationHS.hlsl`, you have `outputcontrolpoints(3)`. Can a Hull Shader output more control points than it inputs? When is this useful?"

**S (Follow-Up Answer 2)**: "Yes, a Hull Shader can output a different number of control points (up to 32) than it receives as input. 
The Hull Shader control point phase (`main`) runs once for each output control point, using `SV_OutputControlPointID` to calculate its properties.
This is useful when converting representations—for example, converting a simple 3-point triangle patch into a 9-point Bezier patch. The Domain Shader can then evaluate these extra control points to calculate smooth curves."

**T (Follow-Up 3)**: "Direct3D 11 tessellation has hardware limitations. What is the maximum Tessellation Factor supported by the hardware, and how does the GPU map fractional factors to triangle subdivision layouts?"

**S (Follow-Up Answer 3)**: "The maximum tessellation factor supported by D3D11 hardware is **$64.0$**. If we calculate a factor larger than 64.0, the hardware clamps it to 64.0. 
When using `fractional_odd` partitioning, the computed factor $k$ is rounded up to the nearest odd integer $n \in \{3, 5, \dots, 63, 65\}$. 
The tessellator subdivides the triangle edge into $n-2$ segments of equal scale, and two smaller segments at the outer ends. As $k$ increases, these two outer segments grow from size 0 until they match the scale of the inner segments. At that point, the layout matches the next odd integer, ensuring smooth transitions without popped vertices."

---

### Topic 4: Domain Shader Phong Smoothing Math

**T**: "Trace the mathematical steps in [TessellationDS.hlsl:L46-77](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/TessellationDS.hlsl#L46) that project a vertex onto a tangent plane. How does this make a blocky sphere round?"

**S**: "First, we compute the flat, linearly interpolated position using barycentrics:
`float3 linearPosition = patch[0].worldPosition * barycentricCoords.x + ...`
Next, we project this flat coordinate onto the tangent planes defined by the control points $P_i$ and normal vectors $N_i$:
```hlsl
float3 ProjectToPlane(float3 position, float3 planePoint, float3 planeNormal)
{
    float3 toPosition = position - planePoint;
    float distanceToPlane = dot(toPosition, planeNormal);
    return position - distanceToPlane * planeNormal;
}
```
We project `linearPosition` onto the tangent planes of all three vertices:
* `proj0 = ProjectToPlane(linearPosition, P0, N0)`
* `proj1 = ProjectToPlane(linearPosition, P1, N1)`
* `proj2 = ProjectToPlane(linearPosition, P2, N2)`
We average these projected points using barycentric coordinates to find `phongPosition`. Because the normal vectors point outward from the sphere's center, projecting onto their tangent planes offsets the new vertices outward, creating a spherical curve.
We then blend this curved position back by 75% to round out the sharp corners:
`output.worldPosition = lerp(linearPosition, phongPosition, PHONG_ALPHA);`"

**T (Follow-Up 1)**: "How do you calculate the normal vector for these newly generated vertices in the Domain Shader? Do you apply the Phong projection equations to the normals as well?"

**S (Follow-Up Answer 1)**: "No, the Phong smoothing projection equations are only applied to the vertex positions. For the normals, we use standard linear interpolation of the control point normals using the barycentric coordinates, and normalize the result:
```hlsl
output.worldNormal = normalize(
    patch[0].worldNormal * barycentricCoords.x + 
    patch[1].worldNormal * barycentricCoords.y + 
    patch[2].worldNormal * barycentricCoords.z
);
```
Applying Phong projection math to normals is not necessary for lighting; linear interpolation of vertex normals across the patch surface is sufficient and computationally cheaper."

**T (Follow-Up 2)**: "Why do you apply the Phong projection equations in **World Space** in the Domain Shader instead of **Clip Space** (after multiplying by the View-Projection matrix)?"

**S (Follow-Up Answer 2)**: "We must perform the projections in world space because the projection equations rely on dot products:
`dot(toPosition, planeNormal)`
Dot products measure angles and distances between vectors. 
If we performed this in clip space, the perspective projection (which scales coordinates non-linearly using $1/Z$) would distort the angles and normals, making the dot products mathematically incorrect and warping the sphere shape. Running the projections in world space ensures the physical curvature is calculated correctly before the perspective transform is applied."

**T (Follow-Up 3)**: "Explain the mathematical relationship between PN-Triangles (Point-Normal Triangles) and Phong Tessellation. How does Phong Tessellation differ from displacement mapping, and why is it preferred when we don't have a heightmap?"

**S (Follow-Up Answer 3)**: "
* **PN-Triangles** construct a curved surface across a triangle by building a cubic Bezier patch. This approach uses 10 control points for positions and 6 control points for normals, requiring significant computation to evaluate the Bezier equations in the Domain Shader.
* **Phong Tessellation** is an approximation that runs much faster. Instead of evaluating Bezier patches, it performs three linear projections onto the vertex tangent planes and interpolates the results.
* **Displacement Mapping** uses a heightmap texture to offset vertex positions along normal vectors, which requires a pre-authored texture.
Phong Tessellation is a purely geometric, mathematical approach that rounds out polygonal shapes (like low-poly spheres or cylinders) automatically without requiring any extra texture data or high-overhead Bezier math."
