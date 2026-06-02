# Study Guide 2: Deferred Rendering vs. Forward Rendering & G-Buffer Design

This guide explains the fundamental differences between forward and deferred rendering, details the G-Buffer layout of this project, walks through the initialization of G-Buffer textures, and details how the lighting pass is performed via compute shaders.

---

## 1. Forward vs. Deferred Rendering: Theoretical Comparison

When rendering a 3D scene with $N$ objects and $M$ light sources:

### Forward Rendering
* **How it works**: For each of the $N$ objects, you submit a draw call. The pixel shader calculates the contribution of all $M$ lights for every single rasterized fragment.
* **Complexity**: $\mathcal{O}(N \times M)$ shading operations.
* **Drawback**: **Overdraw**. If object A is drawn, and then object B is drawn directly in front of it, object A’s pixels were shaded and lit, only to be overwritten. Shading is wasted on hidden surfaces.

### Deferred Rendering
* **How it works**: Splitting the rendering pipeline into two distinct passes:
  1. **Geometry Pass (Write)**: Render all objects once. Instead of performing expensive lighting calculations, write raw material properties (diffuse color, normal, position, specularity) into a set of screen-sized textures called the **G-Buffer** (Geometry Buffer).
  2. **Lighting/Shading Pass (Read)**: Run a screen-space shader (in our case, a Compute Shader `LightingCS.hlsl`) that reads from the G-Buffer textures. For each screen pixel, compute the lighting using the unpacked material properties.
* **Complexity**: $\mathcal{O}(N + M)$ operations. The geometry is processed once to fill the G-Buffer, and then the lights are processed once in screen space.
* **Advantages**:
  * Shading is only performed for visible pixels (no overdraw for lighting calculations).
  * Highly scaleable to dozens of dynamic lights.
* **Disadvantages**:
  * High memory bandwidth requirements.
  * Difficult to handle semi-transparent objects (because only the closest surface depth/material is stored in the G-Buffer).
  * Anti-aliasing (like MSAA) is complex to implement.

---

## 2. G-Buffer Layout in this Project

Our G-Buffer is managed by [GBufferD3D11.h](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/GBufferD3D11.h) and holds three render target textures. All three render targets are initialized with **`DXGI_FORMAT_R16G16B16A16_FLOAT`** (64-bit float format: 16 bits per channel for Red, Green, Blue, Alpha). This high-precision float format prevents banding and allows storing values outside the $[0, 1]$ range (like world positions).

| G-Buffer Texture | Channels | Stored Property | Unpacking / Reconstruct Method in `LightingCS.hlsl` |
| :--- | :--- | :--- | :--- |
| **`albedoRT`** (t0) | **RGB** (16f)<br>**A** (16f) | Diffuse Color / Albedo<br>Ambient Strength | `diffuseColor = albedoSample.rgb;`<br>`ambientStrength = albedoSample.a;` |
| **`normalRT`** (t1) | **RGB** (16f)<br>**A** (16f) | World Normal vector<br>Specular Strength | `normal = normalize(normalPacked * 2.0f - 1.0f);`<br>`specularStrength = normalSample.a;` |
| **`positionRT`** (t2) | **RGB** (16f)<br>**A** (16f) | World Position $(X, Y, Z)$<br>Specular Exponent (Exponent / 256.0f) | `worldPosition = positionSample.xyz;`<br>`specularPower = max(specularPacked * 256.0f, 1.0f);` |

> [!NOTE]
> Normals are unit vectors with components in $[-1.0, 1.0]$. To store them in textures, we pack them into the $[0.0, 1.0]$ range in pixel shaders: `packed = normal * 0.5f + 0.5f`. In the lighting pass compute shader, we unpack them using: `normal = normalize(normalPacked * 2.0f - 1.0f)`.

---

## 3. G-Buffer Pipeline Life-Cycle (D3D11 Commands)

