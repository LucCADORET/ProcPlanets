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
@group(0) @binding(1) var textureSampler: sampler;
@group(0) @binding(2) var baseColorTexture: texture_2d<f32>;
@group(0) @binding(3) var normalTexture: texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uSceneUniforms.projectionMatrix * uSceneUniforms.viewMatrix * uSceneUniforms.modelMatrix * vec4f(in.position, 1.0);
	out.tangent = (uSceneUniforms.modelMatrix * vec4<f32>(in.tangent, 0.0)).xyz;
	out.bitangent = (uSceneUniforms.modelMatrix * vec4<f32>(in.bitangent, 0.0)).xyz;
    out.normal = (uSceneUniforms.modelMatrix * vec4<f32>(in.normal, 0.0)).xyz;
	out.color = in.color;
	out.uv = in.uv;
	out.worldPosition = uSceneUniforms.modelMatrix * vec4f(in.position, 1.0);
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {

	// Sample the normal mapping
	let encodedN = textureSample(normalTexture, textureSampler, in.uv).rgb;
	let localN = encodedN - 0.5;

	// Rotate the normal with the TBN matrix
	let rotation = mat3x3<f32>(
		normalize(in.tangent),
		normalize(in.bitangent),
		normalize(in.normal),
	);
	let rotatedN = normalize(rotation * localN);
	let normalMapStrength = 0.5;
	let N = mix(in.normal, rotatedN, normalMapStrength);

	// diffuse component
	let lightIntensity = vec3f(0.5, 0.5, 0.5);
	let lightDirection = normalize(-uSceneUniforms.lightDirection);
	let incidence = max(dot(lightDirection, vec4f(N, 0.0)), 0.0);
	let diffuse = incidence * lightIntensity;

	// // The specular part
	let viewDir = normalize(uSceneUniforms.viewPosition - in.worldPosition);
	let reflectDir = reflect(uSceneUniforms.lightDirection.xyz, N);  
	let specular = pow(max(dot(viewDir.xyz, reflectDir), 0.0), 54.0);
	
	// Sample texture
	let baseColor = textureSample(baseColorTexture, textureSampler, in.uv).rgb;

	let kd = 1.0;
	let ks = 0.9;

	// Combine texture and lighting
	let color = baseColor * kd * diffuse + ks * specular;
	// return vec4<f32>(baseColor, 1.0);
	return vec4<f32>(color, 1.0);
}
