# Study Guide 11: Master FAQ & Exam Preparation Database

This document contains over 50 highly technical questions and detailed answers designed to prepare you for the 1-on-1 presentation/exam with your teacher. These cover overall architecture, deferred rendering, shadow mapping, Phong tessellation, buffers, environment maps, quadtrees, particle systems, normals/POM, and binding/dispatch math.

---

## Part 1: Win32 & Core Direct3D 11 Setup

### Q1: What is the purpose of `_CrtSetDbgFlag` at the start of `wWinMain`?
**Answer**: It configures the MSVC runtime library’s debug heap allocator to track allocated memory blocks. When the program terminates, it checks for memory leaks and outputs details (file, line number, allocation ID) of any unreleased buffers to the Debug console.

### Q2: What is the difference between `ID3D11Device` and `ID3D11DeviceContext`?
**Answer**: 
* `ID3D11Device` represents the GPU adapter and is used to allocate hardware resources (textures, buffers, shaders, state objects). It is thread-safe.
* `ID3D11DeviceContext` represents the pipeline controller and is used to bind those resources, set pipeline states, configure render targets, and dispatch draw calls. It is not thread-safe and executes commands.

### Q3: What is the role of `IDXGISwapChain` in rendering?
**Answer**: It handles double buffering. It owns a back buffer texture (which the GPU renders to) and a front buffer texture (which is currently displayed on the monitor). When `Present` is called, DXGI swaps the pointers of these buffers to display the newly rendered frame without screen tearing.

### Q4: Why is `GetFocus() == window` checked before updating camera rotation?
**Answer**: This checks if the game window is currently the active foreground window. If it isn't (e.g. the player Alt-Tabs out), we disable mouse tracking and cursor centering, preventing the mouse from getting locked and the camera from spinning uncontrollably in the background.

### Q5: How is delta time ($dt$) calculated, and why is it essential for frame-rate independence?
**Answer**: It is computed using `std::chrono::high_resolution_clock` by measuring the time elapsed since the previous frame. It is essential because computing movements using absolute values (like shifting position by `0.1` per frame) would run 10x faster on a 300 FPS PC than on a 30 FPS PC. Multiplying updates by $dt$ guarantees objects move at a constant speed per second regardless of frame rate.

### Q6: What is a local orthonormal basis for a camera?
**Answer**: It is a set of three mutually perpendicular unit vectors pointing along the camera's local axes: `forward` (look direction), `right` (horizontal axis), and `up` (vertical axis). These vectors define the camera's orientation in 3D world space.

