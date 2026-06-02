# Study Guide 6: Environment Mapping (Skybox, Texture Cubemaps & Dynamic Reflections)

This guide explains how real-time reflections are implemented: allocating texture cubemaps, configuring six virtual cameras to capture environment perspectives, rendering the scene into the cubemap faces, and sampling the reflection vector in the pixel shader.

---

## 1. Core Concept of Dynamic Environment Mapping

To render a reflective surface (like a chrome sphere) that reflects its surroundings in real-time, the application must capture what the sphere "sees" in all directions.
1. We position six virtual cameras at the center of the reflective object.
2. Each camera points in a different cardinal direction (+X, -X, +Y, -Y, +Z, -Z) with a **90-degree field of view** and a **1:1 aspect ratio**.
3. We render the scene six times (once per camera) into a **texture cube**.
4. When rendering the reflective object, the pixel shader calculates the reflection vector based on the player's view angle and samples the texture cube.

---

## 2. Allocation of `TextureCubeD3D11`

A Direct3D 11 cubemap is represented as a texture array containing exactly six 2D textures.
In [TextureCubeD3D11.cpp:L18](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/TextureCubeD3D11.cpp#L18):

### Texture Description
```cpp
desc.ArraySize = 6;
desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // Crucial flag for cubemaps
```
* **`D3D11_RESOURCE_MISC_TEXTURECUBE`**: Tells the D3D11 runtime that this texture array can be bound to shaders as a 3D texture cube (`TextureCube` in HLSL), enabling 3D directional vector sampling.

### Render Target Views (RTVs)
We create a separate RTV for each of the six faces. This allows rendering to each face individually:
```cpp
D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
rtvDesc.Format = desc.Format;
rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
rtvDesc.Texture2DArray.ArraySize = 1;

for (int i = 0; i < 6; ++i)
{
    rtvDesc.Texture2DArray.FirstArraySlice = i; // Map RTV to specific slice
    device->CreateRenderTargetView(m_textureCube, &rtvDesc, &m_rtvs[i]);
}
```

### Shared Depth Stencil View (DSV)
To perform depth sorting while rendering into the cubemap faces, we create a single, shared depth texture of the same size ($512 \times 512$) and bind it alongside the active face RTV:
`context->OMSetRenderTargets(1, &cubeRTV, m_dsv);`

---

## 3. Six Virtual Cameras and Orientations

To capture a seamless environment, the cameras must be oriented precisely to map to the DirectX cubemap specification.
In [EnvironmentMapRenderer.cpp:L29-52](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/EnvironmentMapRenderer.cpp#L29):
* **FOV**: `fovAngleY = XM_PIDIV2` (90 degrees). A larger FOV would overlap faces; a smaller FOV would leave gaps.
* **Orientations**:
  * Face 0 (+X, Right): Rotate right 90 degrees.
  * Face 1 (-X, Left): Rotate left 90 degrees.
  * Face 2 (+Y, Up): Rotate forward -90 degrees.
  * Face 3 (-Y, Down): Rotate forward 90 degrees, apply $\pi$ roll correction.
  * Face 4 (+Z, Forward): Facing forward (0 degrees).
  * Face 5 (-Z, Backward): Rotate right 180 degrees.

---

## 4. The Environment Render Loop

Dynamic environment map rendering occurs in [EnvironmentMapRenderer.cpp:L54](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/EnvironmentMapRenderer.cpp#L54) before the main geometry pass.

### Execution Sequence
1. Move all six cameras to the reflective object's center:
   `m_cameras[i].SetPosition(objectPosition);`
2. Loop over the six faces (`faceIndex` 0 to 5):
   * Bind the target slice RTV: `context->OMSetRenderTargets(1, &m_rtvs[faceIndex], m_dsv);`
   * Clear the face and depth buffer: `m_cubeMap.ClearFace(context, faceIndex, clearColor);`
   * Set viewport: `context->RSSetViewports(1, &cubeViewport);`
   * Bind forward shaders (`VertexShader.hlsl` and `CubeMapPS.hlsl`).
   * **Crucial Step: Avoid Self-Reflection**: We skip drawing the reflective object itself to prevent a feedback loop (or rendering its interior):
     ```cpp
     if (objIdx == reflectiveObjectIndex)
         continue;
     ```
   * Draw the rest of the scene into the current face.

---

## 5. Shading Dynamic Reflections

When drawing the reflective object in the main Geometry Pass, we bind the cubemap SRV to slot `t1` and run the pixel shader [ReflectionPS.hlsl](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ReflectionPS.hlsl):

```hlsl
TextureCube reflectionTexture : register(t1);
SamplerState samplerState : register(s0);
```

### Reflection Math in HLSL
1. **Incoming View Vector ($\vec{V}$)**: The vector from the camera to the pixel position:
   $$\vec{V} = \text{normalize}(\vec{P}_{\text{world}} - \vec{P}_{\text{camera}})$$
2. **Reflected Vector ($\vec{R}$)**: The path light takes as it bounces off the surface normal ($\vec{N}$):
   $$\vec{R} = \vec{V} - 2(\vec{V} \cdot \vec{N})\vec{N}$$
   In HLSL: `float3 reflectedView = reflect(incomingView, normalizedNormal);`
3. **Sampling**: We look up the color using the 3D reflection direction $\vec{R}$ as the coordinates:
   `float4 sampledValue = reflectionTexture.Sample(samplerState, reflectedView);`

---

## Teacher Presentation Tips 🎓

* **Explain why the dynamic cubemap pass uses forward rendering instead of deferred**:
  * *Answer*: Deferred rendering is optimized for drawing high-resolution screen images with many lights. The environment map has a low resolution ($512 \times 512$) and does not need particle effects or advanced POM. Performing deferred rendering 6 times per frame would require initializing six separate G-Buffers, which is extremely expensive in terms of GPU memory bandwidth. Simple forward rendering is much faster for capturing surrounding color.
* **What is the difference between Texture2D and TextureCube in HLSL?**:
  * *Answer*: `Texture2D` is sampled using 2D UV coordinates. `TextureCube` is sampled using a 3D direction vector representing a ray cast from the center of a cube. The GPU automatically determines which face of the cube the ray intersects and performs bilinear filtering at the intersection point.
