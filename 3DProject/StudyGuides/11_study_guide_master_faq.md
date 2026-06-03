# Study Guide 11: Master FAQ & Exam Preparation Database (Ultimate Edition)

This document contains 75 highly technical questions and detailed answers designed to prepare you for the 1-on-1 presentation/exam with your teacher. These cover overall architecture, deferred rendering, shadow mapping, Phong tessellation, buffers, environment maps, quadtrees, particle systems, normals/POM, and binding/dispatch math.

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
**Answer**: This checks if the game window is currently the active foreground window. If it isn't (e.g., the player Alt-Tabs out), we disable mouse tracking and cursor centering, preventing the mouse from getting locked and the camera from spinning uncontrollably in the background.

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

### Q10: How do we construct the LookAt matrix mathematically?
**Answer**: Given camera position $\vec{P}$, target vector $\vec{T}$, and up vector $\vec{U}$:
1. $\vec{F} = \text{normalize}(\vec{T} - \vec{P})$
2. $\vec{R} = \text{normalize}(\vec{U} \times \vec{F})$
3. $\vec{U}' = \vec{F} \times \vec{R}$
The View Matrix $V$ is the inverse of this camera world matrix, which rotates and translates:
$$V = \begin{bmatrix} R_x & U'_x & F_x & 0 \\ R_y & U'_y & F_y & 0 \\ R_z & U'_z & F_z & 0 \\ -(\vec{P} \cdot \vec{R}) & -(\vec{P} \cdot \vec{U}') & -(\vec{P} \cdot \vec{F}) & 1 \end{bmatrix}$$

---

## Part 2: Deferred Rendering & G-Buffer Design

### Q11: What is the main performance advantage of Deferred Rendering over Forward Rendering?
**Answer**: Forward rendering shades every fragment that passes the depth test, which leads to **overdraw** when objects overlap (lit pixels get overwritten). Deferred rendering separates geometry rendering from lighting. We only compute lighting once per screen pixel for visible surfaces, reducing shading complexity from $\mathcal{O}(F \cdot L)$ to $\mathcal{O}(F) + \mathcal{O}(P \cdot L)$ (where $F$ is geometry fragments, $P$ is screen pixels, and $L$ is lights).

### Q12: What is stored in each of the three Render Targets in our G-Buffer?
**Answer**:
1. `albedoRT`: Diffuse Color (RGB) + Ambient Strength (A).
2. `normalRT`: World-space normal vector (RGB, packed as $normal \times 0.5 + 0.5$) + Specular Strength (A).
3. `positionRT`: World-space position coordinates (RGB) + Specular Exponent (A, scaled down by 256.0).

### Q13: Why do G-Buffer textures use `DXGI_FORMAT_R16G16B16A16_FLOAT`?
**Answer**: Traditional formats like `R8G8B8A8_UNORM` clamp values to $[0.0, 1.0]$ and suffer from precision issues. For normals and world positions, we need negative values and high floating-point precision to prevent rendering artifacts (like blocky shadows and banding).

### Q14: How is the G-Buffer bound to the pipeline during the geometry pass?
**Answer**: We create an array of three Render Target Views (RTVs) and bind them along with the main depth buffer's depth-stencil view (DSV) using:
`context->OMSetRenderTargets(3, rtvs, dsv);`

### Q15: Why must the G-Buffer RTVs be unbound before the Compute Shader lighting pass?
**Answer**: Direct3D 11 does not allow a resource to be bound as an output (RTV) and an input (SRV) at the same time. If we try to bind a G-Buffer texture as an input to the Compute Shader while it is still bound as a Render Target, the D3D11 runtime will force bind it to `nullptr` to prevent read/write conflicts. To avoid this, we must unbind the RTVs before dispatching the Compute Shader.

### Q16: How does `LightingCS.hlsl` load G-Buffer texels?
**Answer**: It uses the `.Load` instruction on `Texture2D` objects with integer pixel coordinates (`DTid.xy`) rather than floating-point UV coordinates:
`float4 albedoSample = gAlbedo.Load(int3(pixel, 0));`

### Q17: How is the normal vector reconstructed in `LightingCS.hlsl`?
**Answer**: The G-Buffer stores normals packed in the $[0.0, 1.0]$ range. We unpack them back into $[-1.0, 1.0]$ space and normalize the result:
`float3 normal = normalize(normalPacked * 2.0f - 1.0f);`

