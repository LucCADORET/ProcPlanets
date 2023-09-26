struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) worldPosition: vec4f,
};

/**
 * A structure holding the value of our uniforms
 */
struct SceneUniforms {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    color: vec4f,
	lightDirection: vec4f,
	baseColor: vec4f,
	viewPosition: vec4f,
    time: f32,
};

@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uSceneUniforms.projectionMatrix * uSceneUniforms.viewMatrix * uSceneUniforms.modelMatrix * vec4f(in.position, 1.0);

	// get the normal in world coordinate
	// note: this actually will not work if there is a scaling change involved
	// (see the part about the model matrix here: https://learnopengl.com/Lighting/Basic-Lighting)
    out.normal = (uSceneUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;
	out.uv = in.uv;
	out.worldPosition = uSceneUniforms.modelMatrix * vec4f(in.position, 1.0);
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let ambiant = uSceneUniforms.baseColor;

	// diffuse component
	let lightIntensity = vec3f(0.5, 0.5, 0.5);
	let lightDirection = normalize(-uSceneUniforms.lightDirection);
	let incidence = max(dot(lightDirection, vec4f(in.normal, 0.0)), 0.0);
	let diffuse = vec4f(incidence * lightIntensity, 1.0);

	// // The specular part
	let viewDir = normalize(uSceneUniforms.viewPosition - in.worldPosition);
	let reflectDir = reflect(uSceneUniforms.lightDirection.xyz, in.normal);  
	let specular = pow(max(dot(viewDir.xyz, reflectDir), 0.0), 16.0);

	// return ambiant + diffuse + specular;
	return ambiant + diffuse + specular;
}
