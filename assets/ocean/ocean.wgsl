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
};

struct VertexOutput {
	@builtin(position) position: vec4f,
  @location(1) uv: vec2f,
};


@group(0) @binding(0) var<uniform> uSceneUniforms: SceneUniforms;
@group(0) @binding(1) var textureSampler: sampler;
@group(0) @binding(2) var depthTexture: texture_depth_2d;
@group(0) @binding(3) var normalTexture: texture_2d<f32>;

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

fn getTriPlanarBlend(normal: vec3<f32>) -> vec3<f32> {
	// in wNorm is the world-space normal of the fragment
    var blending = abs(normal);
	
	// ensure a minimum
    let clamped = vec3<f32>(max(blending.x, 0.00001), max(blending.y, 0.00001), max(blending.z, 0.00001));
    blending = normalize(clamped); 

	// get percentage on each axis
    let b = (blending.x + blending.y + blending.z);
    blending /= vec3(b, b, b);
    return blending;
}

// changes a normal from 0-1 space to -1-+1 space
fn unpackNormal(packednormal: vec4<f32>) -> vec4<f32> {
    return packednormal * 2.0 - 1.0;
}

// Return the normal using the normal map and triplanar mapping
fn get_normal(eyePos: vec3f, hit_point: vec3f, spherePos: vec3f) -> vec3f {
    let normal: vec3f = normalize(hit_point - spherePos);
  
    // GPU Gems 3 blend// Triplanar uvs
    // Triplanar blend of the normal map
    let blend = getTriPlanarBlend(hit_point);
    let uvX = hit_point.zy; // x facing plane
    let uvY = hit_point.xz; // y facing plane
    let uvZ = hit_point.xy; // z facing plane// Tangent space normal maps
    let tnormalX = unpackNormal(textureSample(normalTexture, textureSampler, uvX));
    let tnormalY = unpackNormal(textureSample(normalTexture, textureSampler, uvY));
    let tnormalZ = unpackNormal(textureSample(normalTexture, textureSampler, uvZ));

    // Swizzle tangent normals into world space and zero out "z"
    let normalX = vec3<f32>(0.0, tnormalX.yx);
    let normalY = vec3<f32>(tnormalY.x, 0.0, tnormalY.y);
    let normalZ = vec3<f32>(tnormalZ.xy, 0.0);// Triblend normals and add to world normal
    let normalBlend = normalX.xyz * blend.x + normalY.xyz * blend.y + normalZ.xyz * blend.z + normal;
    let worldNormal = normalize(normalBlend);
    return worldNormal;
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
  let y = -(-1.0 + (2.0*(in.position.y/height)));
  let z = -d;
  var rayDir = vec3f(x, y, z);
  rayDir = normalize((uSceneUniforms.invViewmatrix * vec4f(rayDir, 0.0)).xyz);

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
    depthTexture, textureSampler,
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
    let ocean_distance = dot(ray, normalize(spherePos-eyePos));

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

    // compute the normal of the sphere
    let hit_point = eyePos + ray; // hit point world pos
    let normal: vec3f = get_normal(eyePos, hit_point, spherePos);

    // diffuse component
    let lightDirection = normalize(-uSceneUniforms.lightDirection);
    let incidence = max(dot(lightDirection, vec4f(normal, 0.0)), 0.0);
    let diffuse = vec4f(base_ocean_color * incidence, 1.0);

    // The specular part
    let worldPosition = hit_point;
    let viewDir = normalize(-ray);
    let reflectDir = reflect(uSceneUniforms.lightDirection.xyz, normal);  
    let specular: f32 = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    
    // Final output
    // let light_color = vec4f(1.0);
	  let color: vec4f = diffuse + specular;

    // gamma correction
    let corrected_color = pow(color.xyz, vec3f(2.2));
    
    return vec4f(corrected_color, alpha);
  }
  else
  {
    return vec4f(0.0);
  }
}
