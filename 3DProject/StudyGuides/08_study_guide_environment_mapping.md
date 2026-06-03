# Study Guide 8: Environment Maps

This guide covers environment mapping: initializing cubemap textures, configuring rendering view matrices for the 6 cube faces, calculating reflection vectors, and implementing mipmap filtering for rough reflections.

---

## 1. Dynamic Cubemap Texture Allocation

A Cubemap is a specialized texture array containing 6 faces representing the environment surrounding an object. We allocate it dynamically in C++ inside [TextureCubeD3D11.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/TextureCubeD3D11.cpp):

```cpp
D3D11_TEXTURE2D_DESC texDesc = {};
texDesc.Width = 512;
texDesc.Height = 512;
texDesc.MipLevels = 0; // Automatically allocate full mipmap chain
texDesc.ArraySize = 6;  // 6 faces of the cube
texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
texDesc.SampleDesc.Count = 1;
texDesc.Usage = D3D11_USAGE_DEFAULT;
texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
texDesc.CPUAccessFlags = 0;
texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // Flag as cubemap

device->CreateTexture2D(&texDesc, nullptr, &cubeTexture);
```

### Views
* **Render Target Views (RTVs)**: We create an array of 6 RTVs. Each RTV targets a specific face index slice using `D3D11_RENDER_TARGET_VIEW_DESC::Texture2DArray.FirstArraySlice`.
* **Shader Resource View (SRV)**: We create a single SRV with format `D3D11_SRV_DIMENSION_TEXTURECUBE` to bind the entire cubemap to the shaders as one resource.

---

## 2. Multi-Pass Face Rendering (6-Pass Loop)

To generate reflections in real-time, we position six virtual cameras at the center of the reflective object. In [EnvironmentMapRenderer.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/EnvironmentMapRenderer.cpp), we loop through each face and render the scene:

```
                  +--------------+
                  |  +Y (Up)     |
                  |  Face 2      |
   +--------------+--------------+--------------+--------------+
   |  -X (Left)   |  +Z (Front)  |  +X (Right)  |  -Z (Back)   |
   |  Face 1      |  Face 4      |  Face 0      |  Face 5      |
   +--------------+--------------+--------------+--------------+
                  |  -Y (Down)   |
                  |  Face 3      |
                  +--------------+
```

### Face Camera Orientations
Each face camera requires a $90^\circ$ Field of View (`XM_PIDIV2`) and a $1:1$ aspect ratio. The view matrix look-at directions and up-vectors are defined by DirectX coordinates:

| Face Index | Target Direction | Look-At vector | Up Vector |
| :--- | :--- | :--- | :--- |
| **0 ($+X$)** | Right | `(1, 0, 0)` | `(0, 1, 0)` |
| **1 ($-X$)** | Left | `(-1, 0, 0)` | `(0, 1, 0)` |
| **2 ($+Y$)** | Up | `(0, 1, 0)` | `(0, 0, -1)` |
| **3 ($-Y$)** | Down | `(0, -1, 0)` | `(0, 0, 1)` |
| **4 ($+Z$)** | Forward | `(0, 0, 1)` | `(0, 1, 0)` |
| **5 ($-Z$)** | Backward | `(0, 0, -1)` | `(0, 1, 0)` |

### The Shared Depth-Stencil View
We clear the shared depth buffer (`m_dsv`) at the start of rendering **each** face slice:
`context->ClearDepthStencilView(m_dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);`
Because all 6 passes share one depth buffer, the depth values written during the first pass are still present when we switch to the second face. If we do not clear the depth buffer, depth testing will fail, culling geometry on subsequent faces.

---

## 3. Reflection Vector Calculations

To draw reflections, the pixel shader calculates the reflection vector using the camera view direction and the surface normal.

```
       View Vector (I)        Normal (N)       Reflection (R)
                 \                |                /
                  \               |               /
                   \              |              /
  __________________\_____________|_____________/___________
```

### The Reflection Equation
Mathematically, given the incident view vector $\vec{I}$ (from camera to pixel position) and surface normal vector $\vec{N}$:
$$\vec{R} = \vec{I} - 2(\vec{I} \cdot \vec{N})\vec{N}$$

In the pixel shader `ReflectionPS.hlsl`:
```hlsl
// Calculate incident view vector in world space
float3 incidentView = normalize(pixelWorldPos - cameraPosition);

// Calculate reflection vector
float3 reflectDir = reflect(incidentView, normalize(pixelNormal));

// Sample the environment map using the 3D reflection direction vector
float4 reflectedColor = environmentMap.Sample(samplerState, reflectDir);
```

---

## 4. Glossy / Rough Reflections (Mipmap Filtering)

Perfect mirrors are rare. Most reflective materials (like brushed metal) have rough surfaces that scatter reflected light, creating blurry reflections.

We simulate surface roughness using the cubemap's **Mipmap Chain**. 

```
Mip 0 (Full Res: 512x512)   ---> Mirror Reflection (Roughness = 0.0)
Mip 2 (Low Res: 128x128)    ---> Glossy Reflection (Roughness = 0.3)
Mip 5 (Ultra Low: 16x16)    ---> Diffuse/Blurry Reflection (Roughness = 0.8)
```

1. **Mip Generation**: After rendering the 6 faces, we generate mipmaps using the GPU device context:
   `context->GenerateMips(cubeSRV);`
   The GPU downsamples the cubemap, blurring details in lower mipmap levels.
2. **Mip Selection**: In the pixel shader, we sample a specific mipmap level based on the material's roughness using `SampleLevel`:
   ```hlsl
   float mipLevel = roughness * maxMipLevels; // e.g., 0.3 * 9 = level 2.7
   float4 reflection = environmentMap.SampleLevel(samplerState, reflectDir, mipLevel);
   ```
   The GPU interpolates between adjacent mipmap levels, producing a smooth blur effect.

---

## Teacher Presentation Tips 🎓

* **Why must we skip rendering the reflective object itself during the cubemap passes?**:
  * *Answer*: To prevent a feedback loop and avoid rendering the interior geometry of the reflective object. When capturing what the sphere sees, the sphere itself is not visible in its own perspective. If we did not skip it, the camera would render the inside of the sphere, blocking the surrounding scene.
* **Explain how the GPU samples a 3D vector from a cubemap**:
  * *Answer*: The GPU hardware texture unit processes the 3D direction vector $\vec{R} = (x,y,z)$. It finds the component with the largest absolute value to select the target face (e.g., if $+Y$ is largest, it selects the top face). It then projects the remaining coordinates onto that face to calculate standard 2D UV coordinates to sample the texture.
* **What are the performance implications of real-time dynamic cubemaps?**:
  * *Answer*: Dynamic cubemaps are expensive. Rendering the scene 6 times per frame multiplies the vertex count and draw calls by 6. This can severely bottleneck the CPU.
* **How do you optimize dynamic cubemap rendering in production games?**:
  * *Answer*:
    1. **Time-Slicing**: Update only 1 face of the cubemap per frame, updating the full cube over 6 frames.
    2. **Low Resolution**: Render the faces at a reduced resolution (e.g., $128 \times 128$ or $256 \times 256$), which is sufficient for blurry reflections.
    3. **Cull Details**: Skip rendering complex vertex details (like particles, grass, or small clutter) in the cubemap passes.
* **Explain the difference between `Sample` and `SampleLevel` in HLSL**:
  * *Answer*: `Sample` is used in pixel shaders, where the GPU automatically calculates gradients (`ddx`/`ddy`) based on screen pixel differences to select the mipmap level. `SampleLevel` is used to override this, allowing us to specify a specific mipmap level (the third argument) to control the blur.
