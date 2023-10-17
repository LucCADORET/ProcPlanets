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

// Inspired by the seb lague code: https://youtu.be/lctXaT9pxA0?si=pKqieIk5W5wFcSOV&t=942
// Returns the dstToSphere and dstThroughSphere
// If inside the sphere, dstToSphere = 0
// If ray misses the sphere: dstToSphere = max float value, dstThroughSphere = 0
// rayDir must be normalized
fn raySphere(radius: f32, rayOrigin: vec3f, rayDir: vec3f) -> vec2f {
  let center: vec3f = vec3f(0.0, 0.0, 0.0);
  let offset: vec3f = rayOrigin - center;
  let a: f32 = 1.0; // set to dot(rayDir, rayDir) if it's not normalized
  let b: f32 = 2.0 * dot(offset, rayDir);
  let c: f32 = dot(offset, offset) - radius * radius;

  let discriminant: f32 = b * b - 4.0 *a * c;
  // No intersections: discriminant < 0
  // 1 Intersection: discriminant == 0
  // 2 Intersections: discriminant > 0

  if(discriminant > 0.0) {
    let s: f32 = sqrt(discriminant);
    let dstToSphereNear = max(0.0, (-b -s) / (2.0 * a));
    let dstToSphereFar = (-b + s) / (2.0 * a);
    if(dstToSphereFar >= 0.0) {
      return vec2f(dstToSphereNear, dstToSphereFar - dstToSphereNear);
    }
  }

  // ray did not intersect sphere
  // We don't  have a f32::MAX yet so we just put a huge number
  return vec2f(10000000000000.0, 0.0);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
  let eyePos: vec3f = uSceneUniforms.viewPosition.xyz; // TODO: rename "eyePos"

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
  rayDir = (vec4f(rayDir, 0.0) * uSceneUniforms.viewMatrix).xyz;

  // Calculate the intersection of the ray with the sphere
  let sphereRadius = 1.5;
  let spherePos = vec3f(0.0, 0.0, 0.0);
  let oc = eyePos - spherePos;
  let a = dot(rayDir, rayDir);
  let b = 2.0 * dot(oc, rayDir);
  let c = dot(oc, oc) - sphereRadius * sphereRadius;
  let discriminant = b * b - 4.0 * a * c;
  
  let scene_depth: f32 = textureSample(
    depthTexture, depthSampler,
    in.uv
  );
  // return vec4f(scene_depth, scene_depth, scene_depth, 1.0);

  // If the ray intersects the sphere, set the pixel color to white
  // TODO: 
  // Otherwise, set the pixel color to black
  if (discriminant > 0.0)
  {     
    let s: f32 = sqrt(discriminant);
    let t1 = (-b - s) / (2.0 * a);
    let t2 = (-b + s) / (2.0 * a);

    // Use the closest intersection point
    let ocean_distance = min(t1, t2);

    // TODO: I have the distance in world space... what now ?
    // Idea: get the pixel position in world space, compute the distance from the eye, and compare to t
    // let upos: vec4f = uSceneUniforms.invProjectionMatrix * vec4(in.position.xy * 2.0 - 1.0, scene_depth, 1.0);
    let upos: vec4f = uSceneUniforms.invProjectionMatrix * vec4(in.uv * 2.0 - 1.0, scene_depth, 1.0);
    let pixel_position: vec3f = upos.xyz / upos.w;
    let planet_distance = length(pixel_position - eyePos);
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
