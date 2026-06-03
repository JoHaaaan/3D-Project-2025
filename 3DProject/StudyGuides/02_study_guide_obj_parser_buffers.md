# Study Guide 2: Mesh Loading & Buffer Management (Vertex, Index, Submeshes)

This guide covers mesh ingestion in this project: parsing OBJ/MTL files, using a vertex cache to consolidate index buffers, initializing GPU vertex and index buffers, dividing meshes into submeshes by material, and calculating local bounding boxes for collision detection.

---

## 1. The OBJ Format vs. GPU Input Assembler

An OBJ file is a text-based format that stores geometric attributes separately to minimize file size. A typical file contains:
* `v x y z`: Vertex position coordinates.
* `vt u v`: Texture UV coordinates.
* `vn x y z`: Vertex normal vector components.
* `f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3`: Face index definitions.

### The Indexing Conflict

The GPU's Input Assembler (IA) stage requires a 1:1 mapping: each index in the Index Buffer points to a single, complete vertex structure containing position, UV, and normal data combined at the same memory address. 

In the raw OBJ file, however, faces can look like this:
```
f 1/1/1 2/2/1 3/3/1
f 4/4/2 1/5/2 3/6/2
```
Notice that vertex position index `1` is paired with texture coordinate index `1` in the first face, but is paired with texture coordinate index `5` in the second face. The GPU cannot index these attributes separately.

### The Solution: Vertex Consolidation & Cache

In [OBJParser.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/OBJParser.cpp), the parser resolves this conflict during ingestion using a custom **Vertex Cache**:

1. **Tokenization**: The parser splits each face line into individual coordinate triplets (`v/vt/vn`).
2. **String Key Generation**: For each triplet, the parser creates a unique string key, such as `"1/5/2"`.
3. **Cache Lookup**: It queries this key in an `std::unordered_map<std::string, unsigned int> vertexCache`:
   ```cpp
   // Pseudo-code of the consolidation loop
   auto it = vertexCache.find(tripletKey);
   if (it != vertexCache.end())
   {
       // Re-use existing vertex index
       indices.push_back(it->second);
   }
   else
   {
       // Construct a new consolidated Vertex struct
       Vertex newVertex;
       newVertex.position = tempPositions[vIndex - 1];
       newVertex.normal   = tempNormals[vnIndex - 1];
       newVertex.uv       = tempUVs[vtIndex - 1];

       unsigned int newIndex = (unsigned int)vertices.size();
       vertices.push_back(newVertex);
       vertexCache[tripletKey] = newIndex;
       indices.push_back(newIndex);
   }
   ```
4. **Result**: A clean, deduplicated array of unique `Vertex` structs and a corresponding index buffer that references them.

---

## 2. Materials and Submesh Splitting

3D models are typically divided into sections using different materials. The OBJ file defines this using:
* `mtllib library_name.mtl`: Points to a separate material file containing colors, specular parameters, and texture paths.
* `usemtl material_name`: Commands the parser to apply a specific material to all subsequent faces.

### Submeshes
Rather than allocating a separate vertex and index buffer for every material (which would trigger high CPU draw-call overhead and state-change bottlenecks), our engine compiles a single large vertex and index buffer for the entire model. To apply different textures, the model is split into **Submeshes**.

