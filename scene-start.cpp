
#include "Angel.h"

#include <stdlib.h>
#include <dirent.h>
#include <time.h>

#include <cmath>

// Open Asset Importer header files (in ../../assimp--3.0.1270/include)
// This is a standard open source library for loading meshes, see gnatidread.h
#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

GLint windowHeight=640, windowWidth=960;

// gnatidread.cpp is the CITS3003 "Graphics n Animation Tool Interface & Data
// Reader" code.  This file contains parts of the code that you shouldn't need
// to modify (but, you can).
#include "gnatidread.h"

using namespace std;        // Import the C++ standard functions (e.g., min) 


// IDs for the GLSL program and GLSL variables.
GLuint shaderProgram; // The number identifying the GLSL shader program
GLuint vPosition, vNormal, vTexCoord; // IDs for vshader input vars (from glGetAttribLocation)
GLuint projectionU, modelViewU; // IDs for uniform variables (from glGetUniformLocation)

static float viewDist = 1.5; // Distance from the camera to the centre of the scene
static float camRotSidewaysDeg=0; // rotates the camera sideways around the centre
static float camRotUpAndOverDeg=20; // rotates the camera up and over the centre.

mat4 projection; // Projection matrix - set in the reshape function
mat4 view; // View matrix - set in the display function.

// These are used to set the window title
char lab[] = "Project1";
char *programName = NULL; // Set in main 
int numDisplayCalls = 0; // Used to calculate the number of frames per second

//------Meshes----------------------------------------------------------------
// Uses the type aiMesh from ../../assimp--3.0.1270/include/assimp/mesh.h
//                           (numMeshes is defined in gnatidread.h)
aiMesh* meshes[numMeshes]; // For each mesh we have a pointer to the mesh to draw
GLuint vaoIDs[numMeshes]; // and a corresponding VAO ID from glGenVertexArrays

// -----Textures--------------------------------------------------------------
//                           (numTextures is defined in gnatidread.h)
texture* textures[numTextures]; // An array of texture pointers - see gnatidread.h
GLuint textureIDs[numTextures]; // Stores the IDs returned by glGenTextures

//------Scene Objects---------------------------------------------------------
//
// For each object in a scene we store the following
// Note: the following is exactly what the sample solution uses, you can do things differently if you want.
typedef struct {
    vec4 loc;
    float scale;
    float angles[3]; // rotations around X, Y and Z axes.
    float diffuse, specular, ambient; // Amount of each light component
    float shine;
    vec3 rgb;
    float brightness; // Multiplies all colours
    int meshId;
    int texId;
    float texScale;
} SceneObject;

const int maxObjects = 1024; // Scenes with more than 1024 objects seem unlikely

SceneObject sceneObjs[maxObjects]; // An array storing the objects currently in the scene.
int nObjects = 0;    // How many objects are currenly in the scene.
int currObject = -1; // The current object
int toolObj = -1;    // The object currently being modified


int menu_in_use = 0; // For checking menu is open or not //KV

//----------------------------------------------------------------------------
//
// Loads a texture by number, and binds it for later use.    
void loadTextureIfNotAlreadyLoaded(int i)
{
    if (textures[i] != NULL) return; // The texture is already loaded.

    textures[i] = loadTextureNum(i); CheckError();
    glActiveTexture(GL_TEXTURE0); CheckError();

    // Based on: http://www.opengl.org/wiki/Common_Mistakes
    glBindTexture(GL_TEXTURE_2D, textureIDs[i]); CheckError();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textures[i]->width, textures[i]->height,
                 0, GL_RGB, GL_UNSIGNED_BYTE, textures[i]->rgbData); CheckError();
    glGenerateMipmap(GL_TEXTURE_2D); CheckError();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); CheckError();
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); CheckError();

    glBindTexture(GL_TEXTURE_2D, 0); CheckError(); // Back to default texture
}

//------Mesh loading----------------------------------------------------------
//
// The following uses the Open Asset Importer library via loadMesh in 
// gnatidread.h to load models in .x format, including vertex positions, 
// normals, and texture coordinates.
// You shouldn't need to modify this - it's called from drawMesh below.

