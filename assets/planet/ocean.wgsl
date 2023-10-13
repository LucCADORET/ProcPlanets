/**
 * A structure holding the value of our uniforms
 */
struct SceneUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    lightViewProjMatrix: mat4x4f,
    color: vec4f,
    lightDirection: vec4f,
    baseColor: vec4f,
    viewPosition: vec4f,
    time: f32,
};

@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;

@vertex
fn vs_main(@builtin(vertex_index) VertexIndex : u32) -> @builtin(position) vec4f {
  var pos = array<vec2<f32>, 6>(
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
	vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
  );

  return vec4<f32>(pos[VertexIndex], 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {	
	return vec4f(0.5, 0.6, 0.7, 0.5);
}
