# Study Guide 10: Shader Resource Binding, Constant Buffers, and Compute Dispatch

This guide covers resource binding, constant buffer alignment, CPU-GPU memory updates, register slots, compute shader thread grids, and thread group boundaries in Direct3D 11.

---

## 1. Constant Buffers & 16-Byte Register Packing

Constant Buffers (`cbuffers`) store global parameters (like view-projection matrices, light arrays, and time constants) that remain uniform for a draw call.

### The 16-Byte Alignment Rule
Direct3D 11 requires that all constant buffers be allocated with a size that is a multiple of **16 bytes** (matching a single 4-component vector register, `float4`). The GPU packing unit groups variables into 16-byte registers ($X, Y, Z, W$).

If variables cross this 16-byte boundary, they are pushed to the next register, leaving empty padding bytes:
```hlsl
// HLSL Constant Buffer Layout
cbuffer MaterialBuffer : register(b1)
{
    float3 diffuseColor;  // Occupies bytes 0 to 11 (3 floats * 4 bytes)
    float roughness;      // Fits in the remaining 4 bytes (bytes 12 to 15) -> Register 0 complete.
    float3 specularColor; // Cannot fit in Register 0. Moved to Register 1 (bytes 16 to 27).
    float specularExp;    // Fits in Register 1 (bytes 28 to 31) -> Register 1 complete.
}; // Total size = 32 bytes (Multiple of 16)
```

If the C++ struct size does not match this HLSL packing layout (e.g., if we forget padding variables or mismatched data types), the GPU will read misaligned bytes, scrambling variables.

---

## 2. Memory Updates: `Map`/`Unmap` vs. `UpdateSubresource`

To update constant buffers with new camera matrices or frame times, we send data from the CPU to the GPU. We choose between two update techniques in [ConstantBufferD3D11.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ConstantBufferD3D11.cpp):

### 1. `Map` / `Unmap` (Dynamic Renaming)
For dynamic buffers updated every frame, we configure `D3D11_USAGE_DYNAMIC` with write CPU access flags. We lock the buffer using `Map`:
```cpp
D3D11_MAPPED_SUBRESOURCE mappedResource;
context->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
memcpy(mappedResource.pData, cpuDataPointer, dataSize);
context->Unmap(buffer, 0);
```
* **`D3D11_MAP_WRITE_DISCARD`**: Tells the GPU driver that we do not care about the old data in the buffer. If the GPU is currently reading from the buffer during a draw call, the driver allocates a new memory block (renaming), copies our CPU data, and binds it to the pipeline. This avoids CPU stalling, allowing the CPU to write updates without waiting for the GPU to finish.

### 2. `UpdateSubresource` (Direct GPU Copy)
For static buffers updated rarely, we use `UpdateSubresource`:
`context->UpdateSubresource(buffer, 0, nullptr, cpuDataPointer, 0, 0);`
The driver copy-commands the CPU data to a temporary memory block and submits a command to swap it on the GPU queue. This avoids buffer renaming overhead but can stall the CPU if the GPU is busy.

---

## 3. Shader Resource View (SRV) & Unordered Access View (UAV) Mappings

For deferred lighting in [Main.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp), we bind resources to specific register slots:

```
+-----------------------------------------------------------------------------+
|                                LIGHTING PASS                                |
|  INPUTS (SRVs):                                    OUTPUT (UAV):            |
|  [Slot t0] -> G-Buffer Albedo (R16G16B16A16)       [Slot u0] -> OutputTex   |
|  [Slot t1] -> G-Buffer Normal (R16G16B16A16)                     (UAV)      |
|  [Slot t2] -> G-Buffer Position (R16G16B16A16)                              |
|  [Slot t3] -> Shadow Map Array (Texture2DArray)                            |
|  [Slot t4] -> Light Structured Buffer                                       |
+-----------------------------------------------------------------------------+
```

We bind these resources using device context calls:
```cpp
context->CSSetShaderResources(0, 3, gBufferSRVs);
context->CSSetShaderResources(3, 1, &shadowSRV);
context->CSSetShaderResources(4, 1, &lightBufferSRV);
context->CSSetUnorderedAccessViews(0, 1, &lightingUAV, nullptr);
```

---

## 4. Compute Shader Dispatch and Warp Scheduling

Compute Shaders execute threads in groups. In [LightingCS.hlsl](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/LightingCS.hlsl), we define a thread group layout:
`[numthreads(16, 16, 1)]` (256 threads total per group).

### Warp/Wavefront Scheduling
At the hardware level, GPU execution units (SIMD cores) execute threads in lockstep groups called **Warps** (Nvidia, 32 threads) or **Wavefronts** (AMD, 64 threads). 
* Selecting thread group counts that are multiples of 32 (like 256) ensures the warp schedulers are fully occupied with zero idle execution units.
* Storing thread groups in a $16 \times 16$ tile layout optimizes texture read cache locality, as neighboring threads access adjacent texels in L1 cache.

### Out-of-Bounds Protection
If screen resolution dimensions ($1024 \times 576$) are not multiples of our group tile size, the dispatch boundary can extend beyond the screen area. To prevent out-of-bounds memory writes, we run bounds checks inside the shader:
```hlsl
[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // If the global thread coordinates exceed screen dimensions, exit immediately
    if (DTid.x >= screenWidth || DTid.y >= screenHeight)
    {
        return;
    }
    
    // Shader calculations...
}
```

---

## Teacher Presentation Tips 🎓

* **What is the difference between `SV_GroupThreadID` and `SV_DispatchThreadID`?**:
  * *Answer*: 
    * `SV_GroupThreadID` represents the thread's local coordinate within its active thread group (range $[0, 15]$ on X/Y in our $16 \times 16$ layout).
    * `SV_DispatchThreadID` represents the thread's global coordinate across the entire dispatch grid (range $[0, 1023]$ on X, $[0, 575]$ on Y), corresponding to the pixel coordinates on the screen.
* **Why do we get a D3D11 validation warning if a constant buffer struct has a size of 44 bytes in C++?**:
  * *Answer*: Constant buffers must have a size that is a multiple of 16 bytes. A struct of 44 bytes violates this alignment rule. The D3D11 runtime will refuse to bind the buffer, generating a validation warning and rendering the object invisible. We fix this by adding padding variables to align the size to 48 bytes.
* **Explain how `D3D11_MAP_WRITE_DISCARD` prevents GPU pipeline stalls**:
  * *Answer*: If we try to write to a buffer currently in use by the GPU, the CPU must wait for the GPU to finish, stalling execution. `D3D11_MAP_WRITE_DISCARD` tells the driver to ignore the old buffer memory. The driver renames the buffer and assigns a new memory block to the CPU, allowing it to write updates immediately without stalling.
* **What is the register slot bound limit for SRVs and UAVs in Direct3D 11?**:
  * *Answer*: D3D11 supports binding up to 128 Shader Resource Views (slots `t0` to `t127`) and up to 8 Unordered Access Views (slots `u0` to `u7`) in a single shader pass.
* **How does memory coalescing affect structured buffer performance in Compute Shaders?**:
  * *Answer*: Memory coalescing occurs when threads in a warp access contiguous memory addresses. When thread 0 reads index 0, thread 1 reads index 1, and so on, the GPU hardware combines these reads into a single memory transaction. If threads access random, non-contiguous indices, the hardware must execute multiple separate memory fetches, slowing down performance.