void loadMeshIfNotAlreadyLoaded(int meshNumber)
{
    if (meshNumber>=numMeshes || meshNumber < 0) {
        printf("Error - no such model number");
        exit(1);
    }

    if (meshes[meshNumber] != NULL)
        return; // Already loaded

    aiMesh* mesh = loadMesh(meshNumber);
    meshes[meshNumber] = mesh;

    glBindVertexArray( vaoIDs[meshNumber] );

    // Create and initialize a buffer object for positions and texture coordinates, initially empty.
    // mesh->mTextureCoords[0] has space for up to 3 dimensions, but we only need 2.
    GLuint buffer[1];
    glGenBuffers( 1, buffer );
    glBindBuffer( GL_ARRAY_BUFFER, buffer[0] );
    glBufferData( GL_ARRAY_BUFFER, sizeof(float)*(3+3+3)*mesh->mNumVertices,
                  NULL, GL_STATIC_DRAW );

    int nVerts = mesh->mNumVertices;
    // Next, we load the position and texCoord data in parts.    
    glBufferSubData( GL_ARRAY_BUFFER, 0, sizeof(float)*3*nVerts, mesh->mVertices );
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(float)*3*nVerts, sizeof(float)*3*nVerts, mesh->mTextureCoords[0] );
    glBufferSubData( GL_ARRAY_BUFFER, sizeof(float)*6*nVerts, sizeof(float)*3*nVerts, mesh->mNormals);

    // Load the element index data
    GLuint elements[mesh->mNumFaces*3];
    for (GLuint i=0; i < mesh->mNumFaces; i++) {
        elements[i*3] = mesh->mFaces[i].mIndices[0];
        elements[i*3+1] = mesh->mFaces[i].mIndices[1];
        elements[i*3+2] = mesh->mFaces[i].mIndices[2];
    }

    GLuint elementBufferId[1];
    glGenBuffers(1, elementBufferId);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBufferId[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * mesh->mNumFaces * 3, elements, GL_STATIC_DRAW);

    // vPosition it actually 4D - the conversion sets the fourth dimension (i.e. w) to 1.0                 
    glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0) );
    glEnableVertexAttribArray( vPosition );

    // vTexCoord is actually 2D - the third dimension is ignored (it's always 0.0)
    glVertexAttribPointer( vTexCoord, 3, GL_FLOAT, GL_FALSE, 0,
                           BUFFER_OFFSET(sizeof(float)*3*mesh->mNumVertices) );
    glEnableVertexAttribArray( vTexCoord );
    glVertexAttribPointer( vNormal, 3, GL_FLOAT, GL_FALSE, 0,
                           BUFFER_OFFSET(sizeof(float)*6*mesh->mNumVertices) );
    glEnableVertexAttribArray( vNormal );
    CheckError();
}

//----------------------------------------------------------------------------

static void mouseClickOrScroll(int button, int state, int x, int y)
{

    if (button==GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (glutGetModifiers()!=GLUT_ACTIVE_SHIFT) activateTool(button);
        else activateTool(GLUT_LEFT_BUTTON);

    }

    else if (button==GLUT_LEFT_BUTTON && state == GLUT_UP) deactivateTool();

    else if (button==GLUT_MIDDLE_BUTTON && state==GLUT_DOWN) { 
        activateTool(button);
    }

    else if (button==GLUT_MIDDLE_BUTTON && state==GLUT_UP) deactivateTool();

    else if (button == 3) { // scroll up
        viewDist = (viewDist < 0.0 ? viewDist : viewDist*0.8) - 0.05;
    }

    else if (button == 4) { // scroll down
        viewDist = (viewDist < 0.0 ? viewDist : viewDist*1.25) + 0.05;
    }

}

//----------------------------------------------------------------------------
void myLeftClickFunc(int button, int state, int x, int y){}

//----------------------------------------------------------------------------

static void mousePassiveMotion(int x, int y)
{
    mouseX=x;
    mouseY=y;
}

//----------------------------------------------------------------------------

mat2 camRotZ()
{
    return rotZ(-camRotSidewaysDeg) * mat2(10.0, 0, 0, -10.0);
}

//------callback functions for doRotate below and later-----------------------

static void adjustCamrotsideViewdist(vec2 cv)
{
    cout << cv << endl;
    camRotSidewaysDeg+=cv[0]; viewDist+=cv[1];
}

static void adjustcamSideUp(vec2 su)
{
    camRotSidewaysDeg+=su[0]; camRotUpAndOverDeg+=su[1];
}
    
static void adjustLocXZ(vec2 xz)
{
    sceneObjs[toolObj].loc[0]+=xz[0]; sceneObjs[toolObj].loc[2]+=xz[1];
}

static void adjustScaleY(vec2 sy)
{
    sceneObjs[toolObj].scale+=sy[0]; sceneObjs[toolObj].loc[1]+=sy[1];
}


//----------------------------------------------------------------------------
//------Set the mouse buttons to rotate the camera----------------------------
//------around the centre of the scene.---------------------------------------
//----------------------------------------------------------------------------

