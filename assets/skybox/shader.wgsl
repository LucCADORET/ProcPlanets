struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
	@location(4) tangent: vec3f,
	@location(5) bitangent: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) worldPosition: vec4f,
	@location(4) tangent: vec3<f32>,
	@location(5) bitangent: vec3<f32>,
	@location(6) cubeTextureCoord: vec3f, // direction vector representing a 3D texture coordinate
};

/**
 * A structure holding the value of our uniforms
 */
struct SceneUniforms {
    projectionMatrix: mat4x4f,
    invProjectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
	invViewmatrix: mat4x4f,
    modelMatrix: mat4x4f,
	lightViewProjMatrix: mat4x4f,
    color: vec4f,
	lightDirection: vec4f,
	baseColor: vec4f,
	viewPosition: vec4f,
    time: f32,
    fov: f32,
	// terrainShininess: f32,
	// terrainKSpecular: f32,
    width: f32,
    height: f32,
    oceanColor: vec4f,
    oceanRadius: f32,
    oceanShininess: f32,
    oceanKSpecular: f32,
};

@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;
@group(0) @binding(1) var textureSampler: sampler;
@group(0) @binding(2) var baseColorTexture: texture_cube<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

	// the viewMatrix shouldnt include the translation part (as if we were looking from the origin)
	out.position = uSceneUniforms.projectionMatrix * uSceneUniforms.viewMatrix * vec4f(in.position, 1.0);
	
	// the texture coordinate is simply the direction from the origin
	// so in other words, the vertex position when the cube is at the origin
	out.cubeTextureCoord = in.position.xyz;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	
	// Sample texture
	return textureSample(baseColorTexture, textureSampler, in.cubeTextureCoord);

}
