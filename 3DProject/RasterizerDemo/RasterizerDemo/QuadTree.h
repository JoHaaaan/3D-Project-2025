#pragma once

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <unordered_set>

// Helper structure to store element with its bounding box
template<typename T>
struct QuadTreeElement
{
	T data;
	DirectX::BoundingBox boundingBox;

	QuadTreeElement(const T& d, const DirectX::BoundingBox& box)
		: data(d), boundingBox(box) {
	}
};

// Template QuadTree class to handle generic data types
template<typename T>
class QuadTree
{
private:
	struct Node
	{
		DirectX::BoundingBox boundingBox;
		std::unique_ptr<Node> children[4];  // 4 children for a QuadTree
		std::vector<QuadTreeElement<T>> elements; // Objects stored in this node with their bounding boxes
		bool isLeaf = true;
	};

	std::unique_ptr<Node> root;
	int maxDepth;
	int maxElementsPerNode;

	// Helper methods
	void Subdivide(Node* node, int currentDepth);
	void Insert(Node* node, const QuadTreeElement<T>& element, int currentDepth);
	void Query(Node* node, const DirectX::BoundingFrustum& frustum, std::unordered_set<T>& visited, std::vector<T>& result) const;
	void Clear(Node* node);

public:
	QuadTree(const DirectX::BoundingBox& worldBounds, int maxDepth = 5, int maxElementsPerNode = 8);
	~QuadTree();

	// Insert an element with its bounding box
	void Insert(const T& element, const DirectX::BoundingBox& elementBox);

	// Query elements that intersect with the frustum
	void Query(const DirectX::BoundingFrustum& frustum, std::vector<T>& result) const;

	// Clear all elements from the tree
	void Clear();

	// Rebuild the tree
	void Rebuild(const DirectX::BoundingBox& worldBounds);
};

// Template implementation must be in header
template<typename T>
QuadTree<T>::QuadTree(const DirectX::BoundingBox& worldBounds, int maxDepth, int maxElementsPerNode)
	: maxDepth(maxDepth), maxElementsPerNode(maxElementsPerNode)
{
	root = std::make_unique<Node>();
	root->boundingBox = worldBounds;
	root->isLeaf = true;
}

template<typename T>
QuadTree<T>::~QuadTree()
{
	Clear();
}

template<typename T>
void QuadTree<T>::Subdivide(Node* node, int currentDepth)
{
	if (!node->isLeaf || currentDepth >= maxDepth)
		return;

	node->isLeaf = false;

	// Get parent bounds
	DirectX::XMFLOAT3 center = node->boundingBox.Center;
	DirectX::XMFLOAT3 extents = node->boundingBox.Extents;

	// Calculate new extents (half in X and Z, same in Y)
	float halfX = extents.x * 0.5f;
	float halfZ = extents.z * 0.5f;

	// Create 4 child nodes (quadrants)
	// Child 0: Top-Left (-X, +Z)
	node->children[0] = std::make_unique<Node>();
	node->children[0]->boundingBox.Center = DirectX::XMFLOAT3(center.x - halfX, center.y, center.z + halfZ);
	node->children[0]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[0]->isLeaf = true;

	// Child 1: Top-Right (+X, +Z)
	node->children[1] = std::make_unique<Node>();
	node->children[1]->boundingBox.Center = DirectX::XMFLOAT3(center.x + halfX, center.y, center.z + halfZ);
	node->children[1]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[1]->isLeaf = true;

	// Child 2: Bottom-Left (-X, -Z)
	node->children[2] = std::make_unique<Node>();
	node->children[2]->boundingBox.Center = DirectX::XMFLOAT3(center.x - halfX, center.y, center.z - halfZ);
	node->children[2]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[2]->isLeaf = true;

	// Child 3: Bottom-Right (+X, -Z)
	node->children[3] = std::make_unique<Node>();
	node->children[3]->boundingBox.Center = DirectX::XMFLOAT3(center.x + halfX, center.y, center.z - halfZ);
	node->children[3]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[3]->isLeaf = true;

	// Note: Elements will be moved to children when we re-insert them
	node->elements.clear();
}

