# Study Guide 3: CPU Frustum Culling and Quadtree Traversal

This guide covers spatial partitioning and view frustum culling: extracting camera frustum planes from view-projection matrices, calculating plane-to-box intersections, traversing the Quadtree structure, and compiling the visible render list.

---

## 1. Camera Frustum Planes Extraction

To determine if an object is visible to the camera, we must check it against the camera's **View Frustum** (the 3D pyramid volume defining the camera's field of view). The frustum is bounded by 6 planes: Left, Right, Bottom, Top, Near, and Far.

```
       Far Plane
     +-----------+
    /             \
   /   Frustum     \
  /    Volume       \
 /                   \
+---------------------+
 Near Plane  (Camera origin at apex)
```

Each plane is defined by a 3D normal vector $\vec{N} = (A,B,C)$ and a distance constant $D$:
$$A \cdot x + B \cdot y + C \cdot z + D = 0$$

### Mathematical Extraction from View-Projection Matrix

Rather than calculating these planes manually using camera angles and FOV geometry, we extract them directly from the rows of the combined **View-Projection Matrix ($M = V \cdot P$)**. 

If $M$ is a column-major matrix or row-major matrix transposed, its rows represent the coordinate transformation coefficients. The clipping boundaries in homogeneous clip space are $-W \le X \le W$, $-W \le Y \le W$, and $0 \le Z \le W$. 

By mapping these boundaries, we extract the planes:
* **Left Plane**: $\vec{P}_{\text{left}} = \text{Row}_3 + \text{Row}_0$
* **Right Plane**: $\vec{P}_{\text{right}} = \text{Row}_3 - \text{Row}_0$
* **Bottom Plane**: $\vec{P}_{\text{bottom}} = \text{Row}_3 + \text{Row}_1$
* **Top Plane**: $\vec{P}_{\text{top}} = \text{Row}_3 - \text{Row}_1$
* **Near Plane**: $\vec{P}_{\text{near}} = \text{Row}_2$ (or $\text{Row}_3 + \text{Row}_2$ depending on D3D coordinate convention)
* **Far Plane**: $\vec{P}_{\text{far}} = \text{Row}_3 - \text{Row}_2$

### Normalization
The extracted vectors are typeless plane parameters. To measure the actual Euclidean distance from a point to a plane, we must normalize the plane equations. We divide the components $A, B, C, D$ by the length of the normal vector:
$$\text{Length} = \sqrt{A^2 + B^2 + C^2}$$
$$\vec{N}_{\text{normalized}} = \frac{(A, B, C)}{\text{Length}}, \quad D_{\text{normalized}} = \frac{D}{\text{Length}}$$
After normalization, evaluating $A \cdot x + B \cdot y + C \cdot z + D$ for any point $(x,y,z)$ returns the exact signed distance in world units. If the result is positive, the point is in front of the plane; if negative, the point is behind the plane.

---

## 2. Frustum vs. Bounding Box (AABB) Intersection

To cull objects, we test their Axis-Aligned Bounding Box (AABB) against each of the 6 normalized frustum planes.

### The Positive/Negative Vertex Method (P-vertex / N-vertex)
For any plane with normal vector $\vec{N} = (N_x, N_y, N_z)$, we can find the corner of the AABB (defined by $\vec{min}$ and $\vec{max}$) that lies furthest in the direction of the normal (the **P-vertex**) and the corner furthest in the opposite direction (the **N-vertex**):

$$\vec{P}_{\text{vertex}.x} = (N_x \ge 0) ? x_{\text{max}} : x_{\text{min}}$$
$$\vec{P}_{\text{vertex}.y} = (N_y \ge 0) ? y_{\text{max}} : y_{\text{min}}$$
$$\vec{P}_{\text{vertex}.z} = (N_z \ge 0) ? z_{\text{max}} : z_{\text{min}}$$

$$\vec{N}_{\text{vertex}.x} = (N_x \ge 0) ? x_{\text{min}} : x_{\text{max}}$$
$$\vec{N}_{\text{vertex}.y} = (N_y \ge 0) ? y_{\text{min}} : y_{\text{max}}$$
$$\vec{N}_{\text{vertex}.z} = (N_z \ge 0) ? z_{\text{min}} : z_{\text{max}}$$

```
       AABB Box
     +---------+ max (P-vertex for positive normal)
     |         |
     |    *    |
     |         |
     +---------+
    min (N-vertex for positive normal)
```

We evaluate the plane equation for these vertices:
1. **Fully Outside**: If the distance to the P-vertex is negative:
   $$\vec{N} \cdot \vec{P}_{\text{vertex}} + D < 0$$
   The entire bounding box lies in the negative half-space of the plane. The object is completely outside the frustum and is culled immediately.
2. **Intersecting**: If the P-vertex is in front of the plane (positive) but the N-vertex is behind the plane (negative):
   The bounding box overlaps the plane.
3. **Fully Inside**: If the N-vertex is in front of the plane (positive):
   The entire bounding box lies in the positive half-space. If this condition is met for all 6 planes, the object is fully inside the frustum, and we can bypass culling tests for its subcomponents.