static void doRotate()
{
    setToolCallbacks(adjustCamrotsideViewdist, mat2(400,0,0,-2),
                     adjustcamSideUp, mat2(400, 0, 0,-90) );
}
                                     
//------Add an object to the scene--------------------------------------------

static void addObject(int id)
{

    vec2 currPos = currMouseXYworld(camRotSidewaysDeg);
    sceneObjs[nObjects].loc[0] = currPos[0];
    sceneObjs[nObjects].loc[1] = 0.0;
    sceneObjs[nObjects].loc[2] = currPos[1];
    sceneObjs[nObjects].loc[3] = 1.0;

    if (id!=0 && id!=55)
        sceneObjs[nObjects].scale = 0.005;

    sceneObjs[nObjects].rgb[0] = 0.7; sceneObjs[nObjects].rgb[1] = 0.7;
    sceneObjs[nObjects].rgb[2] = 0.7; sceneObjs[nObjects].brightness = 1.0;

    sceneObjs[nObjects].diffuse = 1.0; sceneObjs[nObjects].specular = 0.5;
    sceneObjs[nObjects].ambient = 0.7; sceneObjs[nObjects].shine = 10.0;

    sceneObjs[nObjects].angles[0] = 0.0; sceneObjs[nObjects].angles[1] = 180.0;
    sceneObjs[nObjects].angles[2] = 0.0;

    sceneObjs[nObjects].meshId = id;
    sceneObjs[nObjects].texId = rand() % numTextures;
    sceneObjs[nObjects].texScale = 2.0;

    toolObj = currObject = nObjects++;
    setToolCallbacks(adjustLocXZ, camRotZ(),
                     adjustScaleY, mat2(0.05, 0, 0, 10.0) );


    glutPostRedisplay();
}

//------The init function-----------------------------------------------------
GLuint transformID;

// Modified for Part[i] and Part[j]
void init( void )
{
    srand ( time(NULL) ); /* initialize random seed - so the starting scene varies */
    aiInit();

    // for (int i=0; i < numMeshes; i++)
    //     meshes[i] = NULL;

    glGenVertexArrays(numMeshes, vaoIDs); CheckError(); // Allocate vertex array objects for meshes
    glGenTextures(numTextures, textureIDs); CheckError(); // Allocate texture objects

    // Load shaders and use the resulting shader program
    shaderProgram = InitShader( "vStart.glsl", "fStart.glsl" );

    glUseProgram( shaderProgram ); CheckError();

    // Initialize the vertex position attribute from the vertex shader        
    vPosition = glGetAttribLocation( shaderProgram, "vPosition" );
    vNormal = glGetAttribLocation( shaderProgram, "vNormal" ); CheckError();

    // Likewise, initialize the vertex texture coordinates attribute.    
    vTexCoord = glGetAttribLocation( shaderProgram, "vTexCoord" );
    CheckError();

    projectionU = glGetUniformLocation(shaderProgram, "Projection");
    modelViewU = glGetUniformLocation(shaderProgram, "ModelView");

    transformID = glGetUniformLocation(shaderProgram,"rotation");

    // Objects 0, and 1 are the ground and the first light.
    addObject(0); // Square for the ground
    sceneObjs[0].loc = vec4(0.0, 0.0, 0.0, 1.0);
    sceneObjs[0].scale = 10.0;
    sceneObjs[0].angles[0] = 90.0; // Rotate it.
    sceneObjs[0].texScale = 5.0; // Repeat the texture.

    addObject(55); // Sphere for the first light
    sceneObjs[1].loc = vec4(2.0, 1.0, 1.0, 1.0);
    sceneObjs[1].scale = 0.1;
    sceneObjs[1].texId = 0; // Plain texture
    sceneObjs[1].brightness = 0.4; // The light's brightness is 5 times this (below).

    addObject(55); // Sphere for the second light - KV
    sceneObjs[2].loc = vec4(2.0, 2.0, 1.0, 1.0); // Added height to show in scene
    sceneObjs[2].scale = 0.1;
    sceneObjs[2].texId = 0; // Plain texture
    sceneObjs[2].brightness = 0.2; 

    addObject(55); // Third rotational light - KV
    sceneObjs[3].loc = vec4(2.0, 3.0, 1.0, 1.0); // Added height to see in scene
    sceneObjs[3].scale = 0.1;
    sceneObjs[3].texId = 0; // Plain texture
    sceneObjs[3].brightness = 0.2; 

    addObject(rand() % numMeshes); // A test mesh

    // We need to enable the depth test to discard fragments that
    // are behind previously drawn fragments for the same pixel.
    glEnable( GL_DEPTH_TEST );
    doRotate(); // Start in camera rotate mode.
    glClearColor( 0.0, 0.0, 0.0, 1.0 ); /* black background */
}

