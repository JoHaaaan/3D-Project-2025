# Study Guide 7: Normal & Parallax Maps

This guide covers tangent space calculations and texture offset shaders: deriving Tangents and Bitangents, constructing the TBN matrix, implementing Normal Mapping, and writing Ray-Marched Parallax Occlusion Mapping (POM) with hardware gradient sampling.

---

## 1. Normal Mapping & Tangent Space

A mesh's vertex normals represent the overall curvature of the geometry. To add fine details (like cracks in brick walls) without adding vertices, we use **Normal Mapping**.

```
Polygon Surface:     _______________________________ (Flat Geometry)
Normal Map Normals:  / \  |  / \  \ /  |  / \  |  \ /  (Detailed Lighting)
```

Normal maps are textures that store high-resolution surface normals. To keep normal maps reusable across different models, the normal vectors are stored in a local coordinate system called **Tangent Space** (or texture space). In tangent space:
* The $+Z$ axis points along the surface normal direction.
* The $+X$ axis points along the texture horizontal coordinate direction ($U$).
* The $+Y$ axis points along the texture vertical coordinate direction ($V$).

### Unpacking
Normals store coordinates in the $[-1.0, 1.0]$ range. Texture colors are stored in the $[0.0, 1.0]$ range. We unpack the texture sample to reconstruct the original normal vector:
$$\vec{N}_{\text{unpacked}} = 2.0 \times \vec{C}_{\text{sample}} - 1.0$$
Because the unpacked normals point mostly along the $+Z$ axis (the surface normal), normal map textures look light blue.

---

## 2. Deriving Tangents, Bitangents, and the TBN Matrix

To use tangent-space normal maps for lighting, we must transform them into world space. This requires constructing a coordinate transition matrix called the **TBN Matrix**.

```
         Normal (N)
            |
            |
            |____ Tangent (T)
           /
          /
     Bitangent (B)
```

### Mathematical Derivation of Tangent and Bitangent
For a triangle defined by positions $P_0, P_1, P_2$ and UV coordinates $U_0, U_1, U_2$:
1. We calculate edge vectors and UV differences:
   $$\vec{E}_1 = P_1 - P_0, \quad \vec{E}_2 = P_2 - P_0$$
   $$\Delta u_1 = u_1 - u_0, \quad \Delta v_1 = v_1 - v_0$$
   $$\Delta u_2 = u_2 - u_0, \quad \Delta v_2 = v_2 - v_0$$
2. The relationship between edge vectors, UV vectors, and the tangent basis ($\vec{T}, \vec{B}$) is:
   $$\vec{E}_1 = \Delta u_1 \cdot \vec{T} + \Delta v_1 \cdot \vec{B}$$
   $$\vec{E}_2 = \Delta u_2 \cdot \vec{T} + \Delta v_2 \cdot \vec{B}$$
3. We write this as a matrix multiplication:
   $$\begin{bmatrix} \vec{E}_1 \\ \vec{E}_2 \end{bmatrix} = \begin{bmatrix} \Delta u_1 & \Delta v_1 \\ \Delta u_2 & \Delta v_2 \end{bmatrix} \begin{bmatrix} \vec{T} \\ \vec{B} \end{bmatrix}$$
4. Inverting the UV matrix yields the solution for $\vec{T}$ and $\vec{B}$:
   $$\begin{bmatrix} \vec{T} \\ \vec{B} \end{bmatrix} = \frac{1}{\Delta u_1 \Delta v_2 - \Delta u_2 \Delta v_1} \begin{bmatrix} \Delta v_2 & -\Delta v_1 \\ -\Delta u_2 & \Delta u_1 \end{bmatrix} \begin{bmatrix} \vec{E}_1 \\ \vec{E}_2 \end{bmatrix}$$

### Gram-Schmidt Orthonormalization
Due to precision limits and UV stretching, the calculated vectors $\vec{T}$ and $\vec{B}$ might not be perfectly perpendicular to the normal $\vec{N}$. In [NormalMapPS.hlsl](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/NormalMapPS.hlsl), we orthonormalize the basis:
$$\vec{T}' = \text{normalize}(\vec{T} - (\vec{T} \cdot \vec{N})\vec{N})$$
$$\vec{B}' = \vec{N} \times \vec{T}'$$
The resulting T, B, and N vectors are mutually perpendicular unit vectors. The TBN matrix is constructed as:
$$\text{TBN} = \begin{bmatrix} T'_x & B'_x & N_x \\ T'_y & B'_y & N_y \\ T'_z & B'_z & N_z \end{bmatrix}$$

World space normals are calculated by multiplying the tangent normal by the TBN matrix:
$$\vec{N}_{\text{world}} = \text{mul}(\vec{N}_{\text{unpacked}}, \text{TBN})$$

---

## 3. Parallax Mapping (Offset Mapping)

Normal mapping alters lighting angles but keeps the polygon surface flat. **Parallax Mapping** shifts UV coordinates based on the camera view angle and a heightmap to simulate depth, making details (like brick cracks) look recessed.

```
PARALLAX UV OFFSET:
  Camera View Vector (V)
       \
________\___________ Polygon Surface
   |     \
   |      \--> Adjusted UV Coordinate (Samples lower height point)
 Heightmap Profile
```