---

## 3. Spatial Partitioning: The Quadtree

Frustum culling every object in the world one by one is an $\mathcal{O}(N)$ operation. In large scenes, this creates a CPU bottleneck. To solve this, we use a **Quadtree** in [QuadTree.h](file:///c:/Users/Barnen/Desktop/3D-Project-2025/3DProject/RasterizerDemo/RasterizerDemo/QuadTree.h) to organize objects based on their horizontal positions.

### Structure of the Quadtree
* A **Node** represents a square boundary on the horizontal $XZ$-plane.
* It contains:
  * A bounding box describing its boundary.
  * A list of pointers to game objects located within its boundary.
  * Four pointers to child nodes representing its sub-quadrants (North-West, North-East, South-West, South-East).
* **Division Criteria**: If the number of objects in a node exceeds a limit (e.g., 8 elements) and the node's size is larger than a minimum limit, it splits into four children, distributing its objects to them.

```
+-------------------+-------------------+
|                   |                   |
|    North-West     |    North-East     |
|                   |                   |
+-------------------+-------------------+
|                   |                   |
|    South-West     |    South-East     |
|                   |                   |
+-------------------+-------------------+
```

* **Why 2D Quadtree over 3D Octree?**: An Octree splits space on three axes ($X, Y, Z$), creating 8 children. Since our game environments are horizontally expansive with low vertical variation, partitioning the vertical $Y$-axis adds unnecessary traversal overhead. A 2D Quadtree is faster and easier to manage for these layouts.

---

## 4. Traversal and Culling

During rendering, we query the Quadtree to compile a list of visible objects:

```cpp
void QueryVisible(Node* node, const Frustum& frustum, std::vector<GameObject*>& outList)
{
    // Test node's XZ bounding box against frustum
    if (frustum.IsOutside(node->boundingBox))
    {
        return; // Cull the entire branch
    }

    if (frustum.IsInside(node->boundingBox))
    {
        // Node is fully inside frustum; add all objects in this branch
        AppendAllChildren(node, outList);
        return;
    }

    // Node is intersecting; check individual elements
    for (auto* element : node->elements)
    {
        if (frustum.Intersect(element->boundingBox))
        {
            outList.push_back(element);
        }
    }

    // Recurse into child nodes
    for (int i = 0; i < 4; ++i)
    {
        if (node->children[i])
        {
            QueryVisible(node->children[i], frustum, outList);
        }
    }
}
```

### The Visited Set
Large objects (like bridges or castle walls) can cross quadrant boundaries, so their pointers are stored in multiple leaf nodes. 

To prevent rendering the same object multiple times (which wastes draw calls), we keep a tracking set during traversal:
`std::unordered_set<GameObject*> visited;`
Before adding an object to the output rendering list, we check if it is already in the `visited` set. If it is, we skip it.

---

## Teacher Presentation Tips 🎓

* **Why is it safe to rebuild the Quadtree from scratch every frame in this project?**:
  * *Answer*: Rebuilding a quadtree involves clearing it and re-inserting all dynamic game objects. Since our project has a relatively small number of moving objects (less than 1000), rebuilding takes under a microsecond on the CPU. Incremental updates require complex tree restructuring, boundary checking, and recursive element re-linking. Rebuilding is faster, simpler, and guarantees the tree structure remains balanced.
* **Explain how we optimize memory when rebuilding the Quadtree**:
  * *Answer*: Frequent memory allocations and de-allocations can cause memory fragmentation and CPU cache misses. In our Quadtree, clearing a node only resets its element counts (`elements.clear()`) and does not release the underlying vector capacity. The child node structures are kept allocated in a pre-allocated memory block, reusing the same pointers frame after frame without invoking the OS heap manager.
* **How do we handle objects that are too large to fit in any child quadrant?**:
  * *Answer*: If an object is so large that it overlaps multiple quadrants at the center of a node, it cannot be placed cleanly into a single child. The Quadtree handles this by storing the object in the **parent node** itself, rather than forcing it down to the children. When traversing, parent elements are checked first before recursing.
* **Under what scenario does frustum culling with a Quadtree become slower than brute-force culling?**:
  * *Answer*: If all objects are clustered in a single corner of the map, or if the camera's FOV is extremely wide and captures the entire map, the Quadtree must traverse and test almost every node anyway. The overhead of recursive function calls, tree traversal, and node bounding box checks makes the Quadtree slower than simply running a flat loop over the objects in a brute-force array.
* **What is the mathematical difference between checking a sphere vs. checking an AABB against a frustum plane?**:
  * *Answer*: Sphere-plane checks are computationally cheap. We compute the dot product of the plane normal and the sphere center, add the plane distance constant, and compare the result to the sphere's radius. If the distance is less than negative radius, it is culled:
    $$\vec{N} \cdot \vec{C} + D < -R$$
    Checking an AABB is slightly more expensive because we must evaluate the signs of the plane normal components to locate the P-vertex and N-vertex coordinates before running the distance comparison.