### Q18: What formula is used to calculate distance-based light attenuation in `LightingCS.hlsl`?
**Answer**: It uses a quadratic falloff formula:
$$attenuation = \text{saturate}\left(1.0 - \left(\frac{d}{\text{range}}\right)\right)^2$$
This ensures smooth fading to black at the boundary of the light's range.

### Q19: Explain the mathematics behind reconstructing world positions from depth.
**Answer**:
1. Map pixel coordinates to UV space: $u = x/\text{width}$, $v = y/\text{height}$.
2. Convert to NDC space: $x_{\text{ndc}} = u \times 2 - 1$, $y_{\text{ndc}} = (1 - v) \times 2 - 1$, $z_{\text{ndc}} = \text{depthSample}$.
3. Transform homogeneous NDC position using the inverse View-Projection matrix:
   $$\vec{P}_{\text{world}} = \text{mul}(float4(x_{\text{ndc}}, y_{\text{ndc}}, z_{\text{ndc}}, 1.0), \text{InvViewProj})$$
4. Apply perspective divide: $\vec{P}_{\text{final}} = \vec{P}_{\text{world}.xyz} / \vec{P}_{\text{world}.w}$.

---

## Part 3: Shadow Mapping & Shadow Map Arrays

### Q20: What is a Shadow Map Array, and why is it used?
**Answer**: A Shadow Map Array is a single 2D texture array containing multiple depth maps (one slice per light). It allows us to bind all shadow maps to the lighting shader at once (register `t3`), avoiding expensive texture swaps in C++ between rendering different lights.

### Q21: Why does the shadow map texture use the format `DXGI_FORMAT_R24G8_TYPELESS`?
**Answer**: Typeless formats allow the same memory block to be viewed differently in different pipeline stages. We bind it as `DXGI_FORMAT_D24_UNORM_S8_UINT` for writing depth in the shadow pass, and as `DXGI_FORMAT_R24_UNORM_X8_TYPELESS` to read depth values in the lighting shader.

### Q22: Why do we swap viewports to $2048 \times 2048$ during the shadow pass?
**Answer**: The shadow map texture has a higher resolution ($2048 \times 2048$) than the screen swap chain ($1024 \times 576$). We must adjust the rasterizer viewport to match the shadow texture dimensions, or the depth buffer will only render to a fraction of the texture.

### Q23: What is Shadow Acne, and how do we solve it using Rasterizer and Shader Biases?
**Answer**: Shadow Acne consists of self-shadowing striping artifacts caused by floating-point precision limits. We solve it with two techniques:
1. **Hardware Bias**: In the shadow pass rasterizer state, we set `DepthBias = 2000` and `SlopeScaledDepthBias = 2.0f` to shift the rendered depth slightly away from the light.
2. **Slope-Dependent Shader Bias**: In the compute shader, we calculate a bias based on the angle between the normal and the light direction:
   `bias = 0.0005f * (sqrt(1.0f - cosTheta * cosTheta) / (cosTheta + 0.0001f))`

### Q24: What is Peter Panning, and what causes it?
**Answer**: It occurs when the depth bias is set too high, causing shadows to detach from their objects and appear to float. We must balance the hardware bias (2000) and shader bias clamp ($0.001$) to eliminate acne without causing Peter Panning.

### Q25: How does the Comparison Sampler work in hardware PCF?
**Answer**: We configure the sampler with `Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT` and `ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL`. When we call `SampleCmpLevelZero`, the GPU compares our reference depth against the four nearest texels, interpolates the boolean results, and returns a value between $0.0$ (fully shadowed) and $1.0$ (fully lit), yielding smooth shadow edges.

### Q26: Why is `D3D11_TEXTURE_ADDRESS_BORDER` with white border color used for the shadow sampler?
**Answer**: If a pixel falls outside the boundaries of the light's frustum, the sampler returns the border color (`1.0f`, meaning fully lit). This prevents pixels outside the light cone from incorrectly rendering in shadow.

### Q27: How does slope-scaled depth bias relate to triangle slope relative to the light?
**Answer**: The slope is defined as $\tan(\theta)$ where $\theta$ is the angle between the triangle normal and the light direction. As the slope increases (steeper angles), the depth difference across a single shadow map texel expands, requiring a larger bias. The formula is:
$$\text{Bias} = (\text{DepthBias} \times r) + (\text{SlopeScaledDepthBias} \times \text{MaxSlope})$$

