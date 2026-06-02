# Study Guide 8: GPU-Based Particle System (Compute Shaders, Geometry Shader Billboarding, and Blending)

This guide covers the GPU-driven particle system in this project: structured buffers with read/write access (UAV), Euler integration in compute shaders, vertex pulling, billboard geometry expansion, radial pixel masking, and alpha blending.

---

## 1. Why GPU-Based Particle Systems?

In a traditional CPU-based particle system:
1. The CPU updates particle positions and velocities.
2. The CPU writes all particle positions to a dynamic vertex buffer every frame.
3. The CPU uploads this buffer to the GPU.
* **The Bottleneck**: Uploading megabytes of data from CPU memory to GPU memory across the PCIe bus every frame creates a major performance bottleneck.

In our GPU-based particle system:
1. The particle data stays in GPU memory inside a **Structured Buffer** with Unordered Access View (UAV) and Shader Resource View (SRV) bindings.
2. The **Compute Shader** updates the particle physics directly on the GPU.
3. The **Vertex/Geometry Shaders** draw the particles directly from the buffer.
* **Result**: Zero bandwidth transfer across the PCIe bus during the simulation loop. We can simulate thousands of particles with minimal overhead.

---

## 2. Structured Buffers (SRV & UAV)

In [ParticleSystemD3D11.cpp:L22](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleSystemD3D11.cpp#L22), the particle pool is initialized:
`particleBuffer.Initialize(device, sizeof(Particle), numberOfParticles, particles, false, true);`
This creates a structured buffer of 300 elements, each storing a `Particle` struct (48 bytes). It is initialized with two key views:
* **UAV (Unordered Access View)**: Allows the Compute Shader to read and write randomly to any particle in the buffer:
  `RWStructuredBuffer<Particle> Particles : register(u0);`
* **SRV (Shader Resource View)**: Allows the Vertex Shader to read the particle properties during rendering:
  `StructuredBuffer<Particle> Particles : register(t0);`

---

## 3. Update Pass: Compute Shader (`ParticleUpdateCS.hlsl`)

Every frame, the update logic runs in [ParticleSystemD3D11.cpp:L86-124](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleSystemD3D11.cpp#L86):
1. **Bind UAV**: `context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);`
2. **Dispatch Groups**: The compute shader processes particles in threads. The thread layout is defined as `[numthreads(32, 1, 1)]` (32 threads per group). We divide the total particle count by 32 to determine how many thread groups to launch:
   `unsigned int numGroups = (numParticles + 31) / 32;`
   `context->Dispatch(numGroups, 1, 1);`
3. **Unbind UAV**: We must set the UAV slot back to `nullptr` before using the buffer as an input to the rendering pipeline:
   `context->CSSetUnorderedAccessViews(0, 1, &nullUAV, nullptr);`

### Physics Integration inside the Compute Shader
For each particle index, `ParticleUpdateCS.hlsl` performs Euler integration:
```hlsl
// Position integration
particle.position += particle.velocity * deltaTime;
// Gravity acceleration (constant downward force)
particle.velocity.y += -2.0f * deltaTime;
// Age update
particle.lifetime += deltaTime;
```
If a particle exceeds its maximum lifetime, the shader calls `RespawnParticle()`. It uses a pseudo-random hash generator based on the thread index and current time to calculate a new position (emitter position) and a randomized velocity vector within `velocityMin` and `velocityMax`.

---

## 4. Render Pass: Vertex Pulling and Geometry Billboarding

### Vertex Pulling (Vertex Buffer-less Drawing)
Instead of binding a traditional vertex buffer, we bind `nullptr` and set the topology to a point list:
```cpp
context->IASetInputLayout(nullptr);
context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
context->VSSetShaderResources(0, 1, &srv);
context->Draw(numParticles, 0);
```
During drawing, the GPU generates a sequential vertex index (`vertexID`). In [ParticleVS.hlsl:L23](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleVS.hlsl#L23), the shader pulls the vertex data manually:
```hlsl
VS_OUTPUT main(uint vertexID : SV_VertexID)
{
    Particle particle = Particles[vertexID]; // Pull vertex from structured buffer SRV
    ...
}
```

### Geometry Billboarding (`ParticleGS.hlsl`)
The vertex shader outputs a point (center position). The Geometry Shader [ParticleGS.hlsl](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleGS.hlsl) expands this single point into a 2D quad (2 triangles / 6 vertices) that always faces the camera.
It uses the camera's `right` and `up` vectors (passed in the constant buffer `b0`):
```hlsl
float3 right = cameraRight * quadSize;
float3 up = cameraUp * quadSize;

// Compute 4 corner points around the particle center position
float3 vertex1 = particlePosition - right + up; // Top-Left
float3 vertex2 = particlePosition + right + up; // Top-Right
float3 vertex3 = particlePosition - right - up; // Bottom-Left
float3 vertex4 = particlePosition + right - up; // Bottom-Right
```
We project each corner vertex to screen coordinates using `viewProjection` and output UVs:
* Vertex 1: UV `(0.0, 0.0)`
* Vertex 2: UV `(1.0, 0.0)`
* Vertex 3: UV `(0.0, 1.0)`
* Vertex 4: UV `(1.0, 1.0)`
These are streamed into two triangles using `output.Append(o)`.

---

## 5. Shading and Alpha Blending

### Pixel Shading (`ParticlePS.hlsl`)
To make the particles soft and round instead of square:
1. Map UVs from $[0,1]$ to a centered $[-1,1]$ coordinate system:
   `float2 centeredUV = input.uv * 2.0f - 1.0f;`
2. Compute distance from the center: `float radiusSquared = dot(centeredUV, centeredUV);`
3. Apply a smooth edge falloff using HLSL `smoothstep`:
   `float alphaMask = 1.0f - smoothstep(0.85f, 1.0f, radiusSquared);`
4. Discard pixels outside the radius to optimize performance:
   `if (outputColor.a <= 0.001f) discard;`

### Alpha Blending Configuration
Because particles are transparent, they must blend with the geometry already drawn to the screen. In [Main.cpp:L874](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L874), the particle pass is performed immediately after the deferred lighting calculation. We bind the blend state:
```cpp
context->OMSetBlendState(particleBlendState, nullptr, 0xffffffff);
```
This is configured with:
* `SrcBlend = D3D11_BLEND_SRC_ALPHA` (new particle color factor = alpha)
* `DestBlend = D3D11_BLEND_INV_SRC_ALPHA` (existing screen color factor = $1 - \text{alpha}$)
* `BlendOp = D3D11_BLEND_OP_ADD`
This produces standard alpha transparency:
$$\text{Color}_{\text{final}} = \text{Color}_{\text{src}} \times A_{\text{src}} + \text{Color}_{\text{dest}} \times (1 - A_{\text{src}})$$

---

## Teacher Presentation Tips 🎓

* **Explain the difference between SV_VertexID and SV_InstanceID**:
  * *Answer*: `SV_VertexID` is a system-generated index pointing to the current vertex in a draw call. In our vertex pulling setup, we call `Draw(numParticles, 0)`, which tells the GPU to render `numParticles` vertices. The GPU automatically increments `SV_VertexID` from $0$ to `numParticles - 1` for each vertex, allowing us to index our structured buffer. `SV_InstanceID` is used for instanced drawing, which is a different technique.
* **Explain why you unbind the Geometry Shader after the particle pass**:
  * *Answer*: The Geometry Shader remains bound to the pipeline until it is explicitly unbound by setting it to `nullptr`. If we don't unbind it (`context->GSSetShader(nullptr, nullptr, 0)`), the next draw call in the next frame will attempt to run standard triangles through the particle geometry shader, leading to rendering errors or crashes.