//----------------------------------------------------------------------------

//Modified for Part[b]
void drawMesh(SceneObject sceneObj)
{

    // Activate a texture, loading if needed.
    loadTextureIfNotAlreadyLoaded(sceneObj.texId);
    glActiveTexture(GL_TEXTURE0 );
    glBindTexture(GL_TEXTURE_2D, textureIDs[sceneObj.texId]);

    // Texture 0 is the only texture type in this program, and is for the rgb
    // colour of the surface but there could be separate types for, e.g.,
    // specularity and normals.
    glUniform1i( glGetUniformLocation(shaderProgram, "texture"), 0 );

    // Set the texture scale for the shaders
    glUniform1f( glGetUniformLocation( shaderProgram, "texScale"), sceneObj.texScale ); // For shaders? texScale isn't there?

    // Set the projection matrix for the shaders
    glUniformMatrix4fv( projectionU, 1, GL_TRUE, projection );


    // Set the model matrix - this should combine translation, rotation and scaling based on what's
    // in the sceneObj structure (see near the top of the program).
    
    //Form angles matrix to manipulate mesh
    // Part[b]

    mat4 xRotation = RotateX(sceneObj.angles[0]);
    mat4 yRotation = RotateY(sceneObj.angles[1]);
    mat4 zRotation = RotateZ(sceneObj.angles[2]);
    mat4 rotationMatrix = xRotation * yRotation * zRotation; // Matrix for manipulating the rotation matrix.

    // Final matrix required for transformation.
    mat4 model = Translate(sceneObj.loc) * rotationMatrix * Scale(sceneObj.scale);


    // Set the model-view matrix for the shaders
    glUniformMatrix4fv( modelViewU, 1, GL_TRUE, view * model );

    //For rotating view about the vertical axis
    //glUniformMatrix4fv( rotateView, 1, GL_TRUE, *model); //REMOVE KUSHIL!

    // Activate the VAO for a mesh, loading if needed.
    loadMeshIfNotAlreadyLoaded(sceneObj.meshId);
    CheckError();
    glBindVertexArray( vaoIDs[sceneObj.meshId] );
    CheckError();

    glDrawElements(GL_TRIANGLES, meshes[sceneObj.meshId]->mNumFaces * 3,
                   GL_UNSIGNED_INT, NULL);
    CheckError();
}


//----------------------------------------------------------------------------

// Modified for Part[a]
// Modified for Part[i]
// Modified for Part[h]
void display( void )
{
    numDisplayCalls++;

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    CheckError(); // May report a harmless GL_INVALID_OPERATION with GLEW on the first frame

    // Set the view matrix. To start with this just moves the camera
    // backwards.  You'll need to add appropriate rotations.

    view = Translate(0.0, 0.0, -viewDist);

    //Rotations - Part[a]
    mat4 yaw = RotateY(camRotSidewaysDeg);
    mat4 pitch = RotateX(camRotUpAndOverDeg);

    view = view * pitch * yaw;

    //glUniformMatrix3fv(transformID, 1, GL_FALSE, mTransform);
    //Rotation

    // Light 1
    SceneObject lightObj1 = sceneObjs[1]; 
    vec4 lightPosition = view * lightObj1.loc;

    // Light 2 is independent of the light sources position, therefore we take into account the camera as it is moved 
    // around the frame i.e yaw and pitch.
    mat4 light_2_movement = pitch * yaw;
    SceneObject light_2 = sceneObjs[2];
    vec4 light_2_pos = light_2_movement * light_2.loc;

    // Light 3
    SceneObject lightObj3 = sceneObjs[3];
    float light_pitch = lightObj3.angles[1]; //Equivalent to pitch of light 
    float light_yaw = lightObj3.angles[2]; //Equivalent to yaw of light
    vec4 light_3_pos = view * lightObj3.loc;



    glUniform4fv( glGetUniformLocation(shaderProgram, "LightPosition"), 1, lightPosition);
    CheckError();

    glUniform4fv( glGetUniformLocation(shaderProgram, "Light_2_Position"), 1, light_2_pos);
    CheckError();

    glUniform4fv( glGetUniformLocation(shaderProgram, "Light_3_Position"), 1, light_3_pos);
    CheckError();

    // For Part[h]
    glUniform1f(glGetUniformLocation(shaderProgram,"brightness"), lightObj1.brightness);
    CheckError();

    glUniform1f(glGetUniformLocation(shaderProgram, "brightness_2"), light_2.brightness);
    CheckError();
    
    glUniform1f(glGetUniformLocation(shaderProgram, "brightness_3"), lightObj3.brightness);
    CheckError();

    glUniform1f(glGetUniformLocation(shaderProgram, "pitch"), light_pitch);
    CheckError();

    glUniform1f(glGetUniformLocation(shaderProgram, "yaw"), light_yaw);
    CheckError();

    glUniform3fv(glGetUniformLocation(shaderProgram, "color_1"),1, lightObj1.rgb);
    CheckError();

    glUniform3fv(glGetUniformLocation(shaderProgram, "color_2"),1, light_2.rgb);
    CheckError();

    glUniform3fv(glGetUniformLocation(shaderProgram, "color_3"),1, lightObj3.rgb);
    CheckError();

    for (int i=0; i < nObjects; i++) {
        SceneObject so = sceneObjs[i];

        vec3 rgb = so.rgb  * so.brightness  * 2.0;
        glUniform3fv( glGetUniformLocation(shaderProgram, "AmbientProduct"), 1, so.ambient * rgb );
        CheckError();
        glUniform3fv( glGetUniformLocation(shaderProgram, "DiffuseProduct"), 1, so.diffuse * rgb );
        glUniform3fv( glGetUniformLocation(shaderProgram, "SpecularProduct"), 1, so.specular * rgb );
        glUniform1f( glGetUniformLocation(shaderProgram, "Shininess"), so.shine );
        CheckError();

        drawMesh(sceneObjs[i]);
    }

    glutSwapBuffers();
}

