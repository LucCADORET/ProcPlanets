struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
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
    width: f32,
    height: f32,
    oceanRadius: f32,
};

@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;

@vertex
fn vs_main(in: VertexInput) -> @builtin(position) vec4f {
	return uSceneUniforms.lightViewProjMatrix * uSceneUniforms.modelMatrix * vec4f(in.position, 1.0);
}

