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
	@location(4) shadowPos: vec3f,
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
    oceanColor: vec4f,
    oceanRadius: f32,
};

@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;
@group(0) @binding(1) var shadowSampler: sampler_comparison;
@group(0) @binding(2) var shadowMap: texture_depth_2d;

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

	  // XY is in (-1, 1) space, Z is in (0, 1) space
	let posFromLight = uSceneUniforms.lightViewProjMatrix * uSceneUniforms.modelMatrix * vec4(in.position, 1.0);

	// Convert XY to (0, 1) for fetching the texture
	// Y is flipped because texture coords are Y-down.
	out.shadowPos = vec3(
		posFromLight.xy * vec2(0.5, -0.5) + vec2(0.5),
		posFromLight.z
	);
	return out;
}

// Most of the code for shadow was taken from there: https://webgpu.github.io/webgpu-samples/samples/shadowMapping
fn shadowCalculation(shadowPos: vec3f) -> f32 {
	let shadowDepthTextureSize: f32 = 1024.0;

  	// // Percentage-closer filtering. Sample texels in the region
	// // to smooth the result.
	// var visibility = 0.0;
	// let oneOverShadowDepthTextureSize = 1.0 / shadowDepthTextureSize;
	// for (var y = -1; y <= 1; y++) {
	// 	for (var x = -1; x <= 1; x++) {
	// 		let offset = vec2<f32>(vec2(x, y)) * oneOverShadowDepthTextureSize;

	// 		visibility += textureSampleCompare(
	// 			shadowMap, shadowSampler,
	// 			shadowPos.xy + offset, shadowPos.z - 0.007
	// 		);
	// 	}
	// }
	// visibility /= 9.0;
	// let lambertFactor = max(dot(normalize(scene.lightPos - input.fragPos), input.fragNorm), 0.0);
	// let lightingFactor: f32 = min(ambientFactor + visibility * lambertFactor, 1.0);

	// returns 0 if the texture sample is less than the compare value, 1 if it's higher
	let visibility = textureSampleCompare(
		shadowMap, shadowSampler,
		shadowPos.xy, shadowPos.z - 0.007
	);
	return visibility;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let albedo = uSceneUniforms.baseColor;
	let normal = normalize(in.normal);

	// diffuse component
	let lightDirection = normalize(-uSceneUniforms.lightDirection);
	let incidence = max(dot(lightDirection, vec4f(normal, 0.0)), 0.0);
	let diffuse = vec4f(albedo.xyz * incidence, 1.0);

	// // The specular part
	let viewDir = normalize(uSceneUniforms.viewPosition - in.worldPosition);
	let reflectDir = reflect(uSceneUniforms.lightDirection.xyz, normal);  
	let specular = pow(max(dot(viewDir.xyz, reflectDir), 0.0), 16.0);

	// Shadow computation
	let lightingFactor: f32 = shadowCalculation(in.shadowPos);       

	// Final output
	let color: vec4f = (lightingFactor * (diffuse + specular));

	// gamma correction
    let corrected_color = pow(color, vec4f(2.2));
	
	return vec4f(corrected_color.xyz, 1.0);
}