---

## Part 4: Phong Tessellation

### Q28: What are the three stages of Direct3D 11 hardware tessellation?
**Answer**: 
1. **Hull Shader (HS)**: Runs per control point and patch, determining tessellation factors (LOD).
2. **Tessellator**: A fixed-function unit that subdivides the patch into a mesh of triangles based on the factors.
3. **Domain Shader (DS)**: Evaluates the subdivided vertices, interpolating position/UV data and applying displacement math.

### Q29: What primitive topology must be set for tessellation?
**Answer**: `D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST`. This tells the Input Assembler to treat every three vertices as control points of a patch rather than a flat triangle.

### Q30: How does the Hull Shader calculate dynamic, distance-based LOD?
**Answer**: In the patch constant phase, we compute the midpoint of each edge of the patch. We calculate the distance from the camera to the midpoint:
* If distance $\le 1.0\text{m}$, `tessFactor = 4.0`.
* If distance $\ge 10.0\text{m}$, `tessFactor = 1.0` (no subdivision).
* Distances in between are linearly interpolated.

### Q31: How does the Hull Shader prevent edge cracking/gaps between adjacent triangles?
**Answer**: By calculating tessellation factors at the **edge midpoints** of the patch. Since adjacent triangles share the exact same edge and midpoint, they will compute identical edge tessellation factors. This ensures their boundary vertices match perfectly.

### Q32: What mathematical inputs are received by the Domain Shader?
**Answer**:
1. The original control points of the patch.
2. The patch constant tessellation factors.
3. The **Barycentric Coordinates** $(u, v, w)$ of the generated vertex, where $u + v + w = 1.0$.

### Q33: Write the tangent plane projection formula used in Phong Tessellation.
**Answer**: Given a flat interpolated position $P_{\text{linear}}$, we project it onto the tangent plane of control point $P_i$ with normal $N_i$:
$$\pi_i(P_{\text{linear}}) = P_{\text{linear}} - \left( (P_{\text{linear}} - P_i) \cdot N_i \right) N_i$$

### Q34: How is the final vertex position calculated in the Domain Shader?
**Answer**: We project $P_{\text{linear}}$ onto the tangent planes of all three control points, yielding `projection0`, `projection1`, and `projection2`. We interpolate these using barycentrics to find $P_{\text{phong}}$:
$$P_{\text{phong}} = u \pi_0(P_{\text{linear}}) + v \pi_1(P_{\text{linear}}) + w \pi_2(P_{\text{linear}})$$
We then blend the flat and curved positions using $\alpha = 0.75$:
$$P_{\text{final}} = \text{lerp}(P_{\text{linear}}, P_{\text{phong}}, 0.75)$$

---

## Part 5: Mesh Loading & Buffer Management

### Q35: Why does an OBJ file contain separate index counts for positions, UVs, and normals?
**Answer**: The OBJ format is designed for storage efficiency. A vertex might reuse a position index while referencing a new UV or normal index. However, the GPU Input Assembler requires a 1:1 mapping: each vertex index must map to a unique set of position, normal, and UV coordinates.

### Q36: How does the `vertexCache` in `OBJParser.cpp` consolidate vertex/index buffers?
**Answer**: The parser constructs a string key (like `"positionIndex/uvIndex/normalIndex"`) for each vertex. It looks it up in a hash map. If the combination already exists, it reuses the cached index. If it is new, it appends a new `Vertex` to the vertex array and stores its index in the cache. This removes redundant vertices.

### Q37: What properties define a `Submesh` in our codebase?
**Answer**: A submesh is defined by a start index offset and the number of indices in the index buffer, along with its specific material parameters (ambient, diffuse, specular, and texture SRVs).

### Q38: How is a static Vertex Buffer initialized in Direct3D 11?
**Answer**: We configure a `D3D11_BUFFER_DESC` with `BindFlags = D3D11_BIND_VERTEX_BUFFER` and `Usage = D3D11_USAGE_DEFAULT` (or `D3D11_USAGE_IMMUTABLE`). We pass the CPU data pointer to a `D3D11_SUBRESOURCE_DATA` struct and call `device->CreateBuffer`.

