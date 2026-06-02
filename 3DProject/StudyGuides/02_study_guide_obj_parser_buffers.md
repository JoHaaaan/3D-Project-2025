# Study Guide 5: Mesh Loading & Buffer Management (Vertex, Index, Submeshes)

This guide covers mesh ingestion in this project: parsing OBJ/MTL files, using a vertex cache to consolidate index buffers, initializing GPU vertex and index buffers, dividing meshes into submeshes by material, and calculating local bounding boxes for collision detection.

---

## 1. The OBJ Format vs. GPU Input Assembler

An OBJ file stores geometric attributes separately:
* `v x y z`: Vertex position.
* `vt u v`: Texture coordinate.
* `vn x y z`: Normal vector.
* `f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3`: Face index definitions.

### The Indexing Conflict
The GPU Input Assembler expects a single index buffer to reference a single vertex buffer. Each vertex must contain a position, normal, and UV combined at the same address. In the OBJ file, however, a face links index `1` for position, index `3` for texture coordinate, and index `2` for normal. They do not share indices.

### The Solution: Vertex Consolidation & Cache
In [OBJParser.cpp](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/OBJParser.cpp), the parser reads the raw lines and resolves this mismatch:
1. For each vertex in a face definition (e.g., `v/vt/vn`), the parser generates a string key (like `"1/3/2"`).
2. It looks up this key in an `unordered_map<std::string, unsigned int> vertexCache`.
3. **If found**: The combination already exists. The parser reuse the index of the existing vertex and adds it to the index buffer.
4. **If not found**: This is a new combination. The parser constructs a new `Vertex` object (copying position `1`, UV `3`, and normal `2`), appends it to the vertex list, registers its index in the cache, and writes the new index to the index buffer.

This cache ensures no redundant vertices are created on the GPU, saving valuable memory.

---

## 2. Materials and Submesh Splitting

Large 3D models often use different textures for different parts (e.g., a crate with metallic brackets). The OBJ format denotes this with:
* `mtllib library_name.mtl`: Defines material properties (ambient, diffuse, specular, textures).
* `usemtl material_name`: Applies a material to the subsequent faces.

### Submeshes
A single vertex and index buffer is created for the entire model. However, to draw parts with different textures, the model is split into **submeshes**.
In [OBJParser.h:L35](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/OBJParser.h#L35):
* A **SubMeshInfo** stores:
  * `startIndexValue`: The byte offset or index number where the submesh starts in the main index buffer.
  * `nrOfIndicesInSubMesh`: The number of indices belonging to this submesh.
  * Pointers to the texture SRVs (`diffuseTextureSRV`, `normalHeightTextureSRV`, etc.).
  * Material color coefficients.

When the parser encounters `usemtl`, it packages the current list of indices into a `SubMeshInfo` and starts a new submesh for subsequent faces, tracking the index offsets.

---

## 3. Direct3D 11 Buffer Allocation

Once the parser returns the vertex vector and index vector, the `MeshD3D11` class allocates them into GPU memory.

### 1. Vertex Buffer Setup (`VertexBufferD3D11.cpp`)
We describe the buffer and pass a pointer to our CPU vertex array using `D3D11_SUBRESOURCE_DATA`:
```cpp
D3D11_BUFFER_DESC bufferDesc = {};
bufferDesc.Usage = D3D11_USAGE_DEFAULT;
bufferDesc.ByteWidth = sizeOfVertex * nrOfVertices; // total bytes
bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;   // bound as vertex buffer
bufferDesc.CPUAccessFlags = 0;

D3D11_SUBRESOURCE_DATA initData = {};
initData.pSysMem = vertexData; // CPU pointer to Vertex vector
device->CreateBuffer(&bufferDesc, &initData, &buffer);
```

### 2. Index Buffer Setup (`IndexBufferD3D11.cpp`)
Similar to the vertex buffer, but bound as an index buffer:
```cpp
bufferDesc.ByteWidth = sizeof(uint32_t) * nrOfIndices;
bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
initData.pSysMem = indexData;
device->CreateBuffer(&bufferDesc, &initData, &buffer);
```

---

## 4. Local Bounding Box Calculation

To perform view frustum culling, each mesh calculates a local-space axis-aligned bounding box (AABB) when initialized.
In [MeshD3D11.cpp:L41-79](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/MeshD3D11.cpp#L41):
1. Loop over the raw floats in the vertex array using stride offsets.
2. Find the minimum and maximum coordinates $(X, Y, Z)$ across all vertices.
3. Compute the center:
   $$\text{Center} = \frac{\text{minPos} + \text{maxPos}}{2}$$
4. Compute the half-extents:
   $$\text{Extents} = \frac{\text{maxPos} - \text{minPos}}{2}$$
5. Save these into `localBoundingBox` (`DirectX::BoundingBox`).

---

## 5. Binding and Drawing

To render the mesh, we perform these steps:

1. **Bind Buffers to the Input Assembler**:
   In [MeshD3D11.cpp:L82](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/MeshD3D11.cpp#L82):
   ```cpp
   UINT stride = vertexBuffer.GetVertexSize(); // size of Vertex struct (32 bytes)
   UINT offset = 0;
   ID3D11Buffer* vb = vertexBuffer.GetBuffer();
   context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
   context->IASetIndexBuffer(indexBuffer.GetBuffer(), DXGI_FORMAT_R32_UINT, 0);
   ```
2. **Perform Submesh Draw Calls**:
   For each submesh, we bind its textures and materials, then issue the draw call:
   ```cpp
   void SubMeshD3D11::PerformDrawCall(ID3D11DeviceContext* context) const
   {
       context->DrawIndexed(static_cast<UINT>(nrOfIndices), static_cast<UINT>(startIndex), 0);
   }
   ```
   * **`DrawIndexed` arguments**:
     * `nrOfIndices`: Number of index elements to draw for this submesh.
     * `startIndex`: Offset where this submesh's indices begin.
     * `baseVertexLocation`: Offset added to each index value (we pass `0`).

---

## Teacher Presentation Tips 🎓

* **How is the vertex data laid out in memory?**:
  * *Answer*: Each vertex is represented by the `Vertex` struct. It is 32 bytes in size: `XMFLOAT3 Position` (12 bytes) + `XMFLOAT3 Normal` (12 bytes) + `XMFLOAT2 UV` (8 bytes). They are packed sequentially in a contiguous array.
* **Why do we use 32-bit indices (DXGI_FORMAT_R32_UINT) instead of 16-bit (DXGI_FORMAT_R16_UINT)?**:
  * *Answer*: 16-bit indices restrict meshes to a maximum of 65,535 vertices. High-resolution models easily exceed this limit. Using 32-bit indices allows up to 4.29 billion vertices per mesh, which easily accommodates complex geometry.