A **SubMeshInfo** struct is declared in [OBJParser.h:L35](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/OBJParser.h#L35):
* `startIndexValue`: The index offset where this submesh begins in the main index buffer.
* `nrOfIndicesInSubMesh`: The number of indices that belong to this submesh.
* Pointers to the texture SRVs (`diffuseTextureSRV`, `normalHeightTextureSRV`, etc.).
* Material colors (ambient, diffuse, specular).

When the parser reads `usemtl`, it packages the active face data into a submesh, calculates its index count, and begins tracking the starting index offset for the next submesh.

---

## 3. Direct3D 11 Buffer Allocation

Once the parser outputs the vertex and index vectors, we allocate GPU buffers using the D3D11 device.

### 1. Vertex Buffer Allocation (`VertexBufferD3D11.cpp`)
We describe the buffer geometry and set a pointer to our CPU vertex array using `D3D11_SUBRESOURCE_DATA`:
```cpp
D3D11_BUFFER_DESC bufferDesc = {};
bufferDesc.Usage = D3D11_USAGE_DEFAULT; // Read and write by GPU
bufferDesc.ByteWidth = sizeof(Vertex) * nrOfVertices; // Total size in bytes
bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;     // GPU binding type
bufferDesc.CPUAccessFlags = 0;                        // No CPU reads/writes

D3D11_SUBRESOURCE_DATA initData = {};
initData.pSysMem = vertexData; // CPU pointer to the consolidated vertex vector
device->CreateBuffer(&bufferDesc, &initData, &buffer);
```

### 2. Index Buffer Allocation (`IndexBufferD3D11.cpp`)
Similar to the vertex buffer, but bound as an index buffer:
```cpp
bufferDesc.ByteWidth = sizeof(uint32_t) * nrOfIndices;
bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
initData.pSysMem = indexData;
device->CreateBuffer(&bufferDesc, &initData, &buffer);
```

### Buffer Usage Flags
* **`D3D11_USAGE_DEFAULT`**: Optimized for GPU read/write access. We use this for resources written by shaders (like structured particle buffers).
* **`D3D11_USAGE_IMMUTABLE`**: Set at creation time and cannot be modified. Best for static vertex and index buffers because the driver can place the data in ultra-fast, read-only local GPU memory.
* **`D3D11_USAGE_DYNAMIC`**: Write-only from the CPU. Used for resources updated every frame (like constant buffers) using `Map()` with `D3D11_MAP_WRITE_DISCARD`.

---

## 4. Input Layouts

The GPU must understand how the raw bytes of the bound vertex buffer map to the parameters in the vertex shader. We define this mapping using `ID3D11InputLayout` in [InputLayoutD3D11.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/InputLayoutD3D11.cpp).

### The Input Element Description Array
We describe each member of the `Vertex` struct:
```cpp
D3D11_INPUT_ELEMENT_DESC layout[] =
{
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};
```
* **SemanticName**: Maps to variables in the vertex shader (e.g., `struct VS_INPUT { float3 pos : POSITION; };`).
* **Format**: Maps data types. `R32G32B32_FLOAT` represents three 32-bit floats (`XMFLOAT3` or `float3`).
* **AlignedByteOffset**: The offset of the variable in the struct:
  * `POSITION` starts at byte `0`.
  * `NORMAL` starts at byte `12` (after 3 position floats $\times 4$ bytes).
  * `TEXCOORD` starts at byte `24` (after 12 normal bytes + 12 position bytes).

---

## 5. Local Bounding Box Calculation

To support view frustum culling, we calculate a local-space axis-aligned bounding box (AABB) when initializing each mesh in [MeshD3D11.cpp:L41-79](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/MeshD3D11.cpp#L41):

1. Loop through all vertex positions.
2. Find the minimum and maximum coordinates $(X_{\text{min}}, Y_{\text{min}}, Z_{\text{min}})$ and $(X_{\text{max}}, Y_{\text{max}}, Z_{\text{max}})$.
3. Compute the center:
   $$\vec{C} = \frac{\vec{P}_{\text{min}} + \vec{P}_{\text{max}}}{2}$$
4. Compute the half-extents (the distance from center to boundaries):
   $$\vec{E} = \frac{\vec{P}_{\text{max}} - \vec{P}_{\text{min}}}{2}$$
5. Save this into `localBoundingBox` (`DirectX::BoundingBox`).
6. **Transforming to World Space**: When updating objects, we transform this box using the object's World Matrix $W$:
   `localBoundingBox.Transform(worldBoundingBox, W);`
   This transforms the AABB into an Oriented Bounding Box (OBB) in world space, maintaining bounds accuracy as the object rotates.

---

## Teacher Presentation Tips 🎓

* **Explain why the Input Layout requires the compiled Vertex Shader byte-code to initialize**:
  * *Answer*: Direct3D 11 performs layout signature validation at creation time. The runtime checks the sizes, types, and semantics defined in the `D3D11_INPUT_ELEMENT_DESC` array against the input parameters of the vertex shader to ensure they match. If a discrepancy is found (e.g., a missing semantic or mismatched format size), `CreateInputLayout` fails immediately, preventing runtime crashes.
* **What are the CPU and GPU access flags for an `IMMUTABLE` buffer?**:
  * *Answer*: An immutable buffer has `D3D11_USAGE_IMMUTABLE`. This allows the GPU read access, but denies the GPU write access. The CPU has zero access flags (no read, no write) after creation. This allows the OS driver to place the buffer in dedicated graphics memory with optimal access speeds.
* **If we bind a vertex buffer with stride 32, but our IA input description array claims a size of 24 bytes, what happens?**:
  * *Answer*: The D3D11 validation layers will flag this. The Input Assembler reads vertex data based on the stride passed to `IASetVertexBuffers`. If the stride is 32 but the layout describes only 24 bytes, the GPU reads the attributes of the first vertex correctly, but the second vertex will be offset incorrectly by 8 bytes, causing scrambled vertex positions and UV mapping.
* **How does submesh rendering reduce D3D11 state switches?**:
  * *Answer*: Instead of unbinding and binding new vertex and index buffers for each submesh, we bind a single vertex buffer and index buffer once. To draw different sections of the model, we only swap the shader texture resources (SRVs) and call `DrawIndexed` with the corresponding index offset (`startIndex`). This avoids expensive pipeline re-bindings.
* **What is the difference between an Axis-Aligned Bounding Box (AABB) and an Oriented Bounding Box (OBB)?**:
  * *Answer*: An AABB's edges are locked parallel to the global coordinate axes ($X, Y, Z$). When an object rotates, its AABB must expand to fit the object, leading to loose, inaccurate bounds. An OBB rotates with the object, maintaining tight, accurate bounding margins at the cost of slightly more complex collision calculations.
