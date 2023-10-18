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
    fov: f32,
    width: f32,
    height: f32,
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

  // Build the ray dir: remap fragment position to 0,0, 
  // Good resource here: https://computergraphics.stackexchange.com/questions/8479/how-to-calculate-ray
  let fov = radians(uSceneUniforms.fov);
  let d = 1.0/tan(fov/2.0);
  let width = uSceneUniforms.width;
  let height = uSceneUniforms.height;
  let aspect_ratio = width/height;
  let x = aspect_ratio*(-1.0 + 2.0 * in.position.x/width);
  let y = -1.0 + (2.0*(in.position.y/height));
  let z = -d;
  var rayDir = vec3f(x, y, z);
  rayDir = normalize((vec4f(rayDir, 0.0) * uSceneUniforms.viewMatrix).xyz);

  // Calculate the intersection of the ray with the sphere
  // TODO: the sphere radius should be a parameter !
  let sphereRadius = 1.5;
  let spherePos = vec3f(0.0, 0.0, 0.0);
  let oc: vec3f = eyePos - spherePos;
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
    let solution = min(t1, t2);

    // get the ocean distance from the solution
    // "you probably want to take the dot product instead of the euclidian distance if you just want the "distance parallel to the camera's forward vector""
    let ray = solution * rayDir;
    let ocean_distance = dot(ray, normalize(-eyePos));

    // project the scene depth into camera space
    let upos: vec4f = uSceneUniforms.invProjectionMatrix * vec4(in.uv * 2.0 - 1.0, scene_depth, 1.0);

    // invert z to be the right way
    // this it the actual planet pixel world position
    let planet_pixel_position: vec3f = (upos.xyz / upos.w) * vec3f(1.0, 1.0, -1.0);

    // planet is closer that ocean: show the earth
    if (planet_pixel_position.z < ocean_distance) {
      return vec4f(0.0);
    }

    // adjusting the transparency depending on the depth
    let depth = planet_pixel_position.z - ocean_distance;
    let max_depth = 0.2;
    let alpha = clamp(depth / max_depth, 0.3, 0.8);

    // blue ocean color
    let base_ocean_color = vec3f(0.00, 0.55, 1.00);

    // gamma correction
    let corrected_color = pow(base_ocean_color, vec3f(2.2));
    
    return vec4f(corrected_color, alpha);
  }
  else
  {
    return vec4f(0.0);
  }
}
