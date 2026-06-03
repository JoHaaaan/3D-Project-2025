# Study Guide 4: Deferred Rendering & G-Buffer Design

This guide explains the Deferred Shading pipeline: comparing it to Forward Shading, designing G-Buffers, allocating pixel formats, packing geometric attributes, managing resource binding hazards, and writing the lighting Compute Shader.

---

## 1. Forward vs. Deferred Shading Pipeline

The primary challenge in real-time 3D graphics is calculating lighting from many light sources.

```
FORWARD PIPELINE:
Vertex Shader -> Rasterizer -> Pixel Shader (Draw Mesh + Calculate All Lights) -> Render Target
* Shading occurs for every rasterized fragment, even if overwritten later (High Overdraw).

DEFERRED PIPELINE:
Pass 1: Vertex Shader -> Rasterizer -> Pixel Shader -> G-Buffer (Pack Positions, Normals, Albedo)
Pass 2: Lighting Compute Shader (Bind G-Buffer SRVs + Calculate Lighting) -> Render Target
* Lighting is calculated exactly once per screen pixel for visible surfaces.
```

### Computational Complexity Analysis
* **Forward Shading**:
  $$\text{Complexity} = \mathcal{O}(F \cdot L)$$
  where $F$ is the number of rasterized fragments (including overdraw) and $L$ is the number of lights. If a pixel is overwritten 4 times (overdraw factor of 4), the GPU runs expensive lighting math 4 times for that pixel, only to display the final result.
* **Deferred Shading**:
  $$\text{Complexity} = \mathcal{O}(F) + \mathcal{O}(P \cdot L)$$
  where $P$ is the number of screen pixels ($1024 \times 576 = 589,824$ threads in our project). Shading complexity is decoupled from scene geometry, allowing us to support hundreds of lights.

---

## 2. G-Buffer Layout and Memory Bandwidth

To calculate lighting in screen space during the second pass, the GPU needs access to the material and geometric properties of each visible pixel. We store these attributes in a collection of screen-sized textures called the **Geometry Buffer (G-Buffer)**.

Our G-Buffer is defined in [GBufferD3D11.h](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/GBufferD3D11.h):

| Render Target | Format | R Channel | G Channel | B Channel | A Channel |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `albedoRT` (slot 0) | `R16G16B16A16_FLOAT` | Diffuse R | Diffuse G | Diffuse B | Ambient Strength |
| `normalRT` (slot 1) | `R16G16B16A16_FLOAT` | Packed Normal X | Packed Normal Y | Packed Normal Z | Specular Strength |
| `positionRT` (slot 2) | `R16G16B16A16_FLOAT` | World Position X | World Position Y | World Position Z | Specular Exponent |

### Bandwidth and Selection of `R16G16B16A16_FLOAT`
Using traditional 8-bit formats like `R8G8B8A8_UNORM` clamps values to the $[0.0, 1.0]$ range and has only 256 discrete steps. 
* **Normals** contain negative coordinates (range $[-1.0, 1.0]$). Compressing them into 8 bits leads to precision errors, causing blocky lighting artifacts (banding).
* **World Positions** can scale far beyond $1.0$ (e.g., $X = 354.2$, $Z = -102.5$). 
We use `DXGI_FORMAT_R16G16B16A16_FLOAT` to store these values accurately. Each pixel in our G-Buffer writes 24 bytes ($8 \text{ bytes per target} \times 3$). At $1024 \times 576$ resolution and 60 FPS, this translates to:
$$1024 \times 576 \times 24 \text{ bytes} \times 60 \text{ FPS} = 849.3 \text{ MB/sec of write bandwidth}$$

---

## 3. The Geometry Pass (Writing G-Buffer)

In `Main.cpp` at line 636, we begin the geometry pass:
1. **Clear G-Buffers**: We clear the three render target views and the main depth buffer:
   ```cpp
   float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
   context->ClearRenderTargetView(albedoRTV, clearColor);
   context->ClearRenderTargetView(normalRTV, clearColor);
   context->ClearRenderTargetView(positionRTV, clearColor);
   context->ClearDepthStencilView(myDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
   ```
2. **Bind Render Targets**: We bind the G-Buffer targets and the depth-stencil view:
   ```cpp
   ID3D11RenderTargetView* rtvs[3] = { albedoRTV, normalRTV, positionRTV };
   context->OMSetRenderTargets(3, rtvs, myDSV);
   ```
3. **Draw**: We run the shaders. In the pixel shader `PixelShader.hlsl`, instead of writing a single color, we output a structure containing our target variables:
   ```hlsl
   struct PS_OUTPUT
   {
       float4 albedo   : SV_Target0;
       float4 normal   : SV_Target1;
       float4 position : SV_Target2;
   };
   ```

---

## 4. The Lighting Pass (Compute Shader)

Once geometry is written, we unbind the G-Buffer render targets:
```cpp
ID3D11RenderTargetView* nullRTVs[3] = { nullptr, nullptr, nullptr };
context->OMSetRenderTargets(3, nullRTVs, nullptr); // Important: unbind DSV too
```
This is required to avoid read/write binding conflicts. We then bind the G-Buffer textures as input **Shader Resource Views (SRVs)** to the Compute Shader:
```cpp
ID3D11ShaderResourceView* srvs[3] = { albedoSRV, normalSRV, positionSRV };
context->CSSetShaderResources(0, 3, srvs);
context->CSSetUnorderedAccessViews(0, 1, &lightingUAV, nullptr); // Output texture
context->CSSetShader(lightingShader, nullptr, 0);
```

