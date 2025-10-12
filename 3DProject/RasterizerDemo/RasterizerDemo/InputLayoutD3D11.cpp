#include "InputLayoutD3D11.h"

InputLayoutD3D11::~InputLayoutD3D11()
{
	if (inputLayout)
		inputLayout->Release();
}

void InputLayoutD3D11::AddInputElement(const std::string& semanticName, DXGI_FORMAT format)
{
	// Store the semantic name string (pointers in DESC need to remain valid)
	semanticNames.push_back(semanticName);

	D3D11_INPUT_ELEMENT_DESC element = {};
	element.SemanticName = semanticNames.back().c_str();
	element.SemanticIndex = 0;
	element.Format = format;
	element.InputSlot = 0;
	element.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
	element.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
	element.InstanceDataStepRate = 0;

	elements.push_back(element);
}

void InputLayoutD3D11::FinalizeInputLayout(ID3D11Device* device, const void* vsDataPtr, size_t vsDataSize)
{
	device->CreateInputLayout(
		elements.data(),
		static_cast<UINT>(elements.size()),
		vsDataPtr,
		vsDataSize,
		&inputLayout
	);
}

ID3D11InputLayout* InputLayoutD3D11::GetInputLayout() const
{
	return inputLayout;
}
