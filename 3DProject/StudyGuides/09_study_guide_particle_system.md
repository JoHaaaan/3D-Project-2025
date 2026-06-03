# Study Guide 9: GPU Particle System

This guide covers our GPU particle system: writing the Compute Shader update pass, allocating Structured Buffers with UAV/SRV binds, using Geometry Shaders to expand points into camera-facing billboards, and configuring alpha blend states.

---

## 1. Structured Buffer Allocation (GPU Memory)

Traditional particles are updated on the CPU and sent to the GPU every frame, which consumes PCI-Express bus bandwidth. To solve this, we store and simulate our particles directly on the GPU in a **Structured Buffer**.

We allocate the buffer in C++ inside [StructuredBufferD3D11.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/StructuredBufferD3D11.cpp):

```cpp
D3D11_BUFFER_DESC desc = {};
desc.Usage = D3D11_USAGE_DEFAULT; // Default usage allows GPU writes
desc.ByteWidth = sizeof(Particle) * maxParticles;
desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
desc.CPUAccessFlags = 0;
desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
desc.StructureByteStride = sizeof(Particle);

device->CreateBuffer(&desc, nullptr, &buffer);
```

### Double Binding Views
* **Unordered Access View (UAV)**: Created with format typeless, allowing the Compute Shader to read and write to the buffer.
* **Shader Resource View (SRV)**: Created to allow the Vertex Shader to read the particle positions during rendering.

---

## 2. Compute Shader Physics Simulation

The particle update pass runs in [ParticleUpdateCS.hlsl](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleUpdateCS.hlsl). We dispatch groups of 256 threads to simulate physics in parallel:

```hlsl
// ParticleUpdateCS.hlsl
struct Particle
{
    float3 position;
    float3 velocity;
    float lifetime;
    float maxLifetime;
};

RWStructuredBuffer<Particle> particles : register(u0);

cbuffer TimeBuffer : register(b0)
{
    float deltaTime;
    float3 padding;
};

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint id = DTid.x;
    Particle p = particles[id];
    
    // Update lifetime
    p.lifetime += deltaTime;
    
    if (p.lifetime >= p.maxLifetime)
    {
        // Recycle particle (reset position and velocity)
        p.position = float3(0, 0, 0); // Spawn origin
        p.lifetime = 0.0f;
    }
    else
    {
        // Simple gravity integration (Euler integration)
        p.velocity.y += -9.81f * deltaTime; // Apply gravity force
        p.position += p.velocity * deltaTime;
    }
    
    particles[id] = p; // Write back to buffer
}
```

---

## 3. Geometry Shader Billboarding

Particles are stored as 3D points. To render them as texture-mapped quads facing the camera, we use the **Geometry Shader** [ParticleGS.hlsl](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleGS.hlsl) to expand the points into billboards.

```
                  Top-Left              Top-Right
                     o----------------------o
                     |                      |
  Position (Point)   |          *           |  <--- Face Camera
                     |                      |
                     o----------------------o
                 Bottom-Left          Bottom-Right
```

### Billboarding Equations
Given the particle position $P_{\text{center}}$ and the camera's local **Right** ($\vec{R}_{\text{cam}}$) and **Up** ($\vec{U}_{\text{cam}}$) vectors, we calculate the four corner coordinates in world space:
$$C_{0} = P_{\text{center}} - \vec{R}_{\text{cam}} \cdot \text{halfSize} - \vec{U}_{\text{cam}} \cdot \text{halfSize} \quad (\text{Bottom-Left})$$
$$C_{1} = P_{\text{center}} - \vec{R}_{\text{cam}} \cdot \text{halfSize} + \vec{U}_{\text{cam}} \cdot \text{halfSize} \quad (\text{Top-Left})$$
$$C_{2} = P_{\text{center}} + \vec{R}_{\text{cam}} \cdot \text{halfSize} - \vec{U}_{\text{cam}} \cdot \text{halfSize} \quad (\text{Bottom-Right})$$
$$C_{3} = P_{\text{center}} + \vec{R}_{\text{cam}} \cdot \text{halfSize} + \vec{U}_{\text{cam}} \cdot \text{halfSize} \quad (\text{Top-Right})$$