### Q39: How is the local-space bounding box calculated for a mesh?
**Answer**: During initialization, we loop over all vertices to find the minimum and maximum $X, Y, Z$ coordinates. The center is calculated as $\frac{\text{min} + \text{max}}{2}$, and the extents are calculated as $\frac{\text{max} - \text{min}}{2}$.

### Q40: What is the aligned byte offset mapping in our input layout description?
**Answer**: The offset describes the location of each variable in the `Vertex` struct:
* `POSITION` starts at byte `0`.
* `NORMAL` starts at byte `12` (after 3 floats * 4 bytes for position).
* `TEXCOORD` starts at byte `24` (after 12 normal bytes + 12 position bytes).

---

## Part 6: Environment Mapping

### Q41: How is the `ID3D11Texture2D` allocated to act as a Cubemap Render Target?
**Answer**: We set `ArraySize = 6` in the texture description, along with the flag `MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE` and bind flags `D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE`.

### Q42: Why do we have six separate RTVs but only one shared DSV for the cubemap?
**Answer**: We render to each face of the cubemap one by one, requiring six individual Render Target Views. However, we only render to one face at a time, so we can reuse a single, shared depth buffer for all face passes, saving GPU memory.

### Q43: Why does the environment map render loop skip drawing the reflective object itself?
**Answer**: To prevent a feedback loop and avoid rendering the interior geometry of the reflective object. When capturing what the sphere sees, the sphere itself is not visible in its own perspective.

### Q44: How is the reflection vector calculated in `ReflectionPS.hlsl`?
**Answer**: We compute the view direction vector $\vec{V}$ from the camera to the pixel world position, and reflect it around the normal vector $\vec{N}$ using:
$$\vec{R} = \vec{V} - 2(\vec{V} \cdot \vec{N})\vec{N}$$
`float3 reflectedView = reflect(incomingView, normalizedNormal);`

### Q45: How is mipmap filtering utilized to render rough environment reflections?
**Answer**: After rendering the 6 faces, we call `GenerateMips(cubeSRV)`. This creates downsampled, blurred copies of the textures. In the shader, we sample these blurred levels based on material roughness using the `SampleLevel` instruction, where roughness maps to the mip level parameter.

---

## Part 7: Quadtree Culling

### Q46: What is the difference between a node's bounding box and an element's bounding box in the quadtree?
**Answer**: A node's bounding box represents the static horizontal quadrant boundary of that quadtree cell. An element's bounding box represents the bounding box of a dynamic `GameObject` inserted into the tree.

### Q47: How does the Quadtree partition space when a node is subdivided?
**Answer**: It divides the parent node's bounding box center/extents into four new, half-sized bounding boxes on the X and Z axes, creating 4 child nodes.

### Q48: How does the quadtree query speed up frustum culling?
**Answer**: During a query, if the camera frustum does not intersect a node's bounding box, we skip querying that node and all its children. This rejects large sectors of the scene with a single intersection test.

### Q49: Why do we need a `std::unordered_set<T> visited` during a quadtree query?
**Answer**: Large game objects may overlap the boundary of two or more quadrants, so a pointer to them is placed in multiple leaf nodes. The `visited` set ensures that each object is added to the output rendering list only once.

### Q50: How do you verify AABB intersections against frustum planes using P-vertices?
**Answer**: For each plane, we select the box corner furthest along the plane normal vector (the positive vertex, or P-vertex). If:
$$\text{PlaneNormal} \cdot \vec{P} + D < 0$$
the entire box lies behind the plane, meaning it is completely outside the frustum and is immediately culled.

---

## Part 8: GPU Particle System

### Q51: What is Vertex Pulling (or vertex buffer-less drawing) in the particle pipeline?
**Answer**: Instead of binding a vertex buffer, we bind `nullptr` and set the topology to a point list. The Vertex Shader reads directly from a `StructuredBuffer<Particle>` using the system-generated index `SV_VertexID` to retrieve particle data.

### Q52: How does the Geometry Shader expand point list primitives into billboards?
**Answer**: It takes one point input (particle position) and uses the camera's `right` and `up` vectors to calculate four corner vertices in world space facing the camera. It projects them and streams them out as 6 vertices (forming two triangles).