### Q7: How are pitch and yaw calculated using quaternions in `CameraD3D11.cpp`?
**Answer**: We construct rotation quaternions: one for yaw (rotating around the global up axis `(0, 1, 0)`) and one for pitch (rotating around the camera's local `right` vector). Multiplying these quaternions creates a combined rotation quaternion, which we use to rotate the camera's `forward`, `right`, and `up` vectors.

### Q8: What does the View Matrix represent?
**Answer**: The View Matrix transforms coordinates from global **World Space** to **Camera Space** (putting the camera at `(0,0,0)` looking down $+Z$). It is constructed using `XMMatrixLookAtLH(cameraPosition, cameraPosition + cameraForward, cameraUp)`.

### Q9: What does the Projection Matrix represent?
**Answer**: The Projection Matrix transforms coordinates from **Camera Space** to **Clip Space** ($[-1,1]$ boundary for X/Y, $[0,1]$ for Z). It scales coordinates to simulate perspective (farther objects appear smaller) and defines the view boundaries using FOV, aspect ratio, and near/far clip planes.

---

## Part 2: Deferred Rendering & G-Buffer Design

### Q10: What is the main performance advantage of Deferred Rendering over Forward Rendering?
**Answer**: Forward rendering shades every fragment that passes the depth test, which leads to **overdraw** when objects overlap (lit pixels get overwritten). Deferred rendering separates geometry rendering from lighting. We only compute lighting once per screen pixel for visible surfaces, reducing shading complexity from $\mathcal{O}(N \times M)$ to $\mathcal{O}(N + M)$ (where $N$ is objects, $M$ is lights).

### Q11: What is stored in each of the three Render Targets in our G-Buffer?
**Answer**:
1. `albedoRT`: Diffuse Color (RGB) + Ambient Strength (A).
2. `normalRT`: World-space normal vector (RGB, packed as $normal \times 0.5 + 0.5$) + Specular Strength (A).
3. `positionRT`: World-space position coordinates (RGB) + Specular Exponent (A, scaled down by 256.0).

### Q12: Why do G-Buffer textures use `DXGI_FORMAT_R16G16B16A16_FLOAT`?
**Answer**: Traditional formats like `R8G8B8A8_UNORM` clamp values to $[0.0, 1.0]$ and suffer from precision issues. For normals and world positions, we need negative values and high floating-point precision to prevent rendering artifacts (like blocky shadows and banding).

### Q13: How is the G-Buffer bound to the pipeline during the geometry pass?
**Answer**: We create an array of three Render Target Views (RTVs) and bind them along with the main depth buffer's depth-stencil view (DSV) using:
`context->OMSetRenderTargets(3, rtvs, dsv);`

### Q14: Why must the G-Buffer RTVs be unbound before the Compute Shader lighting pass?
**Answer**: Direct3D 11 does not allow a resource to be bound as an output (RTV) and an input (SRV) at the same time. If we try to bind a G-Buffer texture as an input to the Compute Shader while it is still bound as a Render Target, the D3D11 runtime will force bind it to `nullptr` to prevent read/write conflicts. To avoid this, we must unbind the RTVs before dispatching the Compute Shader.

### Q15: How does `LightingCS.hlsl` load G-Buffer texels?
**Answer**: It uses the `.Load` instruction on `Texture2D` objects with integer pixel coordinates (`DTid.xy`) rather than floating-point UV coordinates:
`float4 albedoSample = gAlbedo.Load(int3(pixel, 0));`

### Q16: How is the normal vector reconstructed in `LightingCS.hlsl`?
**Answer**: The G-Buffer stores normals packed in the $[0.0, 1.0]$ range. We unpack them back into $[-1.0, 1.0]$ space and normalize the result:
`float3 normal = normalize(normalPacked * 2.0f - 1.0f);`

### Q17: What formula is used to calculate distance-based light attenuation in `LightingCS.hlsl`?
**Answer**: It uses a quadratic falloff formula:
$$attenuation = \text{saturate}(1.0 - (\frac{d}{\text{range}}))^2$$

---

## Part 3: Shadow Mapping & Shadow Map Arrays

### Q18: What is a Shadow Map Array, and why is it used?
**Answer**: A Shadow Map Array is a single 2D texture array containing multiple depth maps (one slice per light). It allows us to bind all shadow maps to the lighting shader at once (register `t3`), avoiding expensive texture swaps in C++ between rendering different lights.

### Q19: Why does the shadow map texture use the format `DXGI_FORMAT_R24G8_TYPELESS`?
**Answer**: Typeless formats allow the same memory block to be viewed differently in different pipeline stages. We bind it as `DXGI_FORMAT_D24_UNORM_S8_UINT` for writing depth in the shadow pass, and as `DXGI_FORMAT_R24_UNORM_X8_TYPELESS` to read depth values in the lighting shader.

### Q20: Why do we swap viewports to $2048 \times 2048$ during the shadow pass?
**Answer**: The shadow map texture has a higher resolution ($2048 \times 2048$) than the screen swap chain ($1024 \times 576$). We must adjust the rasterizer viewport to match the shadow texture dimensions, or the depth buffer will only render to a fraction of the texture.

### Q21: What is Shadow Acne, and how do we solve it using Rasterizer and Shader Biases?
**Answer**: Shadow Acne consists of self-shadowing striping artifacts caused by floating-point precision limits. We solve it with two techniques:
1. **Hardware Bias**: In the shadow pass rasterizer state, we set `DepthBias = 2000` and `SlopeScaledDepthBias = 2.0f` to shift the rendered depth slightly away from the light.
2. **Slope-Dependent Shader Bias**: In the compute shader, we calculate a bias based on the angle between the normal and the light direction:
   `bias = 0.0005f * (sqrt(1.0f - cosTheta * cosTheta) / (cosTheta + 0.0001f))`

### Q22: What is Peter Panning, and what causes it?
**Answer**: It occurs when the depth bias is set too high, causing shadows to detach from their objects and appear to float. We must balance the hardware bias (2000) and shader bias clamp ($0.001$) to eliminate acne without causing Peter Panning.

### Q23: How does the Comparison Sampler work in hardware PCF?
**Answer**: We configure the sampler with `Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT` and `ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL`. When we call `SampleCmpLevelZero`, the GPU compares our reference depth against the four nearest texels, interpolates the boolean results, and returns a value between $0.0$ (fully shadowed) and $1.0$ (fully lit), yielding smooth shadow edges.

### Q24: Why is `D3D11_TEXTURE_ADDRESS_BORDER` with white border color used for the shadow sampler?
**Answer**: If a pixel falls outside the boundaries of the light's frustum, the sampler returns the border color (`1.0f`, meaning fully lit). This prevents pixels outside the light cone from incorrectly rendering in shadow.

---

## Part 4: Phong Tessellation

### Q25: What are the three stages of Direct3D 11 hardware tessellation?
**Answer**: 
1. **Hull Shader (HS)**: Runs per control point and patch, determining tessellation factors (LOD).
2. **Tessellator**: A fixed-function unit that subdivides the patch into a mesh of triangles based on the factors.
3. **Domain Shader (DS)**: Evaluates the subdivided vertices, interpolating position/UV data and applying displacement math.

### Q26: What primitive topology must be set for tessellation?
**Answer**: `D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST`. This tells the Input Assembler to treat every three vertices as control points of a patch rather than a flat triangle.

### Q27: How does the Hull Shader calculate dynamic, distance-based LOD?
**Answer**: In the patch constant phase, we compute the midpoint of each edge of the patch. We calculate the distance from the camera to the midpoint:
* If distance $\le 1.0\text{m}$, `tessFactor = 4.0`.
* If distance $\ge 10.0\text{m}$, `tessFactor = 1.0` (no subdivision).
* Distances in between are linearly interpolated.

### Q28: How does the Hull Shader prevent edge cracking/gaps between adjacent triangles?
**Answer**: By calculating tessellation factors at the **edge midpoints** of the patch. Since adjacent triangles share the exact same edge and midpoint, they will compute identical edge tessellation factors. This ensures their boundary vertices match perfectly.

### Q29: What mathematical inputs are received by the Domain Shader?
**Answer**:
1. The original control points of the patch.
2. The patch constant tessellation factors.
3. The **Barycentric Coordinates** $(u, v, w)$ of the generated vertex, where $u + v + w = 1.0$.

### Q30: Write the tangent plane projection formula used in Phong Tessellation.
**Answer**: Given a flat interpolated position $P_{\text{linear}}$, we project it onto the tangent plane of control point $P_i$ with normal $N_i$:
$$\pi_i(P_{\text{linear}}) = P_{\text{linear}} - \left( (P_{\text{linear}} - P_i) \cdot N_i \right) N_i$$

### Q31: How is the final vertex position calculated in the Domain Shader?
**Answer**: We project $P_{\text{linear}}$ onto the tangent planes of all three control points, yielding `projection0`, `projection1`, and `projection2`. We interpolate these using barycentrics to find $P_{\text{phong}}$:
$$P_{\text{phong}} = u \pi_0(P_{\text{linear}}) + v \pi_1(P_{\text{linear}}) + w \pi_2(P_{\text{linear}})$$
We then blend the flat and curved positions using $\alpha = 0.75$:
$$P_{\text{final}} = \text{lerp}(P_{\text{linear}}, P_{\text{phong}}, 0.75)$$

---

## Part 5: Mesh Loading & Buffer Management

### Q32: Why does an OBJ file contain separate index counts for positions, UVs, and normals?
**Answer**: The OBJ format is designed for storage efficiency. A vertex might reuse a position index while referencing a new UV or normal index. However, the GPU Input Assembler requires a 1:1 mapping: each vertex index must map to a unique set of position, normal, and UV coordinates.

### Q33: How does the `vertexCache` in `OBJParser.cpp` consolidate vertex/index buffers?
**Answer**: The parser constructs a string key (like `"positionIndex/uvIndex/normalIndex"`) for each vertex. It looks it up in a hash map. If the combination already exists, it reuses the cached index. If it is new, it appends a new `Vertex` to the vertex array and stores its index in the cache. This removes redundant vertices.

### Q34: What properties define a `Submesh` in our codebase?
**Answer**: A submesh is defined by a start index offset and the number of indices in the index buffer, along with its specific material parameters (ambient, diffuse, specular, and texture SRVs).

### Q35: How is a static Vertex Buffer initialized in Direct3D 11?
**Answer**: We configure a `D3D11_BUFFER_DESC` with `BindFlags = D3D11_BIND_VERTEX_BUFFER` and `Usage = D3D11_USAGE_DEFAULT`. We pass the CPU data pointer to a `D3D11_SUBRESOURCE_DATA` struct and call `device->CreateBuffer`.

### Q36: How is the local-space bounding box calculated for a mesh?
**Answer**: During initialization, we loop over all vertices to find the minimum and maximum $X, Y, Z$ coordinates. The center is calculated as $\frac{\text{min} + \text{max}}{2}$, and the extents are calculated as $\frac{\text{max} - \text{min}}{2}$.

---

## Part 6: Environment Mapping

### Q37: How is the `ID3D11Texture2D` allocated to act as a Cubemap Render Target?
**Answer**: We set `ArraySize = 6` in the texture description, along with the flag `MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE` and bind flags `D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE`.

### Q38: Why do we have six separate RTVs but only one shared DSV for the cubemap?
**Answer**: We render to each face of the cubemap one by one, requiring six individual Render Target Views. However, we only render to one face at a time, so we can reuse a single, shared depth buffer for all face passes, saving GPU memory.

### Q39: Why does the environment map render loop skip drawing the reflective object itself?
**Answer**: To prevent a feedback loop and avoid rendering the interior geometry of the reflective object. When capturing what the sphere sees, the sphere itself is not visible in its own perspective.

### Q40: How is the reflection vector calculated in `ReflectionPS.hlsl`?
**Answer**: We compute the view direction vector $\vec{V}$ from the camera to the pixel world position, and reflect it around the normal vector $\vec{N}$ using:
$$\vec{R} = \vec{V} - 2(\vec{V} \cdot \vec{N})\vec{N}$$
`float3 reflectedView = reflect(incomingView, normalizedNormal);`

---

## Part 7: Quadtree Culling

### Q41: What is the difference between a node's bounding box and an element's bounding box in the quadtree?
**Answer**: A node's bounding box represents the static horizontal quadrant boundary of that quadtree cell. An element's bounding box represents the bounding box of a dynamic `GameObject` inserted into the tree.

### Q42: How does the Quadtree partition space when a node is subdivided?
**Answer**: It divides the parent node's bounding box center/extents into four new, half-sized bounding boxes on the X and Z axes, creating 4 child nodes.

### Q43: How does the quadtree query speed up frustum culling?
**Answer**: During a query, if the camera frustum does not intersect a node's bounding box, we skip querying that node and all its children. This rejects large sectors of the scene with a single intersection test.

### Q44: Why do we need a `std::unordered_set<T> visited` during a quadtree query?
**Answer**: Large game objects may overlap the boundary of two or more quadrants, so a pointer to them is placed in multiple leaf nodes. The `visited` set ensures that each object is added to the output rendering list only once.

---

## Part 8: GPU Particle System

### Q45: What is Vertex Pulling (or vertex buffer-less drawing) in the particle pipeline?
**Answer**: Instead of binding a vertex buffer, we bind `nullptr` and set the topology to a point list. The Vertex Shader reads directly from a `StructuredBuffer<Particle>` using the system-generated index `SV_VertexID` to retrieve particle data.

### Q46: How does the Geometry Shader expand point list primitives into billboards?
**Answer**: It takes one point input (particle position) and uses the camera's `right` and `up` vectors to calculate four corner vertices in world space facing the camera. It projects them and streams them out as 6 vertices (forming two triangles).

### Q47: What formula is used to fade out particles based on lifetime in `ParticlePS.hlsl`?
**Answer**: We calculate the squared distance of the pixel from the center of the billboard using UVs mapped to $[-1, 1]$. We apply a smooth edge falloff using:
`float alphaMask = 1.0f - smoothstep(0.85f, 1.0f, radiusSquared);`

### Q48: How is the blend state configured for transparent particles?
**Answer**: 
* `SrcBlend = D3D11_BLEND_SRC_ALPHA` (factor = source alpha).
* `DestBlend = D3D11_BLEND_INV_SRC_ALPHA` (factor = $1 - \text{source alpha}$).
* `BlendOp = D3D11_BLEND_OP_ADD`.
This mixes the particle color with the existing screen background color.

---

## Part 9: Normal Mapping & POM Shaders

### Q49: Explain the math behind screen-space derivative TBN calculation in `ComputeTBN`.
**Answer**: We compute derivatives of world position (`ddx(worldPosition)`, `ddy(worldPosition)`) and UVs (`ddx(uv)`, `ddy(uv)`) across the rasterized triangle. We use these differences to solve for the tangent ($\vec{T}$) and bitangent ($\vec{B}$) direction vectors relative to the surface normal ($\vec{N}$), ensuring they align with the texture space.

### Q50: How does Parallax Occlusion Mapping (POM) ray-marching work?
**Answer**: We step along the view vector direction in UV space (`deltaTexCoords`) and compare the current ray depth against the height map value. Once the ray depth exceeds the height map value, we linearly interpolate between the intersection step and the step immediately before it to find the precise intersection point.

### Q51: Why is `SampleGrad` required in POM, and what parameters does it take?
**Answer**: When sampling textures inside loops with modified UVs, the UV coordinates can jump suddenly at triangle boundaries, making the GPU load a tiny, blurry mipmap. `SampleGrad` forces the GPU to use explicit UV gradients (`ddx` and `ddy` calculated before ray-marching) to select the correct mipmap level, keeping textures sharp.

### Q52: Why must the world position coordinates be modified in the G-Buffer for POM surfaces?
**Answer**: POM shifts the texture coordinate inward to simulate depth. If we write the flat polygon position to the G-Buffer, our deferred light calculations will be calculated at the wrong position. We subtract `normal * depthOffset` from the world position before writing it to the G-Buffer to ensure correct screen-space lighting.

---

## Part 10: Binding & Dispatch Math

### Q53: Explain the math behind dispatching a compute shader with threads of `16x16` over a `1024x576` screen.
**Answer**: We divide the screen width and height by the thread group dimensions, rounding up to prevent boundary clipping:
* $X = \frac{1024 + 15}{16} = 64$ groups.
* $Y = \frac{576 + 15}{16} = 36$ groups.
We dispatch these groups using: `context->Dispatch(64, 36, 1);`

### Q54: What does the system-generated value `SV_DispatchThreadID` represent?
**Answer**: It represents the global pixel coordinate of the active thread across the entire execution grid. It is calculated automatically as:
$$\text{DispatchThreadID} = \text{GroupID} \times \text{groupDim} + \text{GroupThreadID}$$
