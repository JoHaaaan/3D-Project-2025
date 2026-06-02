# Study Guide 7: Quadtree Culling & Frustum Culling

This guide covers spatial partitioning and visibility determination in this project: the mathematics of a view frustum, the structural design of the quadtree, dynamic subdivision, bounding volume intersection testing, and duplicate filtering.

---

## 1. The Need for Frustum Culling

In a large 3D scene, rendering every object regardless of whether it is visible to the player is highly inefficient.
* **The CPU Bottleneck**: Processing meshes that are behind the player involves calculating world transformations and setting binding states, wasting valuable frame time.
* **The Solution**: **Frustum Culling**. We compare each object's bounding box against the camera's view frustum. If an object is outside, we skip its draw call entirely.

### What is a View Frustum?
A view frustum represents the pyramid-like volume of space visible to the camera. It is defined by six clipping planes: Left, Right, Top, Bottom, Near, and Far. In DirectX, we use the helper class **`DirectX::BoundingFrustum`** to represent and test intersections against this volume.

---

## 2. Why a Quadtree instead of an Octree?

An **Octree** divides 3D space into 8 octants, whereas a **Quadtree** divides a 2D plane into 4 quadrants.
* **Why Quadtree?** In our project, the scene is laid out horizontally across a terrain/floor (X and Z plane). The vertical (Y) variation is minimal. Partitioning the vertical space (Y-axis) with an Octree would create empty layers and add extra intersection checks for no benefit.
* **The Setup**: We define the root bounding box covering the entire playable arena in [Main.cpp:L443](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/Main.cpp#L443) ($100 \times 50 \times 100$ meters):
  ```cpp
  DirectX::BoundingBox worldBoundingBox(XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT3(50.0f, 25.0f, 50.0f));
  QuadTree<GameObject*> sceneTree(worldBoundingBox, 5, 8);
  ```
  * `maxDepth = 5`: Limits recursion to prevent infinite loops.
  * `maxElementsPerNode = 8`: Splitting threshold.

---

## 3. Quadtree Node and Subdivision Design

In [QuadTree.h:L26](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/QuadTree.h#L26), a node in the tree is defined as:
```cpp
struct Node
{
    DirectX::BoundingBox boundingBox;  // AABB covering this node's sector
    std::unique_ptr<Node> children[4]; // 4 Quadrants
    std::vector<QuadTreeElement<T>> elements; // Elements stored if leaf node
    bool isLeaf = true;
};
```

### Subdivision (`Subdivide`)
When an insertion causes a leaf node to exceed `maxElementsPerNode`, we subdivide it into 4 quadrants:
1. **Top-Left (0)**: $(-X, +Z)$
2. **Top-Right (1)**: $(+X, +Z)$
3. **Bottom-Left (2)**: $(-X, -Z)$
4. **Bottom-Right (3)**: $(+X, -Z)$
The child nodes retain the full height ($Y$) of the parent node but cover exactly a quarter of the horizontal area:
```cpp
float halfX = extents.x * 0.5f;
float halfZ = extents.z * 0.5f;
node->children[0]->boundingBox.Center = XMFLOAT3(center.x - halfX, center.y, center.z + halfZ);
node->children[0]->boundingBox.Extents = XMFLOAT3(halfX, extents.y, halfZ);
```
Once subdivided, the elements previously stored in the parent node are redistributed to the children, and the parent's elements vector is cleared.

---

## 4. Node Traversal: Insertion and Query

### 1. Insertion (`Insert`)
Inserting an element is a recursive traversal:
* If the element's bounding box does not intersect the current node's bounding box, return immediately.
* If the node is a leaf, append the element. If it exceeds 8 elements and depth $< 5$, subdivide it and re-insert all elements into the children.
* If it is an internal node, recursively call `Insert` on all four children.

### 2. Querying (`Query`)
To gather visible objects, we query the tree using the camera frustum:
```cpp
template<typename T>
void QuadTree<T>::Query(Node* node, const DirectX::BoundingFrustum& frustum, std::unordered_set<T>& visited, std::vector<T>& result) const
{
    if (!node) return;

    // 1. If frustum does not overlap the node's bounds, skip the entire branch
    if (!frustum.Intersects(node->boundingBox))
        return;

    // 2. If it is a leaf, check the elements individually
    if (node->isLeaf)
    {
        for (const auto& elem : node->elements)
        {
            if (frustum.Intersects(elem.boundingBox))
            {
                // Ensure duplicate nodes are filtered
                if (visited.find(elem.data) == visited.end())
                {
                    visited.insert(elem.data);
                    result.push_back(elem.data);
                }
            }
        }
    }
    // 3. If internal node, traverse down
    else
    {
        for (int i = 0; i < 4; ++i)
            Query(node->children[i].get(), frustum, visited, result);
    }
}
```

### Why we need a `visited` filter
If a game object is large (e.g. the ground floor mesh), it may overlap the boundary of two or more quadrants. As a result, the insertion routine will place a pointer to it in multiple leaf nodes. During the query, both leaf nodes might intersect the frustum. The `unordered_set<T> visited` ensures that each object is added to the rendering list only once, avoiding duplicate draw calls.

---

## 5. Dynamic Frame Update Cycle

Every frame in `Main.cpp`:
1. **Clear Tree**: `sceneTree.Clear()` resets child nodes and clears arrays.
2. **Re-Insert**: As game objects move (like the rotating cubes), their world bounding boxes change. We re-insert them into the tree:
   `sceneTree.Insert(&obj, obj.GetWorldBoundingBox());`
3. **Extract Visible Objects**: Query using the camera's bounding frustum:
   `sceneTree.Query(cullingFrustum, visibleObjects);`
4. **Draw**: Loop through `visibleObjects` and perform the draw calls.

---

## Teacher Presentation Tips 🎓

* **Explain the difference in complexity between Brute-Force and Quadtree Culling**:
  * *Answer*: In a brute-force approach, we would check all $N$ objects in the scene individually against the camera frustum every frame, which runs in $\mathcal{O}(N)$ time. With a Quadtree, if a quadrant containing 100 objects is outside the frustum, we reject the entire branch with a single intersection test. The query runs in $\mathcal{O}(\log N)$ time, significantly improving CPU performance as the scene scales.
* **How are objects stored in the Quadtree?**:
  * *Answer*: The tree stores pointers to `GameObject` instances inside `QuadTreeElement` structs along with their world bounding boxes. This keeps memory overhead minimal, as we only copy pointers rather than duplicating mesh data.