### Thread Grid Calculation
In `LightingCS.hlsl`, we arrange our execution threads in blocks of $16 \times 16$:
`[numthreads(16, 16, 1)]`
To cover our $1024 \times 576$ screen, we compute the number of thread groups to dispatch:
$$\text{GroupsX} = \frac{1024}{16} = 64, \quad \text{GroupsY} = \frac{576}{16} = 36$$
`context->Dispatch(64, 36, 1);`

Inside the shader, each thread queries its global pixel coordinate using `SV_DispatchThreadID`:
```hlsl
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int2 pixelCoords = DTid.xy;
    
    // Load data from G-Buffer using direct texel loading (no UV sampling)
    float4 albedo = gAlbedo.Load(int3(pixelCoords, 0));
    float3 normal = gNormal.Load(int3(pixelCoords, 0)).xyz;
    float3 worldPos = gPosition.Load(int3(pixelCoords, 0)).xyz;
    
    // Unpack normal vector from [0, 1] back to [-1, 1]
    normal = normalize(normal * 2.0f - 1.0f);
    
    // Run Blinn-Phong lighting equations for each light source...
    float3 finalColor = CalculateLighting(worldPos, normal, albedo);
    
    gOutput[pixelCoords] = float4(finalColor, 1.0f);
}
```

---

## 5. Optimization: Reconstructing World Position from Depth

In high-performance engines, storing the full $XYZ$ world position in a separate G-Buffer target (`positionRT`) is avoided because it takes 8 bytes per pixel and wastes bandwidth. Instead, we can reconstruct the world-space position using only the **Depth Buffer** ($Z$) and the camera's **Inverse View-Projection Matrix ($M^{-1}$)**.

1. **Calculate UV Coordinates**: Find the pixel's location in the range $[0.0, 1.0]$:
   $$u = \frac{\text{pixelCoords}.x}{\text{screenWidth}}, \quad v = \frac{\text{pixelCoords}.y}{\text{screenHeight}}$$
2. **Compute Normalized Device Coordinates (NDC)**: Map the UV coordinates and depth to the $[-1, 1]$ range:
   $$x_{\text{ndc}} = u \times 2.0 - 1.0$$
   $$y_{\text{ndc}} = (1.0 - v) \times 2.0 - 1.0$$
   $$z_{\text{ndc}} = \text{depthSample}$$
3. **Unproject to World Space**: Multiply the NDC vector by the inverse View-Projection matrix:
   $$\vec{P}_{\text{clip}} = (x_{\text{ndc}}, y_{\text{ndc}}, z_{\text{ndc}}, 1.0)$$
   $$\vec{P}_{\text{temp}} = \vec{P}_{\text{clip}} \cdot M^{-1}$$
   $$\vec{P}_{\text{world}} = \frac{\vec{P}_{\text{temp}.xyz}}{\vec{P}_{\text{temp}.w}}$$
This technique removes `positionRT` completely, reducing our geometry pass write bandwidth by 33%.

---

## Teacher Presentation Tips 🎓

* **Why must we clear the G-Buffer render targets every frame?**:
  * *Answer*: Render targets preserve their pixel values from the previous frame. If we do not clear them, areas where no new geometry is drawn will display old, stale position and normal data. The compute shader will run lighting math on these old values, creating visual glitches and trails across the screen.
* **Explain why G-Buffer textures use `Load()` instead of `Sample()` in the Compute Shader**:
  * *Answer*: `Sample()` takes normalized floating-point UV coordinates ($[0,1]$) and uses the GPU's texture filtering unit to perform bilinear interpolation. This is useful for meshes, but deferred shading maps 1:1 with screen pixels. `Load()` queries exact integer pixel coordinates (e.g., $x=520, y=300$), bypassing UV interpolation to access the raw data quickly.
* **What is the purpose of unbinding G-Buffer RTVs before binding them as SRVs?**:
  * *Answer*: Direct3D 11 enforces read-write resource safety. If a resource is bound as an active Render Target View (RTV), it is flagged as an output. If we attempt to bind it as a Shader Resource View (SRV) input in the compute shader without unbinding it first, the D3D11 runtime flags a pipeline hazard and binds the input slot to `nullptr`, causing our lighting shader to read empty data.
* **How does Deferred Shading handle MSAA (Multi-Sample Anti-Aliasing)?**:
  * *Answer*: MSAA is difficult to implement in deferred shading. Since the G-Buffer stores raw geometric properties per pixel, averaging positions or normals at polygon edges generates invalid values (e.g., averaging normal $(1,0,0)$ and normal $(-1,0,0)$ results in normal $(0,0,0)$). To support MSAA, we must run the lighting compute shader for every individual sub-sample rather than once per pixel, which increases shading costs. Most deferred engines use post-processing anti-aliasing techniques like FXAA or TAA instead.
* **Why do we store the specular exponent divided by 256 in the G-Buffer?**:
  * *Answer*: Specular exponents in Blinn-Phong lighting typically range from $1.0$ to $256.0$ (representing surface smoothness). G-Buffer targets clamp variables to the $[0,1]$ range if they use normalized integer formats. By dividing the exponent by 256.0 before writing, we scale the range down to $[0.0, 1.0]$. In the compute shader, we multiply the value by 256.0 to reconstruct the original specular exponent.