### Q53: What formula is used to fade out particles based on lifetime in `ParticlePS.hlsl`?
**Answer**: We calculate the squared distance of the pixel from the center of the billboard using UVs mapped to $[-1, 1]$. We apply a smooth edge falloff using:
`float alphaMask = 1.0f - smoothstep(0.85f, 1.0f, radiusSquared);`

### Q54: How is the blend state configured for transparent particles?
**Answer**: 
* `SrcBlend = D3D11_BLEND_SRC_ALPHA` (factor = source alpha).
* `DestBlend = D3D11_BLEND_INV_SRC_ALPHA` (factor = $1 - \text{source alpha}$).
* `BlendOp = D3D11_BLEND_OP_ADD`.
This mixes the particle color with the existing screen background color.

### Q55: Why must depth buffer writing be disabled when rendering particles?
**Answer**: If particles write to the depth buffer, transparent pixels will write depth values. Particles rendered first will block other transparent particles behind them, creating blocky black boxes. We configure read-only depth checks (`D3D11_DEPTH_WRITE_MASK_ZERO`) to prevent this.

---

## Part 9: Normal Mapping & POM Shaders

### Q56: Explain the math behind screen-space derivative TBN calculation in `ComputeTBN`.
**Answer**: We compute derivatives of world position (`ddx(worldPosition)`, `ddy(worldPosition)`) and UVs (`ddx(uv)`, `ddy(uv)`) across the rasterized triangle. We use these differences to solve for the tangent ($\vec{T}$) and bitangent ($\vec{B}$) direction vectors relative to the surface normal ($\vec{N}$), ensuring they align with the texture space.

### Q57: How does Parallax Occlusion Mapping (POM) ray-marching work?
**Answer**: We step along the view vector direction in UV space (`deltaTexCoords`) and compare the current ray depth against the height map value. Once the ray depth exceeds the height map value, we linearly interpolate between the intersection step and the step immediately before it to find the precise intersection point.

### Q58: Why is `SampleGrad` required in POM, and what parameters does it take?
**Answer**: When sampling textures inside loops with modified UVs, the UV coordinates can jump suddenly at triangle boundaries, making the GPU load a tiny, blurry mipmap. `SampleGrad` forces the GPU to use explicit UV gradients (`ddx` and `ddy` calculated before ray-marching) to select the correct mipmap level, keeping textures sharp.

### Q59: Why must the world position coordinates be modified in the G-Buffer for POM surfaces?
**Answer**: POM shifts the texture coordinate inward to simulate depth. If we write the flat polygon position to the G-Buffer, our deferred light calculations will be calculated at the wrong position. We subtract `normal * depthOffset` from the world position before writing it to the G-Buffer to ensure correct screen-space lighting.

### Q60: How does Gram-Schmidt orthonormalization improve TBN matrix calculations?
**Answer**: It forces the vectors $\vec{T}$, $\vec{B}$, and $\vec{N}$ to be perpendicular to each other:
1. $\vec{T}_{\text{new}} = \text{normalize}(\vec{T} - (\vec{T} \cdot \vec{N})\vec{N})$ (removes any normal components from the tangent).
2. $\vec{B}_{\text{new}} = \vec{N} \times \vec{T}_{\text{new}}$ (constructs the bitangent perpendicular to both).

---

## Part 10: Binding & Dispatch Math

### Q61: Explain the math behind dispatching a compute shader with threads of `16x16` over a `1024x576` screen.
**Answer**: We divide the screen width and height by the thread group dimensions, rounding up to prevent boundary clipping:
* $X = \frac{1024 + 15}{16} = 64$ groups.
* $Y = \frac{576 + 15}{16} = 36$ groups.
We dispatch these groups using: `context->Dispatch(64, 36, 1);`

### Q62: What does the system-generated value `SV_DispatchThreadID` represent?
**Answer**: It represents the global pixel coordinate of the active thread across the entire execution grid. It is calculated automatically as:
$$\text{DispatchThreadID} = \text{GroupID} \times \text{groupDim} + \text{GroupThreadID}$$

### Q63: Why are constant buffers required to align to 16 bytes?
**Answer**: GPU registers process constant data using 4-component float vectors. Storing fields that cross 16-byte boundaries (like a float crossing into the next register) requires the GPU to load two registers for a single variable. Packing variables to 16-byte boundaries avoids this overhead.

