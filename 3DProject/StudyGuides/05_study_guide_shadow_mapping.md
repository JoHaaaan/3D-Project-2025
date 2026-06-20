# Study Guide 5: Shadow Mapping & Shadow Map Arrays

This guide covers shadow mapping implementation: depth-only rendering, configuring light view-projection matrices, sampling shadow maps with comparison states, and implementing point light shadows using Texture arrays.

---

## 1. Depth-Only Rendering (The Shadow Pass)

Before rendering our scene colors, we run a **Shadow Pass** in [Main.cpp:L571](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L571). The goal is to capture the depth of all objects from the perspective of each light source.

```
LIGHT CAMERA PERSPECTIVE
           Light
            / \
           /   \
          /     \
    Mesh 1 (Blocks Light)  --> Depth written to Shadow Map
        |
        v
    Mesh 2 (In Shadow)     --> Fails depth check in Lighting Pass
```

### Pipeline Optimizations
To speed up rendering during the shadow pass, we disable parts of the graphics pipeline:
1. **Unbind Render Target**: We do not calculate or write color values, so we pass a `nullptr` RTV:
   ```cpp
   ID3D11RenderTargetView* nullRTV = nullptr;
   context->OMSetRenderTargets(1, &nullRTV, shadowDSV);
   ```
2. **Disable Pixel Shader**: We set the pixel shader to null:
   `context->PSSetShader(nullptr, nullptr, 0);`
   With no pixel shader bound, the GPU stops execution after rasterization. The rasterizer generates fragments, and the hardware **Early-Z** unit writes the depth value ($Z$) of each fragment directly to the depth buffer (`shadowDSV`), bypassing pixel shading calculations.
3. **Change Viewport**: We set the viewport to match our shadow map resolution ($2048 \times 2048$), which is larger than the screen resolution ($1024 \times 576$) to ensure sharp shadow edges.
4. **Culling State**: We bind a rasterizer state configured with `D3D11_CULL_BACK`. This culls faces pointing away from the light, preventing unnecessary depth tests.

---

## 2. Light View-Projection Matrices & Coordinate Mapping

To render from the light's perspective, we construct a View-Projection matrix for the light source.

### 1. Matrix Setup
* **Spotlights**: Emit light in a cone, requiring a **Perspective Projection**:
  $$P_{\text{spot}} = \text{XMMatrixPerspectiveFovLH}(\text{coneAngle}, 1.0f, \text{nearZ}, \text{farZ})$$
* **Directional Lights**: Emit parallel light rays from an infinite distance, requiring an **Orthographic Projection**:
  $$P_{\text{dir}} = \text{XMMatrixOrthographicLH}(\text{width}, \text{height}, \text{nearZ}, \text{farZ})$$
* **View Matrix**: Built using the light's position and direction vector:
  $$V_{\text{light}} = \text{XMMatrixLookAtLH}(\text{lightPos}, \text{lightPos} + \text{lightDir}, \text{worldUp})$$

We multiply these matrices to get the combined light view-projection transform:
$$M_{\text{light}} = V_{\text{light}} \cdot P_{\text{light}}$$

### 2. Mapping to UV Space
When rendering our main pass, the vertex shader outputs the world position of each vertex. In the lighting shader, we transform this world position into the light's clip space:
$$\vec{P}_{\text{lightClip}} = \vec{P}_{\text{world}} \cdot M_{\text{light}}$$
In clip space, coordinates range from $[-1, 1]$ on the X and Y axes, and $[0, 1]$ on the Z axis. To map these coordinates to UV space to sample the shadow texture, we apply the following transformation:
$$u = 0.5 \times \frac{X_{\text{lightClip}}}{W_{\text{lightClip}}} + 0.5$$
$$v = -0.5 \times \frac{Y_{\text{lightClip}}}{W_{\text{lightClip}}} + 0.5$$
$$z_{\text{ref}} = \frac{Z_{\text{lightClip}}}{W_{\text{lightClip}}}$$
where $z_{\text{ref}}$ represents the distance from the light to our pixel.

---

## 3. Shadow Sampler & Percentage-Closer Filtering (PCF)

To query the shadow map, we bind a comparison sampler state in [SamplerD3D11.cpp](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/SamplerD3D11.cpp).

