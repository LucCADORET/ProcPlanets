/**
 * A structure holding the value of our uniforms
 */
struct SceneUniforms {
    projectionMatrix: mat4x4f,
    invProjectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
    lightViewProjMatrix: mat4x4f,
    color: vec4f,
    lightDirection: vec4f,
    baseColor: vec4f,
    viewPosition: vec4f,
    time: f32,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(1) worldPosition: vec4f,
  @location(2) uv: vec2f,
};


@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;
@group(0) @binding(1) var depthSampler: sampler;
@group(0) @binding(2) var depthTexture: texture_depth_2d;

@vertex
fn vs_main(@builtin(vertex_index) VertexIndex : u32) -> VertexOutput {
	var out: VertexOutput;

  // the vertices that build the plane in front of the camera
  var pos = array<vec2<f32>, 6>(
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
	vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
  );

  out.position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);

  // invert y and put in 0,1 range
  out.uv = out.position.xy * vec2(0.5, -0.5) + vec2(0.5);
  return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
  let eyePos: vec3f = uSceneUniforms.viewPosition.xyz;

  // TODO: in the future, should depend on the current res
  // TODO: the FOV should also be a parameter
  // Build the ray dir: remap fragment position to 0,0, 
  // Good resource here: https://computergraphics.stackexchange.com/questions/8479/how-to-calculate-ray
  let fov = radians(45.0);
  let d = 1.0/tan(fov/2.0);
  let width = 1600.0;
  let height = 900.0;
  let aspect_ratio = width/height;
  let x = aspect_ratio*(-1.0 + 2.0 * in.position.x/width);
  let y = -1.0 + (2.0*(in.position.y/height));
  let z = -d;
  var rayDir = vec3f(x, y, z);
  rayDir = normalize((vec4f(rayDir, 0.0) * uSceneUniforms.viewMatrix).xyz);

  // Calculate the intersection of the ray with the sphere
  let sphereRadius = 1.5;
  let spherePos = vec3f(0.0, 0.0, 0.0);
  let oc = eyePos - spherePos;
  let a = 1.0; // works because rayDir is normed
  let b = 2.0 * dot(oc, rayDir);
  let c = dot(oc, oc) - sphereRadius * sphereRadius;
  let discriminant = b * b - 4.0 * a * c;
  
  let scene_depth: f32 = textureSample(
    depthTexture, depthSampler,
    in.uv
  );

  // For debugging the depth
  // return vec4f(scene_depth, scene_depth, scene_depth, 1.0);

  // Discriminant > 0.0 means solutions in front of us
  if (discriminant > 0.0)
  {     
    // Find the closes of the quadratic solution
    let s: f32 = sqrt(discriminant);
    let t1 = (-b - s) / (2.0 * a);
    let t2 = (-b + s) / (2.0 * a);
    let ocean_distance = min(t1, t2);

    // project the scene depth into camera space
    let upos: vec4f = uSceneUniforms.invProjectionMatrix * vec4(in.uv * 2.0 - 1.0, scene_depth, 1.0);
    let pixel_position: vec3f = upos.xyz / upos.w;
    let planet_distance = -pixel_position.z;
    if (planet_distance < ocean_distance) {
      return vec4f(0.0, 0.00, 1.00, 0.0);
    }
    return vec4f(0.0, 0.00, 1.00, 0.5);
  }
  else
  {
    return vec4f(0.0, 0.0, 0.0, 0.0);
  }
}
