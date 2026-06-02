# Study Guide 10: Shader Bindings, Registers, and Dispatch Mathematics

This guide covers resource binding and compute dispatch mechanics: detailing shader registers (constant buffers, SRVs, UAVs, samplers), executing pipeline bindings via the device context, computing thread group sizes, and explaining the C++ lvalue requirement for D3D11 calls.

---

## 1. Complete Resource Binding Table

Shaders communicate with the C++ application through registers. Direct3D 11 uses prefixes:
* **`b`**: Constant Buffers (read-only parameters).
* **`t`**: Shader Resource Views (SRVs - textures, structured buffers).
* **`u`**: Unordered Access Views (UAVs - read-write textures/buffers).
* **`s`**: Samplers (texture filtering states).

### The Deferred Shading Compute Shader (`LightingCS.hlsl`)
| Resource Name | Register | Type | C++ Binding Context Call |
| :--- | :--- | :--- | :--- |
| **`CameraBuffer`** | `b2` | Constant Buffer | `context->CSSetConstantBuffers(2, 1, &cameraCB);` |
| **`LightingToggleBuffer`** | `b4` | Constant Buffer | `context->CSSetConstantBuffers(4, 1, &toggleCB);` |
| **`gAlbedo`** (G-Buffer) | `t0` | Texture2D | `context->CSSetShaderResources(0, 3, srvs);` (slot 0) |
| **`gNormal`** (G-Buffer) | `t1` | Texture2D | `context->CSSetShaderResources(0, 3, srvs);` (slot 1) |
| **`gWorldPos`** (G-Buffer) | `t2` | Texture2D | `context->CSSetShaderResources(0, 3, srvs);` (slot 2) |
| **`shadowMaps`** | `t3` | Texture2DArray | `context->CSSetShaderResources(3, 1, &shadowSRV);` |
| **`lights`** | `t4` | StructuredBuffer | `context->CSSetShaderResources(4, 1, &lightSRV);` |
| **`outColor`** (Screen Output) | `u0` | RWTexture2D | `context->CSSetUnorderedAccessViews(0, 1, &lightingUAV, nullptr);` |
| **`shadowSampler`** | `s1` | SamplerState | `context->CSSetSamplers(1, 1, &shadowSampler);` |

---

## 2. D3D11 Pipeline Binding Calls

To bind resources to specific stages of the graphics/compute pipeline, the C++ application calls `ID3D11DeviceContext` API functions.

### Constant Buffers
* **`VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers)`** - Binds to the Vertex Shader.
* **`HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers)`** - Binds to the Hull Shader.
* **`DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers)`** - Binds to the Domain Shader.
* **`PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers)`** - Binds to the Pixel Shader.
* **`CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers)`** - Binds to the Compute Shader.

### Textures & SRVs
* **`VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews)`**
* **`PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews)`**
* **`CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews)`**

### UAVs (Compute Stage only)
* **`CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts)`**

---

## 3. C++ Lvalue Reference and Address-of Getter Rules

A common C++ compile error in Direct3D 11 programming is trying to pass resource pointers directly from getter methods.

### The Problem
Consider this code:
```cpp
// COMPILE ERROR: Cannot take the address of a temporary rvalue!
context->CSSetShaderResources(3, 1, &shadowMap.GetSRV());
```
* **Why it fails**: `shadowMap.GetSRV()` returns a temporary copy of a pointer (`ID3D11ShaderResourceView*`). This is an **rvalue** (temporary expression). C++ does not allow taking the address (`&`) of an rvalue because it has no permanent memory address.

### The Solution
We must store the return value in a local variable (an **lvalue**) first, then pass the address of that variable:
```cpp
// CORRECT: shadowSRV is an lvalue
ID3D11ShaderResourceView* shadowSRV = shadowMap.GetSRV();
context->CSSetShaderResources(3, 1, &shadowSRV);
```
Since the D3D11 context functions take an array of pointers (represented by a pointer-to-pointer `ID3D11ShaderResourceView**`), we must pass the address of our local pointer.

---

## 4. Compute Shader Dispatch and Grid Mathematics

To run a compute shader, we must define the thread execution grid.
* **Thread**: The smallest unit of execution (runs on one GPU ALU).
* **Thread Group**: A block of threads that run together and can share cache memory.
* **Dispatch Grid**: A collection of thread groups launched in a single call.

```
                  ◄───────────────── WIDTH ─────────────────►
           ┌──────┬──────┬──────┬──────┬──────┬──────┬──────┐ ▲
           │16x16 │16x16 │      │      │      │      │      │ │
           │Group │Group │      │      │      │      │      │ │
           ├──────┼──────┼──────┼──────┼──────┼──────┼──────┤ HEIGHT
           │      │      │      │      │      │      │      │ │
           │      │      │      │      │      │      │      │ │
           └──────┴──────┴──────┴──────┴──────┴──────┴──────┘ ▼
```

### 1. Deferred Shading Dispatch (`LightingCS.hlsl`)
In the shader file:
`[numthreads(16, 16, 1)]`
Each thread group contains $16 \times 16 = 256$ threads, mapped to a 2D screen coordinate.

In C++ [Main.cpp:L864](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L864):
```cpp
context->Dispatch((WIDTH + 15) / 16, (HEIGHT + 15) / 16, 1);
```
* **The Math**: If `WIDTH = 1024` and `HEIGHT = 576`:
  * $X = \frac{1024 + 15}{16} = 64$ groups.
  * $Y = \frac{576 + 15}{16} = 36.9375 \rightarrow 36$ groups (with integer division).
  * We add `15` before dividing by `16` to perform integer division **rounding up**, ensuring that partial screen boundary tiles are not clipped.
* **The Bounds Check**: Because rounding up can launch threads outside the screen dimensions, the shader performs an early return:
  ```hlsl
  uint2 pixel = DTid.xy;
  if (pixel.x >= width || pixel.y >= height) return;
  ```

### 2. Particle Update Dispatch (`ParticleUpdateCS.hlsl`)
In the shader:
`[numthreads(32, 1, 1)]` (1D group of 32 threads).

In C++ [ParticleSystemD3D11.cpp:L116](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/ParticleSystemD3D11.cpp#L116):
```cpp
unsigned int numGroups = static_cast<unsigned int>(std::ceil(numParticles / 32.0f));
context->Dispatch(numGroups, 1, 1);
```
Inside the shader:
```hlsl
uint index = DTid.x;
if (index >= particleCount) return;
```
This ensures we simulate exactly `numParticles` without out-of-bounds array access.

---

## Teacher Presentation Tips 🎓

* **What is the difference between SV_GroupID, SV_GroupThreadID, and SV_DispatchThreadID?**:
  * *Answer*:
    * `SV_GroupID`: The index of the active thread group within the dispatch grid (e.g. $[0, 63]$ along X).
    * `SV_GroupThreadID`: The local index of the thread within its group (e.g. $[0, 15]$ along X and Y).
    * `SV_DispatchThreadID`: The global pixel coordinate on the screen. It is calculated automatically as:
      $$\text{DispatchThreadID} = \text{GroupID} \times \text{groupDim} + \text{GroupThreadID}$$
* **Why do we pass nullptr to unbind resources?**:
  * *Answer*: If we leave G-Buffer textures bound as SRVs to the compute shader, and in the next frame the geometry pass tries to write to them as Render Targets, a binding conflict occurs. Setting them to `nullptr` clears the slots, keeping the pipeline valid.