### Comparison Sampler Configuration
* **Filter**: `D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT`. This enables hardware comparison filtering.
* **ComparisonFunc**: `D3D11_COMPARISON_LESS_EQUAL`.
When sampling with `SampleCmpLevelZero`, the GPU compares our reference depth ($z_{\text{ref}}$) against the depth stored in the shadow map ($z_{\text{map}}$). 
* If $z_{\text{ref}} \le z_{\text{map}}$, the pixel is closer to the light than the blocker. It is lit (returns `1.0`).
* If $z_{\text{ref}} > z_{\text{map}}$, an object blocks the light. The pixel is in shadow (returns `0.0`).

### Percentage-Closer Filtering (PCF)
Sampling a single texel generates hard, jagged edges. To soften shadow edges, we implement PCF by sampling a grid of neighboring texels and averaging the results.

```hlsl
// HLSL PCF Implementation in LightingCS.hlsl
float CalculatePCFShadow(Texture2DArray shadowMaps, SamplerComparisonState shadowSampler, 
                         float3 shadowUV, float depthRef, float texelSize)
{
    float shadow = 0.0f;
    
    // Sample a 3x3 grid around the target shadow texel
    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += shadowMaps.SampleCmpLevelZero(
                shadowSampler, 
                float3(shadowUV.xy + offset, shadowUV.z), // XY coordinates, Z slice
                depthRef
            );
        }
    }
    
    return shadow / 9.0f; // Average the 9 samples
}
```

---

## 4. Point Light Cubemap Shadows (Slices)

Unlike spotlights (which point in a single direction), point lights emit light in all directions, requiring a $360^\circ$ shadow map. We implement this using a **Texture Array** with 6 slices (one for each face of a cube: $+X, -X, +Y, -Y, +Z, -Z$).

### Rendering the Cube Array
In `Main.cpp` at line 583:
1. We define six view matrices centered at the point light source, pointing in the cardinal directions.
2. We bind the shadow map texture array. During each pass, we render the scene from the perspective of one of the 6 views and write the depth to the corresponding array slice using the depth-stencil view index.
3. In `LightingCS.hlsl`, we identify the light index and slice offset to sample:
   ```hlsl
   // Sample point light shadow from the correct slice face
   float3 direction = pixelWorldPos - lightPos;
   float3 absDir = abs(direction);
   int faceIndex = 0;
   // Mathematical face selection based on largest coordinate component...
   float shadow = shadowMaps.SampleCmpLevelZero(shadowSampler, float3(uv, faceIndex), depthRef);
   ```

---

## 5. Shadow Acne & depth bias calculations

Shadow Acne occurs because of precision limits in the shadow map texture. Multiple screen pixels map to a single shadow map texel. When checking depth, the floating-point values alternate due to precision, creating dark banding lines.

```
SHADOW ACNE:
  Pixel surface:       \__________/__________\
  Shadow map texels:   [== 0.45 ==][== 0.46 ==]
  Result:              Acne Acne Lit Lit Acne Acne
```

We resolve this by applying a bias to the depth values during shadow rendering. The D3D11 formula is:
$$\text{Bias} = (\text{DepthBias} \times r) + (\text{SlopeScaledDepthBias} \times \text{MaxSlope})$$
where $r$ is the minimum resolvable depth difference depending on the depth buffer format. 
* **`DepthBias`** shifts all triangles slightly away from the light.
* **`SlopeScaledDepthBias`** scales the bias based on the angle of the triangle relative to the light. Steeper slopes require a larger bias.
* **`DepthBiasClamp`** prevents this bias from scaling too high on extremely steep angles, which would cause 'Peter Panning' (where shadows appear detached from the object base). 
In our project, we tune `DepthBias = 100` and `SlopeScaledDepthBias = 1.5` to match our $2048 \times 2048$ resolution and prevent both acne and Peter Panning.

---

## 6. Step-by-Step Code Walkthrough & Architectural Rationale

This section maps each step of the shadow mapping system to the codebase and details the **engineering rationale** (the "Why") behind every design decision.

### Phase 1: Setup & Initialization (C++ Code)

