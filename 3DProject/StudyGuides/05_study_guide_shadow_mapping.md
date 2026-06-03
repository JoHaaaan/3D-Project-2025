# Study Guide 5: Shadow Mapping & Shadow Map Arrays

This guide covers shadow mapping implementation: depth-only rendering, configuring light view-projection matrices, sampling shadow maps with comparison states, and implementing point light shadows using Texture arrays.

---

## 1. Depth-Only Rendering (The Shadow Pass)

Before rendering our scene colors, we run a **Shadow Pass** in [Main.cpp:L571](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L571). The goal is to capture the depth of all objects from the perspective of each light source.

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

To query the shadow map, we bind a comparison sampler state in [SamplerD3D11.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/SamplerD3D11.cpp).

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