//----------------------------------------------------------------------------
//------Menus-----------------------------------------------------------------
//----------------------------------------------------------------------------

static void objectMenu(int id)
{
    deactivateTool();
    addObject(id);
}

static void texMenu(int id)
{
    deactivateTool();
    if (currObject>=0) {
        sceneObjs[currObject].texId = id;
        glutPostRedisplay();
    }
}

static void groundMenu(int id)
{
    deactivateTool();
    sceneObjs[0].texId = id;
    glutPostRedisplay();
}

// Modified to ensure brightness doesn't go below 0.
static void adjustBrightnessY(vec2 by)
{
    sceneObjs[toolObj].brightness+=by[0];
    if(sceneObjs[toolObj].brightness<=0){
        sceneObjs[toolObj].brightness=0;
    }
    sceneObjs[toolObj].loc[1]+=by[1];
}

static void adjustRedGreen(vec2 rg)
{
    sceneObjs[toolObj].rgb[0]+=rg[0];
    sceneObjs[toolObj].rgb[1]+=rg[1];
}

static void adjustBlueBrightness(vec2 bl_br)
{
    sceneObjs[toolObj].rgb[2]+=bl_br[0];
    sceneObjs[toolObj].brightness+=bl_br[1];
}

// Code for Part[c]
// This is a helper function, which works to modify the Ambience and Diffuse of the program
// when selected. This function is called in the materialMenu function.
static void AmbientDiffuseModification(vec2 ambdif) {
    sceneObjs[toolObj].ambient +=  ambdif[0];
    sceneObjs[toolObj].diffuse +=  ambdif[1];
}

// Code for Part[c]
// This is a helper function, which works to modify the Specular and Shine of the program
// when selected. This function is called in the materialMenu function.
static void SpecularShineModification(vec2 specshi) {
    sceneObjs[toolObj].specular += specshi[0];
    sceneObjs[toolObj].shine += specshi[1];
}

//Code for Part[j]
//Rotates light
static void RotateObj(vec2 xz)
{
    sceneObjs[toolObj].angles[2]+=-20*xz[0]; 
    sceneObjs[toolObj].angles[1]+=-20*xz[1];
}