We transform the camera view vector $\vec{V}$ into tangent space:
$$\vec{V}_{\text{tangent}} = \text{mul}(\vec{V}_{\text{world}}, \text{transpose}(\text{TBN}))$$
The UV offset calculation is:
$$\vec{UV}_{\text{offset}} = \vec{UV}_{\text{orig}} - \left( \frac{\vec{V}_{\text{tangent}.xy}}{\vec{V}_{\text{tangent}.z}} \times \text{height} \times \text{scale} \right)$$
where `height` is sampled from the heightmap, and `scale` controls the depth intensity.

---

## 4. Parallax Occlusion Mapping (POM)

Simple parallax offset mapping fails at steep angles, causing texture stretching. **Parallax Occlusion Mapping (POM)** resolves this by ray-marching through layers of the heightmap in tangent space to find the exact intersection point.

```
PARALLAX OCCLUSION RAY MARCHING:
  Ray steps:    *       *       * (Depth increases)
                \       \       \
  Height profile:________\_______[Intersection Found]_______
```

### POM Shader Implementation in `ParallaxPS.hlsl`
```hlsl
float2 CalculatePOMCoords(float2 uv, float3 viewDirTS, float2 dx, float2 dy)
{
    const float minLayers = 8.0f;
    const float maxLayers = 32.0f;
    
    // Fewer layers when looking straight down, more layers at steep angles
    float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0,0,1), viewDirTS)));
    float layerDepth = 1.0f / numLayers;
    
    float2 p = viewDirTS.xy / viewDirTS.z * gHeightScale;
    float2 deltaTexCoords = p / numLayers;
    
    float2 currentTexCoords = uv;
    float currentLayerDepth = 0.0f;
    
    // Sample heightmap using explicit gradients to prevent mipmap blurring
    float currentDepthMapValue = gHeightMap.SampleGrad(gSampler, currentTexCoords, dx, dy).r;
    
    [loop]
    while(currentLayerDepth < currentDepthMapValue)
    {
        currentTexCoords -= deltaTexCoords;
        currentDepthMapValue = gHeightMap.SampleGrad(gSampler, currentTexCoords, dx, dy).r;
        currentLayerDepth += layerDepth;
    }
    
    // Calculate intersection interpolation
    float2 prevTexCoords = currentTexCoords + deltaTexCoords;
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = gHeightMap.SampleGrad(gSampler, prevTexCoords, dx, dy).r - (currentLayerDepth - layerDepth);
    
    float weight = afterDepth / (afterDepth - beforeDepth);
    return lerp(currentTexCoords, prevTexCoords, weight);
}
```

### The `SampleGrad` Optimization
Normally, the GPU calculates mipmaps automatically based on coordinate differences between adjacent pixels. Inside dynamic loops with branching statements, the GPU cannot evaluate these differences, defaulting to the lowest, most blurry mipmap level. 

To keep textures sharp, we compute coordinate gradients (`ddx`, `ddy`) **before** the ray-marching loop:
```cpp
float2 dx = ddx(uv);
float2 dy = ddy(uv);
```
We pass these gradients to **`SampleGrad`** to force the GPU to load the correct mipmap level inside the loop.

---

## Teacher Presentation Tips 🎓

* **Explain the difference between Normal Mapping and Parallax Occlusion Mapping**:
  * *Answer*: Normal mapping modifies surface normals to alter lighting angles, but the underlying geometry remains flat. Parallax Occlusion Mapping (POM) ray-marches through a heightmap to shift texture coordinates, creating parallax depth effects. POM simulates depth, self-occlusion, and parallax shifts as the camera moves, but the silhouette edge of the polygon remains flat.
* **Why must we transpose the TBN matrix to transform vectors from World Space to Tangent Space?**:
  * *Answer*: The vectors $\vec{T}, \vec{B}, \vec{N}$ form an orthonormal basis, making the TBN matrix orthogonal. A key mathematical property of orthogonal matrices is that their inverse is equal to their transpose ($M^{-1} = M^T$). Transposing is computationally cheaper than calculating a full matrix inversion on the GPU.
* **What causes texture artifacts at polygon edges in POM, and how do we prevent them?**:
  * *Answer*: At steep angles, the POM ray-march can step outside the $[0.0, 1.0]$ boundary of the triangle's UV coordinates, sampling neighboring textures. We prevent this by checking boundary limits inside the pixel shader and clamping or discarding coordinate updates that fall outside the range.
* **How do you calculate soft shadows inside the heightmap with POM?**:
  * *Answer*: Once the intersection point is found, we run a second ray-march from the intersection point towards the light source in tangent space. If the ray hits a higher height value before exiting, the pixel is in shadow relative to the surface details, creating self-shadowing effects.
* **Why do we reconstruct world positions after running POM offsets?**:
  * *Answer*: POM shifts texture coordinates to simulate depth. If we write the original, flat polygon positions to the G-Buffer, deferred lighting calculations will be computed at the wrong depth. We adjust the world position before writing it to the G-Buffer by subtracting `normal * depthOffset` to ensure lighting is calculated at the correct depth.