### 1. Initialization
In [GBufferD3D11.cpp:L3](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/GBufferD3D11.cpp#L3), each `RenderTargetD3D11` is initialized. This calls `CreateTexture2D` with bind flags:
`texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;`
This allows these textures to be written to as Render Targets in the geometry pass, and read from as Shader Resource Views (SRVs) in the lighting pass.

### 2. Binding as Render Targets
Before drawing geometry, we bind the three G-Buffer Render Target Views (RTVs) and the scene's Depth Stencil View (DSV):
```cpp
void GBufferD3D11::SetAsRenderTargets(ID3D11DeviceContext* context, ID3D11DepthStencilView* dsv)
{
    ID3D11RenderTargetView* rtvs[3] = { albedoRT.GetRTV(), normalRT.GetRTV(), positionRT.GetRTV() };
    context->OMSetRenderTargets(3, rtvs, dsv);
}
```
* **D3D11 Call**: `OMSetRenderTargets` sets the render targets to output pixels from the rasterizer's pixel shader stage.

### 3. Clearing the G-Buffer
Every frame, the G-Buffer must be cleared to prevent old data from bleeding over:
```cpp
void GBufferD3D11::Clear(ID3D11DeviceContext* context, const float clearColor[4])
{
    context->ClearRenderTargetView(albedoRT.GetRTV(), clearColor);
    context->ClearRenderTargetView(normalRT.GetRTV(), clearColor);
    context->ClearRenderTargetView(positionRT.GetRTV(), clearColor);
}
```

---

## 4. The Lighting Pass: Compute Shader Shading

Once the G-Buffer contains the geometry data, we run the lighting pass. In our codebase, this is handled by **`LightingCS.hlsl`** (a Compute Shader).

### Why a Compute Shader?
* It allows us to directly write to any screen pixel using random-access writes via an `RWTexture2D` (Unordered Access View or UAV), bypassing standard output merger restrictions.
* High efficiency through thread grouping. Thread groups of $16 \times 16$ threads match GPU architecture warp sizes, optimizing performance.

### Unpacking the G-Buffer
In [LightingCS.hlsl:L104](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L104), the shader reads from the texture registers `t0`, `t1`, and `t2` using texture loads:
```hlsl
float4 albedoSample = gAlbedo.Load(int3(pixel, 0));
float4 normalSample = gNormal.Load(int3(pixel, 0));
float4 positionSample = gWorldPos.Load(int3(pixel, 0));
```
* **`gAlbedo.Load`** takes integer coordinates (`DTid.xy`) rather than UV coordinates (`0.0f` to `1.0f`), extracting exact texel values at the given screen pixel.

### Lighting Accumulation
The shader loops over all enabled light sources stored in a `StructuredBuffer<LightData> lights` (bound to `t4`).
For each light, it calculates:
1. **Light Direction**: Directional vs. Point/Spotlight.
2. **Distance Attenuation**: Quadratic falloff for point/spotlights:
   $$attenuation = \text{saturate}(1.0 - (\frac{d}{\text{range}}))^2$$
3. **Spotlight Cone Attenuation**: Inside/outside cone angle calculation.
4. **Shadow Multiplier**: Computes whether the pixel is shadowed using shadow maps (described in Study Guide 3).
5. **Blinn-Phong Shading**:
   * **Diffuse (Lambertian)**: $I_{diff} = \max(\vec{N} \cdot \vec{L}, 0) \times \text{color} \times \text{intensity}$
   * **Specular (Blinn-Phong)**: Uses the half-vector $\vec{H} = \text{normalize}(\vec{L} + \vec{V})$.
     $$I_{spec} = (\max(\vec{N} \cdot \vec{H}, 0))^{\text{specularPower}} \times \text{color} \times \text{intensity} \times \text{specularStrength}$$

Finally, it saturates the accumulated lighting and writes the color:
```hlsl
outColor[pixel] = float4(lighting, 1.0f);
```

---

## Teacher Presentation Tips 🎓

* **Be prepared to explain DXGI_FORMAT_R16G16B16A16_FLOAT**: Why did you choose it over standard `R8G8B8A8_UNORM`?
  * *Answer*: Standard 8-bit formats clamp values to $[0.0, 1.0]$ and suffer from precision issues. For normals and positions, we need negative numbers, floating-point coordinates, and high-precision normals to avoid rendering artifacts (like blocky shadows and banding).
* **Explain how G-Buffer is unbound before Compute Shader execution**:
  * *Answer*: In Direct3D 11, a resource cannot be bound as a Render Target (output) and a Shader Resource View (input) at the same time. If we try to bind a G-Buffer texture as an input to the Compute Shader while it is still bound as a Render Target, the D3D11 runtime will force bind it to `nullptr` to prevent read/write conflicts. To avoid this, we must unbind the RTVs before dispatching the Compute Shader.