// Modified for Part[i]
static void lightMenu(int id)
{
    deactivateTool();
    if (id == 70) { //Must this be edited? For light movement?
        toolObj = 1;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustBrightnessY, mat2( 1.0, 0.0, 0.0, 10.0) );
    }

    else if (id >= 71 && id <= 74) {
        toolObj = 1;
        setToolCallbacks(adjustRedGreen, mat2(1.0, 0.0, 0.0, 1.0),
                         adjustBlueBrightness, mat2(1.0, 0.0, 0.0, 1.0) );
    }

    //Light 2
    else if (id == 80) { // Copy of code above, except manipulates now controls second light
        toolObj = 2; // Refers to second object in frame, i.e second light source
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustBrightnessY, mat2(1.0, 0.0, 0.0, 10.0) );
    }

    else if (id == 81) { // Copy of code above, except manipulates now controls second light
        toolObj = 2; 
        setToolCallbacks(adjustRedGreen, mat2(1.0, 0.0, 0.0, 1.0),
                         adjustBlueBrightness, mat2(1.0, 0.0, 0.0, 1.0) );
    }

    //Light 3
    else if (id == 90){
        toolObj = 3;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustBrightnessY, mat2( 1.0, 0.0, 0.0, 10.0) );
    }

    //Light 3
    else if (id == 91){
        toolObj = 3;
        setToolCallbacks(adjustRedGreen, mat2(1.0, 0.0, 0.0, 1.0),
                         adjustBlueBrightness, mat2(1.0, 0.0, 0.0, 1.0) );        
    }

    //Light 3 Rotate
    else if (id == 92){
    	toolObj = 3;
    	setToolCallbacks(RotateObj, camRotZ(),
                         adjustBrightnessY, mat2( 1.0, 0.0, 0.0, 10.0) );
    }

    else {
        printf("Error in lightMenu\n");
        exit(1);
    }
}

static int createArrayMenu(int size, const char menuEntries[][128], void(*menuFn)(int))
{
    int nSubMenus = (size-1)/10 + 1;
    int subMenus[nSubMenus];

    for (int i=0; i < nSubMenus; i++) {
        subMenus[i] = glutCreateMenu(menuFn);
        for (int j = i*10+1; j <= min(i*10+10, size); j++)
            glutAddMenuEntry( menuEntries[j-1] , j);
        CheckError();
    }
    int menuId = glutCreateMenu(menuFn);

    for (int i=0; i < nSubMenus; i++) {
        char num[6];
        sprintf(num, "%d-%d", i*10+1, min(i*10+10, size));
        glutAddSubMenu(num,subMenus[i]);
        CheckError();
    }
    return menuId;
}


//Modified for Part[c]
static void materialMenu(int id)
{
    deactivateTool();
    if (currObject < 0) return;

    if (id==10) {
        toolObj = currObject;
        setToolCallbacks(adjustRedGreen, mat2(1, 0, 0, 1),
                         adjustBlueBrightness, mat2(1, 0, 0, 1) );
    }

    //Code for Part[c]
    else if (id == 20) { //This elif, is created to modify ambience, diffuse, specular and shine.
            toolObj = currObject;
            setToolCallbacks(AmbientDiffuseModification, mat2(2.0, 0.0, 0.0, 10.0),
                    SpecularShineModification, mat2(2.0, 0.0, 0.0, 10.0));
        }
    
    // You'll need to fill in the remaining menu items here.                                                
    else {
        printf("Error in materialMenu\n");
    }
}


static void adjustAngleYX(vec2 angle_yx)
{
    sceneObjs[currObject].angles[1]+=angle_yx[0];
    sceneObjs[currObject].angles[0]+=angle_yx[1];
}

static void adjustAngleZTexscale(vec2 az_ts)
{
    sceneObjs[currObject].angles[2]+=az_ts[0];
    sceneObjs[currObject].texScale+=az_ts[1];
}

static void mainmenu(int id)
{
    deactivateTool();
    if (id == 41 && currObject>=0) {
        toolObj=currObject;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                         adjustScaleY, mat2(0.05, 0, 0, 10) );
    }
    if (id == 50)
        doRotate();
    if (id == 55 && currObject>=0) {
        setToolCallbacks(adjustAngleYX, mat2(400, 0, 0, -400),
                         adjustAngleZTexscale, mat2(400, 0, 0, 15) );
    }
    if (id == 99) exit(0);
}

// Additional function to allow manipulation of a specific object chosen from menu
int selected_object = 0;
static void select_object(int id)
{
    deactivateTool();
    if(id >= 200){ // or 204
        selected_object = id;
        currObject = toolObj = selected_object - 200;
    }
}

