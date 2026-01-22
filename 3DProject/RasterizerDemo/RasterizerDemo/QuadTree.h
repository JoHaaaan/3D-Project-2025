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

// Spatial partitioning structure for efficient frustum culling
template<typename T>
class QuadTree
{
private:
	struct Node
	{
		DirectX::BoundingBox boundingBox;
		std::unique_ptr<Node> children[4];
		std::vector<QuadTreeElement<T>> elements;
		bool isLeaf = true;
	};

	std::unique_ptr<Node> root;
	int maxDepth;
	int maxElementsPerNode;

	void Subdivide(Node* node, int currentDepth);
	void Insert(Node* node, const QuadTreeElement<T>& element, int currentDepth);
	void Query(Node* node, const DirectX::BoundingFrustum& frustum, std::unordered_set<T>& visited, std::vector<T>& result) const;
	void Clear(Node* node);

public:
	QuadTree(const DirectX::BoundingBox& worldBounds, int maxDepth = 5, int maxElementsPerNode = 8);
	~QuadTree();

	void Insert(const T& element, const DirectX::BoundingBox& elementBox);
	void Query(const DirectX::BoundingFrustum& frustum, std::vector<T>& result) const;
	void Clear();
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

	DirectX::XMFLOAT3 center = node->boundingBox.Center;
	DirectX::XMFLOAT3 extents = node->boundingBox.Extents;

	float halfX = extents.x * 0.5f;
	float halfZ = extents.z * 0.5f;

	// Top-Left (-X, +Z)
	node->children[0] = std::make_unique<Node>();
	node->children[0]->boundingBox.Center = DirectX::XMFLOAT3(center.x - halfX, center.y, center.z + halfZ);
	node->children[0]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[0]->isLeaf = true;

	// Top-Right (+X, +Z)
	node->children[1] = std::make_unique<Node>();
	node->children[1]->boundingBox.Center = DirectX::XMFLOAT3(center.x + halfX, center.y, center.z + halfZ);
	node->children[1]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[1]->isLeaf = true;

	// Bottom-Left (-X, -Z)
	node->children[2] = std::make_unique<Node>();
	node->children[2]->boundingBox.Center = DirectX::XMFLOAT3(center.x - halfX, center.y, center.z - halfZ);
	node->children[2]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[2]->isLeaf = true;

	// Bottom-Right (+X, -Z)
	node->children[3] = std::make_unique<Node>();
	node->children[3]->boundingBox.Center = DirectX::XMFLOAT3(center.x + halfX, center.y, center.z - halfZ);
	node->children[3]->boundingBox.Extents = DirectX::XMFLOAT3(halfX, extents.y, halfZ);
	node->children[3]->isLeaf = true;

	node->elements.clear();
}

template<typename T>
void QuadTree<T>::Insert(Node* node, const QuadTreeElement<T>& element, int currentDepth)
{
	if (!node)
		return;

	if (!node->boundingBox.Intersects(element.boundingBox))
		return;

	if (node->isLeaf)
	{
		if (node->elements.size() < static_cast<size_t>(maxElementsPerNode) || currentDepth >= maxDepth)
		{
			node->elements.push_back(element);
		}
		else
		{
			std::vector<QuadTreeElement<T>> oldElements = node->elements;

			Subdivide(node, currentDepth);

			for (const auto& elem : oldElements)
			{
				for (int i = 0; i < 4; ++i)
				{
					if (node->children[i])
					{
						Insert(node->children[i].get(), elem, currentDepth + 1);
					}
				}
			}

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

	if (!frustum.Intersects(node->boundingBox))
		return;

	if (node->isLeaf)
	{
		for (const auto& elem : node->elements)
		{
			if (frustum.Intersects(elem.boundingBox))
			{
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
	std::unordered_set<T> visited;
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