template<typename T>
void QuadTree<T>::Insert(Node* node, const QuadTreeElement<T>& element, int currentDepth)
{
	if (!node)
		return;

	// COLLISION CHECK: Check if element intersects this node's bounding box
	if (!node->boundingBox.Intersects(element.boundingBox))
		return; // If not, discard

	// LEAF CHECK
	if (node->isLeaf)
	{
		// Check if we have space (below max object limit)
		if (node->elements.size() < static_cast<size_t>(maxElementsPerNode) || currentDepth >= maxDepth)
		{
			// Add the object to elements
			node->elements.push_back(element);
		}
		else
		{
			// FIX: Save existing elements BEFORE calling Subdivide, 
			// because Subdivide() clears node->elements.
			std::vector<QuadTreeElement<T>> oldElements = node->elements;

			// Node is full - SPLIT it
			Subdivide(node, currentDepth);

			// node->elements.clear(); // This is now redundant as Subdivide clears it, but harmless.

			for (const auto& elem : oldElements)
			{
				// Re-insert into children
				for (int i = 0; i < 4; ++i)
				{
					if (node->children[i])
					{
						Insert(node->children[i].get(), elem, currentDepth + 1);
					}
				}
			}

			// Attempt to add the new object again (into children)
			for (int i = 0; i < 4; ++i)
			{
				if (node->children[i])
				{
					Insert(node->children[i].get(), element, currentDepth + 1);
				}
			}
		}
		
	}
	else
	{
		// PARENT CHECK: If the node is already a parent, recurse into its children
		for (int i = 0; i < 4; ++i)
		{
			if (node->children[i])
			{
				Insert(node->children[i].get(), element, currentDepth + 1);
			}
		}
	}
}

template<typename T>
void QuadTree<T>::Insert(const T& element, const DirectX::BoundingBox& elementBox)
{
	QuadTreeElement<T> treeElement(element, elementBox);
	Insert(root.get(), treeElement, 0);
}

template<typename T>
void QuadTree<T>::Query(Node* node, const DirectX::BoundingFrustum& frustum, std::unordered_set<T>& visited, std::vector<T>& result) const
{
	if (!node)
		return;

	// Check for collision between the Frustum and the Node::boundingBox
	// If they don't intersect, return immediately (cull this branch)
	if (!frustum.Intersects(node->boundingBox))
		return;

	// If they do intersect:
	if (node->isLeaf)
	{
		// It's a LEAF: Check intersection between the Frustum and each object in elements
		for (const auto& elem : node->elements)
		{
			// Perform per-object frustum culling
			if (frustum.Intersects(elem.boundingBox))
			{
				// Check if we've already added this element (duplicate protection)
				if (visited.find(elem.data) == visited.end())
				{
					visited.insert(elem.data);
					result.push_back(elem.data);
				}
			}
		}
	}
	else
	{
		// It's a PARENT: Recurse into all children
		for (int i = 0; i < 4; ++i)
		{
			if (node->children[i])
			{
				Query(node->children[i].get(), frustum, visited, result);
			}
		}
	}
}

template<typename T>
void QuadTree<T>::Query(const DirectX::BoundingFrustum& frustum, std::vector<T>& result) const
{
	result.clear();
	std::unordered_set<T> visited;  // Track processed elements to avoid duplicates
	Query(root.get(), frustum, visited, result);
}

template<typename T>
void QuadTree<T>::Clear(Node* node)
{
	if (!node)
		return;

	for (int i = 0; i < 4; ++i)
	{
		if (node->children[i])
		{
			Clear(node->children[i].get());
			node->children[i].reset();
		}
	}

	node->elements.clear();
	node->isLeaf = true;
}

template<typename T>
void QuadTree<T>::Clear()
{
	if (root)
	{
		Clear(root.get());
	}
}

template<typename T>
void QuadTree<T>::Rebuild(const DirectX::BoundingBox& worldBounds)
{
	Clear();
	root->boundingBox = worldBounds;
	root->isLeaf = true;
}
