# Study Guide 9: Normal Mapping & Parallax Occlusion Mapping

This guide explains how fine surface details are simulated: derivative-based TBN matrix derivation, transforming vectors into tangent space, ray-marching heightmaps in Parallax Occlusion Mapping (POM), fixing mipmapping blur with `SampleGrad`, and updating G-Buffer world coordinates.

---

## 1. What is Normal Mapping and Parallax Mapping?

Standard low-poly meshes have flat polygons, which look smooth but lack depth and rough surface textures.
* **Normal Mapping**: Alters the normal vector at each pixel using a normal map texture. This changes how light bounces off the surface, simulating bumps and grooves. However, the surface remains physically flat.
* **Parallax Occlusion Mapping (POM)**: Actually shifts the texture coordinates (UVs) along the view vector by ray-marching a heightmap. This simulates physical displacement: raised areas occlude deep areas, creating parallax shifts when the viewer moves.

---

## 2. On-The-Fly TBN Matrix Derivation (No Vertex Attributes!)

Typically, normal mapping requires storing three vectors per vertex: the normal ($\vec{N}$), tangent ($\vec{T}$), and bitangent ($\vec{B}$). These form the TBN (Tangent, Bitangent, Normal) rotation matrix.

To save vertex memory and bandwidth, this project calculates the TBN matrix **on-the-fly in the pixel shader** using **Screen-Space Derivatives** (`ddx` and `ddy`).
In [NormalMapPS.hlsl:L36](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/NormalMapPS.hlsl#L36):

```hlsl
float3x3 ComputeTBN(float3 worldPosition, float3 worldNormal, float2 uv)
{
    // 1. Calculate rate of change of position and UVs between adjacent screen pixels
    float3 dp1 = ddx(worldPosition);
    float3 dp2 = ddy(worldPosition);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);
    
    // 2. Solve the linear system mapping UV changes to position changes
    float3 dp2perp = cross(dp2, worldNormal);
    float3 dp1perp = cross(worldNormal, dp1);

    float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;
    
    // Flip bitangent for DirectX coordinates
    bitangent = -bitangent;
    
    // 3. Orthonormalize the vectors
    float invmax = rsqrt(max(dot(tangent, tangent), dot(bitangent, bitangent)));
    return float3x3(tangent * invmax, bitangent * invmax, worldNormal);
}
```
* **How it works**: `ddx` and `ddy` are hardware instructions that return the difference of a variable between neighboring horizontal and vertical pixels on the screen. Since three pixels form a rasterized triangle, we can solve for the spatial direction of the texture axes ($\vec{T}$ and $\vec{B}$) relative to the normal ($\vec{N}$) using these derivatives.

---

## 3. Parallax Occlusion Mapping (POM) Ray-Marching

POM traces a ray through a heightmap texture (where black is deep and white is raised) to find where the player's view vector intersects the geometry.

```
 View Vector (V)
   \   .
====\===\================== Zero Height Level
     \   \  <- Step 1
______\___\________________ Height Map Surface
       \ * \  <- Intersection point (*)
________\___\______________ Deepest Level
```

In [ParallaxPS.hlsl:L70](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParallaxPS.hlsl#L70):

1. **Transform View Vector to Tangent Space**:
   We project the world view direction vector into the coordinate system of the polygon:
   `viewDirTangent.x = dot(TBN[0], viewDirWorld); // etc.`
2. **Determine Steps dynamically (LOD)**:
   If we look straight down (high `viewDirTangent.z`), we use few steps (`MIN_LAYERS = 8`). If looking at a steep grazing angle, we use many steps (`MAX_LAYERS = 64`) to prevent artifacts:
   `float numLayers = lerp(MAX_LAYERS, MIN_LAYERS, abs(viewDirTangent.z));`
3. **Ray-Marching Loop**:
   We step along the view vector direction in UV space (`deltaTexCoords`) and compare the current ray depth against the height map:
   ```hlsl
   for (int i = 0; i < 64 && currentLayerDepth < currentDepthMapValue; i++)
   {
       currentTexCoords -= deltaTexCoords;
       currentDepthMapValue = heightTexture.SampleGrad(samplerState, currentTexCoords, gradientX, gradientY).r;
       currentLayerDepth += layerDepth;
   }
   ```
4. **Precision Interpolation (Relaxation)**:
   Once we break out of the loop (ray depth > surface height), we interpolate between the current step and the previous step to locate the precise intersection point:
   ```hlsl
   float weight = afterDepth / (afterDepth - beforeDepth + 0.0001f);
   float2 finalTexCoords = lerp(currentTexCoords, prevTexCoords, weight);
   ```

---

## 4. The Mipmapping Blur Fix (`SampleGrad`)

### The Problem
When sampling textures inside a loop with modified UVs (like POM), the GPU calculates pixel-to-pixel UV differences (`ddx` and `ddy`) to choose a mipmap level. At the edges of POM surfaces, the UV coordinate jumps suddenly. This large jump makes the GPU think the texture is extremely far away, forcing it to load a tiny, blurry $1\times1$ mipmap.

### The Fix
In [ParallaxPS.hlsl:L114](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParallaxPS.hlsl#L114), we compute the UV derivatives **before** modifying the UVs:
```hlsl
float2 gradientX = ddx(input.uv);
float2 gradientY = ddy(input.uv);
```
We then sample all textures inside POM using `SampleGrad` instead of `Sample`:
```hlsl
diffuseTexture.SampleGrad(samplerState, parallaxUV, gradientX, gradientY);
```
This instructs the GPU to ignore the UV changes from ray-marching and use the gradients of the original mesh UVs, keeping the texture sharp.

---

## 5. Adjusting World Positions in the G-Buffer

Because POM simulates depth offset, a pixel on a brick wall appears deeper than its actual polygon position. If we write the flat polygon position to the G-Buffer, our deferred light calculations will be calculated at the wrong position, causing incorrect shadows and lighting.

To fix this, we recalculate the pixel's world position based on the final height displacement:
```hlsl
float depthOffset = depthFactor * HEIGHT_SCALE + DEPTH_BIAS;
float3 adjustedWorldPosition = input.worldPosition - normalizedNormal * depthOffset;
output.Extra = float4(adjustedWorldPosition, specularPacked);
```
This adjusted position is written to `Target2` (Position G-Buffer), ensuring perfect screen-space lighting.

---

## Teacher Presentation Tips 🎓

* **Explain why you discard pixels in Parallax Occlusion Mapping**:
  * *Answer*: In [ParallaxPS.hlsl:L129](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParallaxPS.hlsl#L129), if the shifted UVs exceed the $[0,1]$ range, it means the ray-marched view vector has exited the boundary of the surface polygon. Discarding these fragments prevents ugly texture wrapping and coordinates bleeding at the edges of the box.
* **Why do we subtract normal * depthOffset?**:
  * *Answer*: Because the displacement goes "inward" (into the surface), we move the world position coordinate backward along the surface normal vector.
