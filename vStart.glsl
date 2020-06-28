//Modified for Part[g] - Light calculations removed
attribute vec3 vPosition;
attribute vec3 vNormal;
attribute vec2 vTexCoord;

varying vec2 texCoord;
varying vec3 pos;
varying vec3 fN;

uniform mat4 ModelView;
uniform mat4 Projection;

//uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;
//uniform float Shininess;
//varying vec4 color;

void main()
{
    //Items commented out for putting in fshader (Part G)
    vec4 vpos = vec4(vPosition, 1.0);

    // Transform vertex position into eye coordinates
    pos = (ModelView * vpos).xyz;

    // Transform vertex normal into eye coordinates (assumes scaling
    // is uniform across dimensions)
    fN = normalize( (ModelView*vec4(vNormal, 0.0)).xyz );

    // The vector to the light from the vertex    
    //Lvec = LightPosition.xyz - pos;
    //
    // Distance between light and object
    //float dist = sqrt((lpos[0]-pos[0])*(lpos[0]-pos[0])+(lpos[1]-pos[1])*(lpos[1]-pos[1])+(lpos[2]-pos[2])*(lpos[2]-pos[2]));
    //float distscale = 1.0/(dist*dist);
    //
    // Unit direction vectors for Blinn-Phong shading calculation
    //vec3 L = normalize( Lvec );   // Direction to the light source
    //vec3 E = normalize( -pos );   // Direction to the eye/camera
    //vec3 H = normalize( L + E );  // Halfway vector
    //
    // Compute terms in the illumination equation
    //vec3 ambient = AmbientProduct;
    //
    //float Kd = max( dot(L, N), 0.0 );
    //vec3  diffuse = Kd * DiffuseProduct;
    //
    //float Ks = pow( max(dot(N, H), 0.0), Shininess );
    //vec3  specular = Ks * SpecularProduct;
    //     
    //if (dot(L, N) < 0.0 ) {
	//specular = vec3(0.0, 0.0, 0.0);
    //} 
    //
    // globalAmbient is independent of distance from the light source
    //vec3 globalAmbient = vec3(0.1, 0.1, 0.1);
    //
    // distscale accounts for light source distance
    //color.rgb = globalAmbient + distscale*(ambient + diffuse + specular);
    //color.a = 1.0;

    gl_Position = Projection * ModelView * vpos;
    texCoord = vTexCoord;
}
