# Shader Naming Conventions

This document outlines the standardized naming conventions used across all HLSL shaders in the project.

## Structure Names

### Input/Output Structures
- **Vertex Shader**: `VS_INPUT`, `VS_OUTPUT`
- **Hull Shader**: `HS_INPUT`, `HS_OUTPUT`, `HS_CONSTANT_OUTPUT`
- **Domain Shader**: `DS_OUTPUT`
- **Geometry Shader**: `GS_INPUT`, `GS_OUTPUT`
- **Pixel Shader**: `PS_INPUT`, `PS_OUTPUT`
- **Compute Shader**: Use descriptive names (e.g., `Particle`, `LightData`)

## Constant Buffer Naming

All constant buffers use descriptive names with "Buffer" suffix:
- `MatrixBuffer`
- `MaterialBuffer`
- `CameraBuffer`
- `LightingToggleBuffer`
- `ParticleCameraBuffer`
- `TimeBuffer`

## Variable Naming

### General Rules
- Use **camelCase** for all variables
- Use descriptive, full names (no abbreviations unless common)
- Constants use **UPPER_SNAKE_CASE**

### Common Variables
- `worldPosition` - Position in world space
- `worldNormal` - Normal in world space
- `clipPosition` - Position in clip space (for SV_POSITION)
- `viewDirection` - View direction vector
- `lightDirection` - Light direction vector
- `diffuseColor` - Diffuse color value
- `specularPower` - Specular exponent
- `specularPacked` - Packed specular value (0-1 range)
- `normalizedNormal` - Normalized normal vector
- `texColor` - Texture sample color

### Avoid Reserved Keywords
Never use these as variable names in HLSL:
- `point` ? use `centeredUV`, `position`, etc.
- `vector` ? use `toPosition`, `direction`, etc.
- `distance` ? use `distanceToPlane`, `distanceValue`, etc.

## Semantic Naming

Use consistent semantics across all shaders:
- `SV_POSITION` - Clip space position (output only)
- `POSITION` - Local/world space position
- `WORLD_POSITION` - World space position (custom semantic)
- `NORMAL` - Normal vector
- `TEXCOORD0` - Primary texture coordinates (use number suffix for multiple)
- `COLOR` - Color values
- `SV_Target` or `SV_Target0/1/2` - Render target output

## Comment Style

### File Header
Each shader file should start with a descriptive comment:
```hlsl
// [Shader Type] [Shader Name]
// Brief description of what the shader does
```

### Section Comments
Use clear section headers for major blocks:
```hlsl
// Calculate diffuse lighting
// Transform to world space
// Parallax Occlusion Mapping parameters
```

### Inline Comments
- Use inline comments for non-obvious operations
- Place on same line for short clarifications
- Place above line for longer explanations

## Constant Naming

Static constants use **UPPER_SNAKE_CASE**:
- `HEIGHT_SCALE`
- `MIN_LAYERS`
- `MAX_LAYERS`
- `PHONG_ALPHA`
- `LIGHT_DIRECTION`
- `LIGHT_COLOR`
- `AMBIENT_STRENGTH`
- `FEATHER`
- `NUM_CONTROL_POINTS`

## Padding Variables

Constant buffer padding follows this pattern:
- `padding` - Single generic padding
- `padding1`, `padding2` - Multiple padding in same buffer
- `padding_[BufferName]` - Descriptive padding (e.g., `padding_Camera`, `padding_Toggle`)
- `padding0`, `padding1` - Numbered padding for multiple fields

## Function Naming

Functions use **PascalCase**:
- `ComputeTBN()`
- `ParallaxOcclusionMapping()`
- `CalculateTessellationFactor()`
- `ProjectToPlane()`
- `CalculateSpotlight()`
- `CalculateShadow()`
- `RespawnParticle()`
- `Random3D()`
- `Hash()`

## Texture and Resource Naming

### Textures
Use descriptive camelCase names with type suffix:
- `diffuseTexture`
- `normalMapTexture`
- `normalHeightTexture`
- `reflectionTexture`
- `shaderTexture`
- `shadowMaps` (for arrays)

### Samplers
Use descriptive names:
- `samplerState` - Standard sampler
- `shadowSampler` - Comparison sampler for shadows

### Buffers
Use descriptive names with type prefix or suffix:
- `StructuredBuffer<T>` ? `Particles`, `lights`
- `RWStructuredBuffer<T>` ? `Particles` (for compute shaders)
- `RWTexture2D<T>` ? `outColor`

## G-Buffer Naming

Consistent naming for deferred rendering targets:
- **RT0 (Albedo)**: `Albedo` - RGB = diffuse color, A = ambient strength
- **RT1 (Normal)**: `Normal` - RGB = packed normal (0-1), A = specular strength  
- **RT2 (Extra)**: `Extra` - RGB = world position, A = packed shininess

Input textures:
- `gAlbedo`, `gNormal`, `gWorldPos`

## Loop and Control Flow

- Loop counters: `i`, `j`, `k` for simple loops
- Descriptive names for complex iterations: `lightIndex`, `vertexIndex`
- Use `numLayers`, `particleCount`, `numLights` for counts

## Summary

The key principles are:
1. **Consistency** - Use the same patterns across all shaders
2. **Clarity** - Names should be self-documenting
3. **Standards** - Follow common HLSL conventions
4. **Avoid Keywords** - Never use reserved HLSL keywords as variable names
5. **Descriptive** - Favor clarity over brevity

Following these conventions ensures that all shaders are easy to read, maintain, and debug.