// Created for Part[j]
static void manipulate_objs(int id){
    if(selected_object == 0 || selected_object < 204){
        return ;
    }

    deactivateTool();

    if(id == 66){ // Duplicate Object
        vec2 currPos = currMouseXYworld(camRotSidewaysDeg);
        int duplicate_object_pos = selected_object - 200; // Reverse offset

        sceneObjs[nObjects] = sceneObjs[duplicate_object_pos];
        sceneObjs[nObjects].loc[0] = currPos[0]+ 0.01; // Offsets position of new object to demonstrate it has been duplicated.
        sceneObjs[nObjects].loc[2] = currPos[1] + 0.01; // Offsets position of new object to demonstrate it has been duplicated.

        // Essentially replicating function to add new object to scene.
        toolObj = currObject = nObjects++;
        setToolCallbacks(adjustLocXZ, camRotZ(),
                     adjustScaleY, mat2(0.05, 0, 0, 10.0) );

        selected_object = 0; // No object is now selected.

        glutPostRedisplay();
    }

    if(id == 67){ // Delete Object
        //cout << "menu in use = " << sceneObs...; //test
        //int shuffle;
        if(nObjects<=4){ //No objects to delete
            return;
        }

        if((selected_object - 200) != (nObjects-1)){
            for(int i=0 ; i<((nObjects-1)-(selected_object-200)+1); i++){
                sceneObjs[selected_object-200+i] = sceneObjs[selected_object-200+1+i]; //Shuffle items to keep indexing correct.
            }
        }

        else if((selected_object - 200) == (nObjects-1)){
            sceneObjs[selected_object-200] = sceneObjs[selected_object-200+1];
        }

        nObjects = nObjects - 1;
        if(nObjects>4){
            select_object(nObjects-1+200);
            doRotate();
        }

        else{
            currObject = -1;
            toolObj = -1;
            selected_object = 0;
            doRotate();
        }
    
        glutPostRedisplay();

    }
}

// Function created to check if menu is in use, else we can't configure it.
static void menu_status(int status, int x, int y)
{
    if(status == GLUT_MENU_IN_USE){
        menu_in_use = 1;
    }
    
    else{
        menu_in_use = 0;
    }
}

// Modified for Part[j]
static void makeMenu()
{

    int objectId = createArrayMenu(numMeshes, objectMenuEntries, objectMenu);

    int materialMenuId = glutCreateMenu(materialMenu);
    glutAddMenuEntry("R/G/B/All",10);
    glutAddMenuEntry("Ambient/Diffuse/Specular/Shine",20); // Fixed

    int texMenuId = createArrayMenu(numTextures, textureMenuEntries, texMenu);
    int groundMenuId = createArrayMenu(numTextures, textureMenuEntries, groundMenu);

    int selection_Id = glutCreateMenu(select_object);
    //glutAddMenuEntry("Objects",14);
    

    for(int i=4; i < nObjects; i++){
        char fig_name[248];
        strcpy(fig_name, objectMenuEntries[sceneObjs[i].meshId-1]); 
        // Ensures that original name is not manipulated. 
        int new_objects_id = i+200; // corresponds to actual objects in array + 200+4
        //Shifted by 200, to account for other objects which may be in scene.
        // Also account for fact that other items have high id's.
        glutAddMenuEntry(strcat(fig_name, textureMenuEntries[sceneObjs[i].texId-1]), new_objects_id); // Makes object easy to identify from scene.

     }

    int manipulate_objects_id = glutCreateMenu(manipulate_objs);
    glutAddMenuEntry("Duplicate Object", 66);
    glutAddMenuEntry("Delete Object", 67);
    

    int lightMenuId = glutCreateMenu(lightMenu);
    glutAddMenuEntry("Move Light 1",70);
    glutAddMenuEntry("R/G/B/All Light 1",71);
    glutAddMenuEntry("Move Light 2",80);
    glutAddMenuEntry("R/G/B/All Light 2",81);
    glutAddMenuEntry("Move Light 3",90);
    glutAddMenuEntry("R/G/B/All Light 3",91);
    glutAddMenuEntry("Rotate Rotational Light", 92); // Added KV


    glutCreateMenu(mainmenu);
    glutAddMenuEntry("Rotate/Move Camera",50);
    glutAddSubMenu("Add object", objectId);
    glutAddMenuEntry("Position/Scale", 41);
    glutAddMenuEntry("Rotation/Texture Scale", 55);
    glutAddSubMenu("Material", materialMenuId);
    glutAddSubMenu("Texture",texMenuId);
    glutAddSubMenu("Ground Texture",groundMenuId);
    glutAddSubMenu("Lights",lightMenuId);
    glutAddSubMenu("Select Object", selection_Id);
    glutAddSubMenu("Manipulate Object", manipulate_objects_id);


    glutAddMenuEntry("EXIT", 99);
    glutAttachMenu(GLUT_RIGHT_BUTTON);
    glutMenuStateFunc(menu_status); //Helps prevent error when refreshing menu
}