### GS Shader Implementation
```hlsl
[maxvertexcount(4)]
void main(point GS_INPUT input[1], inout TriangleStream<PS_INPUT> triStream)
{
    float3 center = input[0].position;
    float halfSize = 0.1f; // Particle size scale
    
    // Four corner offsets in world space relative to camera axes
    float3 corners[4];
    corners[0] = center - rightVector * halfSize - upVector * halfSize; // Bottom-Left
    corners[1] = center - rightVector * halfSize + upVector * halfSize; // Top-Left
    corners[2] = center + rightVector * halfSize - upVector * halfSize; // Bottom-Right
    corners[3] = center + rightVector * halfSize + upVector * halfSize; // Top-Right
    
    float2 uvs[4] = { float2(0,1), float2(0,0), float2(1,1), float2(1,0) };
    
    PS_INPUT output;
    [unroll]
    for(int i = 0; i < 4; ++i)
    {
        output.position = mul(float4(corners[i], 1.0f), viewProjMatrix);
        output.uv = uvs[i];
        output.lifetimeFactor = input[0].lifetimeFactor;
        triStream.Append(output);
    }
    triStream.RestartStrip();
}
```

---

## 4. Alpha Blending & Depth Testing

Particles are transparent and must blend with the background scene. We configure this using an **Alpha Blend State**:

### Blend State Equations
When rendering particles, we blend the shader output color ($\vec{C}_{\text{src}}$, $\alpha_{\text{src}}$) with the existing pixel color in the render target ($\vec{C}_{\text{dest}}$):
$$\vec{C}_{\text{final}} = \vec{C}_{\text{src}} \cdot \alpha_{\text{src}} + \vec{C}_{\text{dest}} \cdot (1 - \alpha_{\text{src}})$$
We configure this in D3D11:
* `BlendEnable = TRUE`
* `SrcBlend = D3D11_BLEND_SRC_ALPHA`
* `DestBlend = D3D11_BLEND_INV_SRC_ALPHA`
* `BlendOp = D3D11_BLEND_OP_ADD`

### Depth-Testing Configuration
If particles wrote to the depth buffer, the first particle rendered would block all particles behind it, making the system look sparse and blocky. 
To resolve this, we bind a **Depth-Stencil State** configured to perform depth tests (so particles are correctly culled behind walls) but disable depth writes:
* `DepthEnable = TRUE`
* `DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO` (Read-only depth)

---

## Teacher Presentation Tips 🎓

* **Explain why the particle structured buffer must be unbound as a UAV before rendering**:
  * *Answer*: During the update pass, the structured buffer is bound as a write-accessible UAV (`CSSetUnorderedAccessViews`). During the render pass, the Vertex Shader reads from this buffer as an input SRV (`VSSetShaderResources`). If we do not unbind it from the UAV slot by binding a null pointer, the D3D11 runtime flags a read-write conflict and disables the input SRV, rendering the particles invisible.
* **Why do we use the Geometry Shader to expand points instead of generating quad vertex buffers on the CPU?**:
  * *Answer*: Generating quad vertex buffers on the CPU requires updating the positions of four vertices per particle and uploading them over the PCIe bus to the GPU every frame. By using the Geometry Shader, we only store and process one point per particle on the GPU. The expansion into four vertices occurs directly in GPU memory, saving PCIe bandwidth and CPU cycles.
* **What is the purpose of `RestartStrip()` in the Geometry Shader?**:
  * *Answer*: The Geometry Shader outputs primitives using `TriangleStream`. Calling `Append` writes vertices sequentially. `RestartStrip()` tells the GPU that the current triangle strip is complete, ensuring that subsequent vertices do not connect to the previous quad, preventing visual stretching between particles.
* **What is particle back-to-front sorting, and why is it necessary for correct alpha blending?**:
  * *Answer*: The alpha blending formula is non-commutative:
    $$(A \text{ over } B) \ne (B \text{ over } A)$$
    If we draw a transparent particle closer to the camera first, it writes its color to the frame buffer. When we draw a particle behind it, the blend equations cannot interpolate them correctly because the depth test culls the rear particle. To ensure correct blending, we must sort the particles back-to-front relative to the camera position before drawing them.
* **Explain how `smoothstep` is used to create soft round particles in the Pixel Shader**:
  * *Answer*: In `ParticlePS.hlsl`, we map the quad's UV coordinates to $[-1, 1]$ to calculate the radial distance from the center. We fade out the edges using:
    `alpha = 1.0f - smoothstep(0.85f, 1.0f, radiusSquared);`
    Any pixel with a radius less than 0.85 remains opaque, pixels between 0.85 and 1.0 fade out smoothly to transparent, and pixels beyond 1.0 are discarded, creating soft, circular particles from square quads.
