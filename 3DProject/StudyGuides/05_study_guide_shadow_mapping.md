# Study Guide 3: Shadow Mapping & Shadow Map Arrays

This guide explains how shadows are generated in this project: rendering depth from the light's perspective, configuring the shadow map texture array, rendering to individual array slices, implementing Percentage Closer Filtering (PCF), and applying depth bias to solve shadow acne.

---

## 1. Core Concept of Shadow Mapping

Shadow mapping is a two-pass rendering technique:
1. **Shadow Pass**: Render the scene from the light source's perspective. Instead of outputting color, we only output depth to a depth-stencil texture. This records the distance from the light to the closest surfaces.
2. **Lighting Pass**: When shading a pixel on a surface, we transform its world position into the light's perspective (clip space). We perform a depth comparison:
   * Let $d_{\text{current}}$ be the distance from the pixel to the light.
   * Let $d_{\text{map}}$ be the distance stored in the shadow map at the matching coordinate.
   * If $d_{\text{current}} > d_{\text{map}}$, another surface is closer to the light than the current pixel, meaning the current pixel is **in shadow**. Otherwise, it is **lit**.

---

## 2. Light View-Projection Matrix

In [LightManager.cpp:L73](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightManager.cpp#L73), the light-space view-projection matrix is constructed:
* **Point/Spotlights (Perspective)**: Since light propagates outward from a point, we use a perspective projection:
  `XMMatrixPerspectiveFovLH(XMConvertToRadians(90.f), 1.0f, 0.1f, 40.f)`
  The view matrix is built using:
  `XMMatrixLookAtLH(pos, pos + dir, up)`
* **Directional Lights (Orthographic)**: (If active) would use an orthographic projection since directional light rays are parallel.

---

## 3. The Shadow Map Array Setup

Instead of creating a separate depth texture for each light (which would require switching textures and bindings constantly), this project creates a **Texture Array** of size 4.
In [Main.cpp:L297](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L297):
```cpp
DepthBufferD3D11 shadowMap;
shadowMap.Initialize(device, 2048, 2048, true, 4);
```
This allocates a single 2D texture array on the GPU with 4 slices, each slice being $2048 \times 2048$ pixels.

### The Typeless Format Trick
To enable both writing depth and reading it as a shader resource, we use a typeless texture format:
* **Texture Format**: `DXGI_FORMAT_R24G8_TYPELESS` (allocates 32 bits per texel: 24 bits for red/depth, 8 bits for green/stencil, but no data type is assigned yet).
* **Depth Stencil View (DSV) Format**: `DXGI_FORMAT_D24_UNORM_S8_UINT` (reinterprets the bits as a 24-bit unsigned normalized depth value and 8-bit stencil during write).
* **Shader Resource View (SRV) Format**: `DXGI_FORMAT_R24_UNORM_X8_TYPELESS` (reinterprets the depth channel as a 24-bit float red channel for shader reading, ignoring the stencil channel).

---

## 4. The Shadow Pass Implementation

The shadow pass runs in a loop for each light in `Main.cpp:L571-618`.

### Step-by-Step Execution Sequence
1. **Shaders and Topology**:
   Disable pixel, hull, and domain shaders (as we only write depth, which is handled automatically after the vertex shader):
   ```cpp
   context->IASetInputLayout(inputLayout.GetInputLayout());
   context->VSSetShader(vShader, nullptr, 0);
   context->HSSetShader(nullptr, nullptr, 0);
   context->DSSetShader(nullptr, nullptr, 0);
   context->PSSetShader(nullptr, nullptr, 0);
   ```
2. **Clear Depth Slice**:
   Get the DSV for the current light slice (`shadowMap.GetDSV(lightIdx)`) and clear it:
   `context->ClearDepthStencilView(shadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);`
3. **Viewport Swapping**:
   Since the shadow map resolution ($2048 \times 2048$) is higher than the window resolution ($1024 \times 576$), we must swap viewports:
   ```cpp
   D3D11_VIEWPORT shadowViewport = { 0.0f, 0.0f, 2048.0f, 2048.0f, 0.0f, 1.0f };
   context->RSSetViewports(1, &shadowViewport);
   ```
4. **Bind the Depth Target (with null RTV)**:
   We render without writing color:
   ```cpp
   ID3D11RenderTargetView* nullRTV = nullptr;
   context->OMSetRenderTargets(1, &nullRTV, shadowDSV);
   ```
5. **Draw Scene Geometry**:
   We update our constant buffer with the light’s view-projection matrix and draw every mesh:
   ```cpp
   MatrixPair shadowData;
   XMStoreFloat4x4(&shadowData.world, XMMatrixTranspose(obj.GetWorldMatrix()));
   XMStoreFloat4x4(&shadowData.viewProj, XMMatrixTranspose(lightVP));
   constantBuffer.UpdateBuffer(context, &shadowData);
   // Perform draw call...
   ```

---

## 5. Solving Shadow Acne and Peter Panning

### Shadow Acne
* **What it is**: Moire-like black stripes across lit surfaces. It occurs because the shadow map resolution is finite. Multiple screen pixels map to the same shadow map texel. Due to precision limits and surface sloping, some screen pixels end up slightly deeper than the stored texel depth, triggering self-shadowing.
* **Solution 1: Hardware Rasterizer Depth Bias**:
  In [Main.cpp:L79](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L79), we configure a shadow rasterizer state:
  ```cpp
  rastDesc.DepthBias = 2000;
  rastDesc.SlopeScaledDepthBias = 2.0f;
  context->RSSetState(shadowRasterizerState);
  ```
  This shifts the rendered depth values slightly away from the light during the shadow pass.
* **Solution 2: Slope-Dependent Shader Bias**:
  In [LightingCS.hlsl:L82](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L82), we subtract a small dynamic bias based on the angle between the surface normal ($\vec{N}$) and the light direction ($\vec{L}$):
  ```hlsl
  float cosTheta = saturate(dot(normal, lightDir));
  float bias = 0.0005f * (sqrt(1.0f - cosTheta * cosTheta) / (cosTheta + 0.0001f));
  bias = clamp(bias, 0.0f, 0.001f);
  depth -= bias;
  ```
  Slanted surfaces need a larger bias because shadow map texels cover larger physical areas on them.

### Peter Panning
* **What it is**: If the depth bias is too large, shadows detach from the objects and appear to float (named after Peter Pan's shadow). We must balance the hardware bias (2000) and shader bias clamp ($0.001$) to eliminate acne without causing shadows to detach.

---

## 6. Percentage Closer Filtering (PCF) and Comparison Sampler

Direct3D 11 provides hardware-accelerated **Percentage Closer Filtering (PCF)** to smooth out jagged shadow borders.

### The Shadow Sampler Configuration
In [Main.cpp:L86](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L86):
```cpp
D3D11_SAMPLER_DESC desc = {};
desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
desc.BorderColor[0] = desc.BorderColor[1] = desc.BorderColor[2] = desc.BorderColor[3] = 1.0f;
desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
```
* **`D3D11_FILTER_COMPARISON_...`**: Instructs the texture unit that when we sample, it shouldn't return the raw depth value. Instead, it compares our lookup depth against the texel values, filters the boolean results (using bilinear interpolation), and returns a single float representing how much of the surrounding area is lit (between $0.0$ and $1.0$).
* **`D3D11_TEXTURE_ADDRESS_BORDER`**: If a pixel falls outside the boundaries of the light's viewport frustum, the sampler returns the `BorderColor` (which is `1.0f`, meaning fully lit). This avoids visual artifacts at the boundaries.
* **`ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL`**: Returns `1.0` if `depth <= shadowMapDepth` (lit), and `0.0` if `depth > shadowMapDepth` (shadowed).

### Compute Shader Sampling
In [LightingCS.hlsl:L88](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L88):
```hlsl
float shadow = shadowMaps.SampleCmpLevelZero(shadowSampler, float3(shadowUV, lightIndex), depth);
```
We specify a 3-component coordinate: the UV coordinates (`shadowUV`), and the slice index of the light (`lightIndex`) in the texture array.

---

## Teacher Presentation Tips 🎓

* **Explain how to bind a single slice of a Texture Array as a Depth Target**:
  * *Answer*: In [DepthBufferD3D11.cpp:L115](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/DepthBufferD3D11.cpp#L115), when arraySize > 1, we populate a `D3D11_DEPTH_STENCIL_VIEW_DESC` structure with `ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY`. We then specify `dsvDesc.Texture2DArray.FirstArraySlice = i;` and `dsvDesc.Texture2DArray.ArraySize = 1;`. This creates $N$ individual `ID3D11DepthStencilView` pointers, each mapping to exactly one slice of the array. During the shadow pass, we bind only that slice's view using `OMSetRenderTargets`.
* **Why do we use SampleCmpLevelZero instead of standard Sample?**:
  * *Answer*: Regular texture sampling is disabled for depth comparison samplers in HLSL because it would interpolate the raw depth values, which is mathematically incorrect for shadow checks. `SampleCmpLevelZero` forces the hardware to compare the depth values of the 4 nearest texels individually, and then interpolate the 0 or 1 result flags, giving us smooth anti-aliased shadow boundaries.
