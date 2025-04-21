# scene

## vertex
	- vec3 pos
	- vec3 normal
	- vec2 texture uv

## mesh
	-  indicesOffset
	- indices
	instancesOffset
	instances

## model
	- texture
	- normal map
	- Mesh[]
	-

## Pool
### vertex
	- low binding frequency (per scene)
### index
	- low binding frequency (per scene)
	- offset for each mesh

### texture
	- medium binding frequency (min. per model, max. per mesh)
	- texture uv's are per vertex data
	- textures may be shared between models
### material
	- low binding frequency (per scene)
	- the push constant should contain the material index