// When up key is pressed, menu is refreshed.
void up_func(int key, int x, int y){
    switch(key){
        case GLUT_KEY_UP:
            if(menu_in_use == 1){
                break;
            }
            else if(menu_in_use == 0){
                makeMenu();
                break;
        }
    }
}
//----------------------------------------------------------------------------

void keyboard( unsigned char key, int x, int y )
{
    switch ( key ) {
        case 033:
            exit( EXIT_SUCCESS );
            break;
    }
}

//----------------------------------------------------------------------------

void idle( void )
{
    glutPostRedisplay();
}

//----------------------------------------------------------------------------

//Modified Part[d]
void reshape( int width, int height )
{
    windowWidth = width;
    windowHeight = height;

    glViewport(0, 0, width, height);

    // You'll need to modify this so that the view is similar to that in the
    // sample solution.
    // In particular: 
    //     - the view should include "closer" visible objects (slightly tricky)
    //     - when the width is less than the height, the view should adjust so
    //         that the same part of the scene is visible across the width of
    //         the window.

    //Frustrum(l,r,top,bottom...)

    //Value modified for Part[d]
    GLfloat nearDist = 0.01;

    // Check width against height first to account for window resizing.
    // Modified for Part[e]
    if(width < height){
        // Essentially reversal of original projection.
        projection = Frustum(-nearDist, nearDist,
                    -nearDist*(float)height/(float)width,
                    nearDist*(float)height/(float)width,
                    0.01, 100.0);
    }

    else{
        projection = Frustum(-nearDist*(float)width/(float)height,
                    nearDist*(float)width/(float)height,
                    -nearDist, nearDist,
                    0.01, 100.0);
    }
}

//----------------------------------------------------------------------------

void timer(int unused)
{
    char title[256];
    sprintf(title, "%s %s: %d Frames Per Second @ %d x %d",
                    lab, programName, numDisplayCalls, windowWidth, windowHeight );

    glutSetWindowTitle(title);

    numDisplayCalls = 0;
    glutTimerFunc(1000, timer, 1);
}

//----------------------------------------------------------------------------

char dirDefault1[] = "models-textures";
char dirDefault3[] = "/tmp/models-textures";
char dirDefault4[] = "/d/models-textures";
char dirDefault2[] = "/cslinux/examples/CITS3003/project-files/models-textures";

void fileErr(char* fileName)
{
    printf("Error reading file: %s\n", fileName);
    printf("When not in the CSSE labs, you will need to include the directory containing\n");
    printf("the models on the command line, or put it in the same folder as the exectutable.");
    exit(1);
}

//----------------------------------------------------------------------------

// Modified for Part[j]
int main( int argc, char* argv[] )
{
    // Get the program name, excluding the directory, for the window title
    programName = argv[0];
    for (char *cpointer = argv[0]; *cpointer != 0; cpointer++)
        if (*cpointer == '/' || *cpointer == '\\') programName = cpointer+1;

    // Set the models-textures directory, via the first argument or some handy defaults.
    if (argc > 1)
        strcpy(dataDir, argv[1]);
    else if (opendir(dirDefault1)) strcpy(dataDir, dirDefault1);
    else if (opendir(dirDefault2)) strcpy(dataDir, dirDefault2);
    else if (opendir(dirDefault3)) strcpy(dataDir, dirDefault3);
    else if (opendir(dirDefault4)) strcpy(dataDir, dirDefault4);
    else fileErr(dirDefault1);

    glutInit( &argc, argv );
    glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );
    glutInitWindowSize( windowWidth, windowHeight );

    //glutInitContextVersion( 3, 2);
    glutInitContextProfile( GLUT_CORE_PROFILE );            // May cause issues, sigh, but you
    // glutInitContextProfile( GLUT_COMPATIBILITY_PROFILE ); // should still use only OpenGL 3.2 Core
                                                            // features.
    glutCreateWindow( "Initialising..." );

    glewInit(); // With some old hardware yields GL_INVALID_ENUM, if so use glewExperimental.
    CheckError(); // This bug is explained at: http://www.opengl.org/wiki/OpenGL_Loading_Library

    init();
    CheckError();

    glutDisplayFunc( display );
    glutKeyboardFunc( keyboard );
    glutIdleFunc( idle );

    // Added for Part[j]
    glutSpecialFunc(up_func);

    glutMouseFunc( mouseClickOrScroll );
    glutPassiveMotionFunc(mousePassiveMotion);
    glutMotionFunc( doToolUpdateXY );
 
    glutReshapeFunc( reshape );
    glutTimerFunc( 1000, timer, 1 );
    CheckError();

    makeMenu();
    CheckError();

    glutMainLoop();
    return 0;
}
