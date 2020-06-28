// Part[g] -- put items into fshader.
// Part[h] -- adjust specular

varying vec2 texCoord;  // The third coordinate is always 0.0 and is discarded
uniform sampler2D texture;
varying vec3 pos;   //equivalent to fV in Lecture Notes
varying vec3 fN; 

uniform vec3 AmbientProduct, DiffuseProduct, SpecularProduct;

uniform vec4 LightPosition;
uniform vec4 Light_2_Position;
uniform vec4 Light_3_Position;

uniform float Shininess;
uniform float brightness;
uniform float brightness_2;
uniform float brightness_3;
uniform float pitch;
uniform float yaw;
uniform vec3 color_1;
uniform vec3 color_2;
uniform vec3 color_3;
uniform float texScale;

void main()
{	
    
    // The vector to the light from the vertex    
    vec3 Lvec = LightPosition.xyz - pos;
    vec3 Lvec_2 = Light_2_Position.xyz - pos;
    vec3 Lvec_3 = Light_3_Position.xyz - pos;

    vec3 direct; //Directional vector
    direct.x = cos(radians(yaw))*cos(radians(pitch));
    direct.y = sin(radians(pitch));
    direct.z = sin(radians(yaw))*cos(radians(pitch));

    // Unit direction vectors for Blinn-Phong shading calculation
    vec3 L = normalize( Lvec );   // Direction to the light source
    vec3 E = normalize( -pos );   // Direction to the eye/camera
    vec3 H = normalize( L + E );  // Halfway vector

    vec3 L2 = normalize(Lvec_2);
    vec3 H2 = normalize(L2 + E);

    vec3 L3 = normalize( Lvec_3 );   // Direction to the light source

    float theta = dot(L3,normalize(direct)); // For rotational light

    // Transform vertex normal into eye coordinates (assumes scaling
    // is uniform across dimensions)
    vec3 N = normalize(fN);

    // Light source information
    vec3 Light_1_info = color_1 * brightness;
    vec3 Light_2_info = color_2 * brightness_2;
    vec3 Light_3_info = color_3 * brightness_3;
    

    // Compute terms in the illumination equation
    // Ambient calculations
    vec3 ambient =  Light_1_info * AmbientProduct;
    vec3 ambient_2 = Light_2_info * AmbientProduct;
    vec3 ambient_3 = Light_3_info * AmbientProduct;

    //Diffuse calculations
    float Kd = max( dot(L, N), 0.0 );
    float Kd_2 = max( dot(L2, N), 0.0);
    float Kd_3 = max(theta, 0.0);
    vec3  diffuse = Kd * Light_1_info * DiffuseProduct;
    vec3  diffuse_2 = Kd_2 * Light_2_info * DiffuseProduct;
    vec3 diffuse_3 = Kd_3 * Light_3_info * DiffuseProduct;


    // Specular calculation 
    // For part H, multiply by brightness to take it into consideration
    float Ks = pow( max(dot(N, H), 0.0), Shininess );
    float Ks_2 = pow( max(dot(N, H2), 0.0), Shininess);
    float Ks_3 = pow( max(theta,0.0), Shininess);
    vec3  specular = Ks * brightness * SpecularProduct;
    vec3  specular_2 = Ks_2 * brightness_2 * SpecularProduct;
    vec3 specular_3 = Ks_3 * brightness_3 * SpecularProduct;

    
    if (dot(L, N) < 0.0 ) {
	specular = vec3(0.0, 0.0, 0.0);
    } 

    if(dot(L2, N) < 0.0) {
    specular_2 = vec3(0.0, 0.0, 0.0);
    }

    if(theta < 0.0){
    specular_3 = vec3(0.0, 0.0, 0.0);
    }


    // globalAmbient is independent of distance from the light source
    vec3 globalAmbient = vec3(0.05, 0.05, 0.05);

    //Position of lights
    vec3 lpos = LightPosition.xyz;
    vec3 l3pos = Light_3_Position.xyz;

    // Distance between light and object, used in color calculations
    float dist = sqrt((lpos[0]-pos[0])*(lpos[0]-pos[0])+(lpos[1]-pos[1])*(lpos[1]-pos[1])+(lpos[2]-pos[2])*(lpos[2]-pos[2]));
    float dist_3 = sqrt((l3pos[0]-pos[0])*(l3pos[0]-pos[0])+(l3pos[1]-pos[1])*(l3pos[1]-pos[1])+(l3pos[2]-pos[2])*(l3pos[2]-pos[2]));

    //Custom chosen values for attenuation
    float distscale = 1.0/(1.0+0.14*dist + 0.07*dist*dist); //Value derived from table - https://learnopengl.com/Lighting/Light-casters
    float distscale_3 = 1.0/(1.0+0.14*dist_3 + 0.07*dist_3*dist_3); //Value derived from table - https://learnopengl.com/Lighting/Light-casters

    vec4 color_3;

    if(theta > 0.7) {
        color_3 = vec4(ambient_3 + distscale_3*(diffuse_3), 1.0); //original
    }
    else {
        color_3 = vec4(ambient_3, 1.0);
    }

    // distscale accounts for light source distance
    vec4 color = vec4(ambient + distscale*( diffuse), 1.0); //original
    vec4 color_2 = vec4((ambient_2 + diffuse_2), 1.0);

    gl_FragColor = vec4(globalAmbient, 1.0) + (color + color_2 + color_3) * texture2D( texture, texCoord * texScale ) + vec4((distscale * specular) + specular_2 + (distscale_3 * specular_3), 1.0);

}
