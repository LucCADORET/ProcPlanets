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

struct VertexOutput {
	@builtin(position) position: vec4f,
};


@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;

@vertex
fn vs_main(@builtin(vertex_index) VertexIndex : u32) -> VertexOutput {
	var out: VertexOutput;
  var pos = array<vec2<f32>, 6>(
    vec2(-1.0, 1.0),
    vec2(-1.0, -1.0),
    vec2(1.0, -1.0),
	vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0),
  );

  out.position = vec4<f32>(pos[VertexIndex], 0.0, 1.0);
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
      return vec2f(dstToSphereNear, dstToSphereNear - dstToSphereFar);
    }
  }

  // ray did not intersect sphere
  // We don't  have a f32::MAX yet so we just put a huge number
  return vec2f(10000000000000.0, 0.0);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
  let rayPos: vec3f = uSceneUniforms.viewPosition.xyz;

  // TODO: in the future, should depend on the current res
  // remap to -1,1 + normalize the direciton vector
  let resolution = vec2f(1600.0, 900.0);
  let uv: vec2f = (-1.0 + 2.0*in.position.xy / resolution.xy) * 
		vec2f(resolution.x/resolution.y, 1.0);
  var rayDir: vec3f = normalize(vec3f(uv, 1.0));

  // Calculate the intersection of the ray with the sphere
  let sphereRadius = 2.0;
  let spherePos = vec3f(0.0, 0.0, 0.0);
  let oc = rayPos - spherePos;
  let a = dot(rayDir, rayDir);
  let b = 2.0 * dot(oc, rayDir);
  let c = dot(oc, oc) - sphereRadius * sphereRadius;
  let discriminant = b * b - 4.0 * a * c;

  // If the ray intersects the sphere, set the pixel color to white
  // Otherwise, set the pixel color to black
  if (discriminant > 0.0)
  {
      return vec4f(1.0, 1.0, 1.0, 1.0);
  }
  else
  {
      return vec4f(0.0, 0.0, 0.0, 0.0);
  }
  

  // // Compute the distance to the sphere
  // // let hitInfo: vec2f = raySphere(2.0, rayPos, rayDir);
  // let dstToOcean = hitInfo.x;
  // let dstThroughOcean = hitInfo.y;

  // // // TODO: use the actual depth values
  // if (dstToOcean < 1000.0) {
  //   return vec4f(0.00, 0.00, 1.00, 0.5);
  // }
  
  // // TODO: return the original view texture
	// // return vec4f(0.5, 0.6, 0.7, 0.0);
	// return vec4f(x, y, 0.0, 1.0);
}