### Q64: What is the difference between `Map/Unmap` using discard flags vs `UpdateSubresource`?
**Answer**:
* `D3D11_MAP_WRITE_DISCARD` renames the buffer on the GPU, allocating a new memory address to prevent stalls if the GPU is currently reading from the old block.
* `UpdateSubresource` writes data directly to the active buffer. It is faster for buffers updated rarely, but causes CPU stalls for buffers updated every frame.

---

## Part 11: Advanced Hardware & Debugging (RenderDoc)

### Q65: How do you identify write hazards in RenderDoc?
**Answer**: In RenderDoc, you capture a frame and inspect the **Pipeline State** tab. If a G-Buffer texture is bound as both a Render Target (OM stage) and a Shader Resource (SRV, pixel/compute stages) in the same draw call, RenderDoc will highlight the resource red, marking a write hazard.

### Q66: What is a Direct3D 11 CPU-GPU Sync Stall?
**Answer**: A stall occurs when the CPU requests data back from a GPU resource (e.g., calling `Map` with read flags on a texture currently being written to by the GPU). The CPU thread blocks and waits for the GPU command queue to finish execution, neutralizing parallel CPU-GPU processing.

### Q67: How do we prevent CPU-GPU stalls when reading occlusion query data?
**Answer**: We use double-buffered queries. Instead of querying and reading data on the same frame, we check the query results from **two frames ago**. This gives the GPU ample time to complete the commands, allowing the CPU to read the data instantly without stalling.

### Q68: What is the hardware function of Early-Z, and when does it fail?
**Answer**: Early-Z allows the GPU to run depth-stencil testing before executing the pixel shader. If a fragment fails the depth test, it is discarded immediately, skipping pixel shader math. It fails (forcing Late-Z testing) if the pixel shader writes to depth (`SV_Depth`) or uses discarding instructions (`discard`), because the GPU cannot determine depth until the shader executes.

### Q69: Explain the impact of Warp Divergence on compute shader performance.
**Answer**: Warp divergence occurs when threads in a single warp execute different branches of an `if-else` statement. Because warp threads execute instructions in lockstep, the GPU must run both branches sequentially, masking out threads not active in the current branch. This cuts warp execution speed in half.

### Q70: What is the difference between linear and non-linear depth mapping?
**Answer**:
* **Orthographic projection** maps depth linearly to the range $[0.0, 1.0]$.
* **Perspective projection** maps depth non-linearly ($1/Z$). This allocates most of the floating-point precision to objects close to the camera, which can cause precision issues and shadow acne on objects far away.

### Q71: How does Forsyth's vertex cache optimization algorithm work?
**Answer**: It analyzes the index buffer of a mesh and reorders the faces to group triangles sharing the same vertices close together. This maximizes Post-Transform Vertex Cache hits on the GPU, reducing vertex shader calculations.

### Q72: What is the purpose of `ID3D11DeviceContext::Flush`?
**Answer**: It sends all currently buffered commands in the CPU command queue directly to the GPU execution queue. Direct3D calls this automatically when presenting, so calling it manually is rarely needed.

### Q73: Why do we use oct-encoding for normal vectors in advanced engines?
**Answer**: Octahedral normal encoding maps a 3D unit normal vector $\vec{N} = (x,y,z)$ to a 2D coordinate $(u,v)$ in the $[-1, 1]$ range. This allows normal vectors to be stored in 2 channels (8 bytes) instead of 3 channels (12 bytes), reducing G-Buffer write bandwidth by 33%.

### Q74: Explain the difference between `DXGI_FORMAT_R32_FLOAT` and `DXGI_FORMAT_D32_FLOAT`?
**Answer**: 
* `R32_FLOAT` is a generic red-channel float texture, which can be bound as a Shader Resource View (SRV) or Render Target (RTV).
* `D32_FLOAT` is a dedicated depth buffer format, which can only be bound using a Depth Stencil View (DSV) for hardware depth testing.

### Q75: How does the DXGI debug layer report resource allocation leaks?
**Answer**: When initializing D3D11, we pass the flag `D3D11_CREATE_DEVICE_DEBUG`. When the program terminates, the DXGI runtime prints a list of all unreleased COM pointers, including their types, reference counts, and allocation numbers, to the Visual Studio Output console.