#### Step 1: Allocating the Shadow Map Texture Array
* **Files:** [Main.cpp:L297-L298](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L297-L298) and [DepthBufferD3D11.cpp:L40-L154](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/DepthBufferD3D11.cpp#L40-L154)
* **What happens:** We allocate `shadowMap` with a resolution of $2048 \times 2048$ and an array size of 4. In `DepthBufferD3D11.cpp`, we allocate the texture as `R24G8_TYPELESS`, creating 4 Depth Stencil Views (`D24_UNORM_S8_UINT`) and 1 Shader Resource View (`R24_UNORM_X8_TYPELESS`).
* **The "Why" (Rationale):**
  * **Why Typeless?** Direct3D 11 enforces a strict pipeline safety rule: a resource cannot be bound to the GPU pipeline as an output (DSV) and an input (SRV) simultaneously. By creating the texture as `TYPELESS`, we allocate the raw memory block but keep the format interpretation open. This allows us to interpret it as a writable Depth Stencil View (`D24_UNORM_S8_UINT`) in the shadow pass, and as a readable Shader Resource View (`R24_UNORM_X8_TYPELESS`) in the lighting pass.
  * **Why a Texture Array of Size 4?** If we used 4 separate textures, the CPU would have to bind and unbind different textures constantly between drawing, causing massive driver overhead. A `Texture2DArray` stores all shadow maps in a single resource. We bind it to the Compute Shader at slot `t3` once, and the shader dynamically indexes it using the light index (`lightIndex`).

#### Step 2: Creating the Rasterizer State with Depth Bias
* **File:** [Main.cpp:L77-L83](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L77-L83)
* **What happens:** We configure a rasterizer state with `DepthBias = 2000` and `SlopeScaledDepthBias = 2.0f`.
* **The "Why" (Rationale):**
  * **Why Depth Bias?** Due to precision limits and discrete texture resolutions, multiple screen pixels map to a single shadow map texel. Some screen pixels will evaluate to being slightly further from the light than the stored depth, creating alternating dark bands (shadow acne).
  * **Why constant vs. slope-scaled?** A constant `DepthBias` shifts all triangles slightly away from the light. However, if a triangle is tilted at a steep angle relative to the light, the discretization errors increase. `SlopeScaledDepthBias` scales this offset dynamically based on the angle of the triangle relative to the light direction, preventing acne on steep slopes without causing Peter Panning (shadow detachment) on flat surfaces.

#### Step 3: Creating the Comparison Sampler
* **File:** [Main.cpp:L85-L100](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L85-L100)
* **What happens:** We create a sampler state configured with `D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT` and `ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL`, setting the border clamping address mode to white (`1.0f`).
* **The "Why" (Rationale):**
  * **Why Comparison Filter?** A standard sampler would return the raw depth value, requiring us to perform manual comparison and manual bilinear filtering in the shader. The comparison sampler delegates this to the GPU hardware: the hardware compares the reference depth with the 4 nearest shadow map texels and interpolates the boolean results (0.0 or 1.0) directly, providing hardware-accelerated PCF for smooth shadow edges.
  * **Why Border Clamp to White?** When a screen pixel falls outside the boundaries of the light's view frustum, its projected light-space coordinates fall outside the standard $[0, 1]$ texture range. Wrapping or clamping would sample edge pixels and create streaky shadow glitches. Setting the border color to white (`1.0f`) returns the maximum possible depth value, causing the shadow test to pass (lit) and preventing ugly shadow borders outside the light cone.

---

### Phase 2: Pass 1 — The Shadow Pass (CPU loop & GPU rendering)

#### Step 4: Setting up the Pipeline for Depth-Only Rendering
* **File:** [Main.cpp:L571-L580](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L571-L580)
* **What happens:** Before rendering objects, we disable the Pixel, Hull, and Domain shaders by setting them to `nullptr`, set the primitive topology to `TRIANGLELIST`, and bind our `shadowRasterizerState`.
* **The "Why" (Rationale):**
  * **Why Disable the Pixel Shader?** The shadow pass only needs to capture the distance (depth) of objects from the light, not their colors or textures. Disabling the pixel shader allows the GPU's hardware **Early-Z** unit to write depth values directly to the depth buffer immediately after rasterization, bypassing the pixel shading stage completely and doubling rendering performance.
  * **Why Disable Hull and Domain Shaders (Tessellation)?** Tessellation splits triangles to add geometric detail when close to the camera. However, shadow maps are read from a distance and filtered, meaning these micro-geometric details do not affect the shadow shape significantly. Disabling the Hull and Domain shaders for the shadow pass prevents the GPU from doing millions of redundant triangle subdivisions, saving massive vertex processing power.
  * **Why Set Topology to `TRIANGLELIST`?** In the geometry pass, we set the topology to `D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST` to feed control patches to the Hull Shader. Since we disabled tessellation for the shadow pass, the input assembler must feed simple triangles to the vertex shader using `TRIANGLELIST`.

#### Step 5: Looping Through Each Light & Configuring Views
* **File:** [Main.cpp:L582-L598](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L582-L598)
* **What happens:** We loop over `ls.size()` (our 4 lights). For each light, we retrieve its specific DSV slice, clear it, set a $2048 \times 2048$ viewport, and bind a `null` Render Target View (`OMSetRenderTargets(1, &nullRTV, shadowDSV)`).
* **The "Why" (Rationale):**
  * **Why a `null` RTV?** Direct3D 11 requires a render target configuration to output data. Since the pixel shader is disabled and we are not writing colors, we pass a `nullptr` RTV to the Output Merger. This explicitly tells the hardware that there are no color attachments, ensuring it performs a pure depth-only write.
  * **Why Viewport size 2048x2048?** Our main screen viewport is $1024 \times 576$, but our shadow maps are $2048 \times 2048$. If we didn't swap the viewport, the GPU would restrict the rasterizer to the bottom-left $1024 \times 576$ region of the shadow map, leaving the remaining texture memory completely empty and mapping the frustum incorrectly.

#### Step 6: Drawing the Scene Geometry from the Light's View
* **File:** [Main.cpp:L600-L617](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L600-L617)
* **What happens:** We get the light's view-projection matrix, update the constant buffer `cb0` (which sends matrices to the vertex shader), bind the mesh buffers, and draw the objects. The GPU writes the depth directly into the current light's shadow map slice.
* **The "Why" (Rationale):** We must draw the scene from the perspective of each light camera to project the vertices into the light's clip space. The vertex shader transforms the 3D model vertices to the light's perspective using `lightVP`, and the rasterizer interpolates their depth to write a complete depth map of the scene.

---

### Phase 3: Pass 2 — The Lighting Pass (Compute Shader)

#### Step 7: Binding the Shadow Map Array and Sampler
* **Files:** [Main.cpp:L852-L853](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L852-L853) and [Main.cpp:L861](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L861)
* **What happens:** We bind the entire shadow map SRV to slot `t3` (`CSSetShaderResources(3, 1, &shadowSRV)`) and the shadow sampler to slot `s1` (`CSSetSamplers(1, 1, &shadowSampler)`), then dispatch the compute shader.
* **The "Why" (Rationale):** The Compute Shader (which runs our deferred lighting calculations) is the consumer of the shadow maps. It needs read access to all the captured depth data to determine whether each pixel is in shadow.

#### Step 8: Transforming G-Buffer World Position to Light Clip-Space
* **File:** [LightingCS.hlsl:L63-L65](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L63-L65)
* **What happens:** Inside the shader, we read the G-buffer world position of the pixel. We multiply it by the current light's view-projection matrix (`light.viewProj`) and perform the perspective divide (`lightSpacePosition.xyz /= lightSpacePosition.w;`) to get coordinates in NDC space.
* **The "Why" (Rationale):** Multiplying a world position by the light's view-projection matrix gives us homogeneous coordinates $(X, Y, Z, W)$ in light-space. In homogeneous space, perspective projection scales coordinates based on their distance ($W$). To get the actual 3D Normalized Device Coordinates (NDC) in the range $[-1, 1]$, we must perform the perspective divide.

#### Step 9: Mapping to UV space
* **File:** [LightingCS.hlsl:L68-L70](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L68-L70)
* **What happens:** We map the NDC coordinates to UV texture coordinates:
  ```hlsl
  shadowUV.x = lightSpacePosition.x * 0.5f + 0.5f;
  shadowUV.y = -lightSpacePosition.y * 0.5f + 0.5f;
  ```
  The variable `depth = lightSpacePosition.z;` represents the **Actual Distance** from the pixel to the light.
* **The "Why" (Rationale):**
  * **Why Scale & Offset?** NDC coordinates range from $-1.0$ (left/bottom) to $+1.0$ (right/top). Texture UV coordinates range from $0.0$ to $1.0$. Scaling by $0.5$ and adding $0.5$ maps the $[-1, 1]$ range linearly to $[0, 1]$.
  * **Why Flip Y?** In D3D clip space, the Y axis points upwards ($+Y$ is up). However, in texture coordinates, the V axis points downwards ($+V$ is down). We negate the Y coordinate before applying the offset to match the texture coordinate layout.

#### Step 10: Computing Shader-Side Depth Bias
* **File:** [LightingCS.hlsl:L81-L85](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L81-L85)
* **What happens:** We calculate a dynamic, slope-dependent shader bias based on the angle of the normal relative to the light direction, and subtract it from our actual depth: `depth -= bias;`.
* **The "Why" (Rationale):** While the hardware rasterizer bias (Step 2) offsets depth, flat surfaces facing the light at very steep, grazing angles can still experience shadow acne because the discretization errors grow exponentially at steep slopes. We calculate a receiver-plane-oriented bias dynamically in the shader using the angle between the normal and the light direction to clean up any remaining acne.

#### Step 11: Performing the Shadow Test
* **File:** [LightingCS.hlsl:L88](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L88)
* **What happens:** We perform the shadow test using:
  ```hlsl
  float shadow = shadowMaps.SampleCmpLevelZero(shadowSampler, float3(shadowUV, lightIndex), depth);
  ```
  The GPU reads the **Stored Distance** from the shadow map at slice `lightIndex`, compares it to our biased actual `depth`, applies PCF filtering, and returns a value between `0.0` (shadow) and `1.0` (lit).
* **The "Why" (Rationale):**
  * **Why `SampleCmp`?** It tells the GPU to perform the depth comparison on all 4 closest texels and interpolate the boolean results (Percentage-Closer Filtering / PCF), generating soft shadow edges.
  * **Why `LevelZero`?** Shadow maps do not contain mipmaps. Calling `SampleCmpLevelZero` forces the GPU to read from the base level (level 0) directly, bypassing the hardware's mip-selection calculations which would fail and trigger compiler warnings.

#### Step 12: Applying Shadows to Shading
* **File:** [LightingCS.hlsl:L183](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L183) and [LightingCS.hlsl:L192](file:///c:/Users/johan/OneDrive/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl#L192)
* **What happens:** We multiply the `shadow` factor ($0.0$ or $1.0$) by our diffuse and specular lighting colors.
* **The "Why" (Rationale):** If the pixel is in shadow (`shadow = 0.0`), multiplying the lighting contributions by `shadow` negates the direct diffuse and specular light, leaving only the ambient light and thus rendering the pixel in shadow.

---

## Teacher Presentation Tips 🎓

* **Why must we disable the pixel shader during the shadow pass?**:
  * *Answer*: If a pixel shader is bound, the GPU runs pixel shading instructions for every rasterized fragment, even though we do not output color values. Disabling the pixel shader allows the GPU to write depth values directly using hardware **Early-Z** optimization, bypassing pixel shading and doubling shadow rendering speeds.
* **Explain the difference between `DXGI_FORMAT_R24G8_TYPELESS` and `DXGI_FORMAT_D24_UNORM_S8_UINT`**:
  * *Answer*: Typeless formats allocate memory but do not define how data bytes are interpreted. We allocate the shadow texture as typeless so we can view it differently in different stages:
    1. During the shadow pass, we bind it using a Depth Stencil View (DSV) with format `D24_UNORM_S8_UINT` to write depth.
    2. During the lighting pass, we bind it using a Shader Resource View (SRV) with format `R24_UNORM_X8_TYPELESS` to sample depth.
* **What is the purpose of setting the shadow sampler's address mode to border color white?**:
  * *Answer*: When a pixel falls outside the boundaries of the light's view frustum, the sampler returns the border color. We set the border color to white (`1.0f`). Since white represents the furthest depth value, checking `depthRef <= shadowMapDepth` returns `true`, and the pixel is lit. This prevents pixels outside the light cone from incorrectly rendering in shadow.
* **Why does a higher shadow map resolution require a different depth bias?**:
  * *Answer*: Higher resolutions reduce the physical size of each texel, reducing the depth difference between adjacent texels. A large depth bias tuned for a $512 \times 512$ map will cause shadows to detach (Peter Panning) on a $2048 \times 2048$ map. We must scale down our depth bias settings as resolution increases.
* **How does the GPU's hardware comparison sampler perform bilinear filtering on shadow comparisons?**:
  * *Answer*: A standard sampler interpolates the depth values of the four nearest texels, then compares the result to the reference depth, which still generates jagged edges. A comparison sampler compares the reference depth against each of the four texels individually, then interpolates the resulting boolean values ($0$ or $1$) to return a smooth gradient factor.
