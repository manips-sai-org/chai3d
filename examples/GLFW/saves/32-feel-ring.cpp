//------------------------------------------------------------------------------
#include "chai3d.h"
//------------------------------------------------------------------------------
#include <GLFW/glfw3.h>
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
//------------------------------------------------------------------------------

#include <fstream>  // <-- needed for std::ifstream, std::ofstream
#include <sstream>  // <-- needed for std::istringstream
#include <iostream> // <-- needed for std::cout, std::endl
#include <vector>   // <-- needed for std::vector
#include <string>   // <-- needed for std::string

//------------------------------------------------------------------------------
// GENERAL SETTINGS
//------------------------------------------------------------------------------

// stereo Mode
/*
    C_STEREO_DISABLED:            Stereo is disabled
    C_STEREO_ACTIVE:              Active stereo for OpenGL NVDIA QUADRO cards
    C_STEREO_PASSIVE_LEFT_RIGHT:  Passive stereo where L/R images are rendered next to each other
    C_STEREO_PASSIVE_TOP_BOTTOM:  Passive stereo where L/R images are rendered above each other
*/
cStereoMode stereoMode = C_STEREO_DISABLED;

// fullscreen mode
bool fullscreen = false;

// mirrored display
bool mirroredDisplay = false;

enum cMode
{
    IDLE,
    GOING_TO_START,
    PLAYING,
    GAME_OVER
};

//------------------------------------------------------------------------------
// CUSTOM VARIABLES
//------------------------------------------------------------------------------
double Z_ROTATION_DEG = 0.1;
double CURRENT_AZIMUTH_ANGLE = 20;
const double ROTATION_SPEED_DEG_PER_SEC = 0.8;

double currentTime = 0.0;
double startTime = 0.0;

bool runStarted = false;
bool musicOn = false;
bool backgroundOn = false;
bool zoomOn = true;
bool objectEnabled = false;

double currentForceMag = 0.0;
double wallPenalty = 0.0;

double maxPenalty = 2000.0;

std::string timerText = "Timer: 0.0 s";
std::string penaltyText = "Life: 100%";

cMode state = IDLE;

double counter = 0;

double maxStiffness = 0.0;

cVector3d startPosition(0.00621073, 0.0246095, 0.0438006);
cVector3d endPosition(0.0186877, -0.00899538, 0.046728);

//------------------------------------------------------------------------------
// DECLARED VARIABLES
//------------------------------------------------------------------------------

// a world that contains all objects of the virtual environment
cWorld *world;

// a camera to render the world in the window display
cCamera *camera;

// a light source to illuminate the objects in the world
cDirectionalLight *light;

// a virtual object
cMultiMesh *object;
cMultiMesh *cartoonMesh;

// a haptic device handler
cHapticDeviceHandler *handler;

// a pointer to the current haptic device
cGenericHapticDevicePtr hapticDevice;

// a virtual tool representing the haptic device in the scene
cToolCursor *tool;

cShapeTorus *torusTool;

// a colored background
cBackground *background;

cFontPtr font;
cFontPtr fontTime;
cFontPtr fontCenter;

// audio device to play sound
cAudioDevice *audioDevice;

// audio buffers to store sound files
cAudioBuffer *audioBuffer1;
cAudioBuffer *audioBuffer2;

cAudioSource *toolAudioSource;

cLabel *timeLabel;

cLabel *globalLabel;

cLabel *centerLabel;

// a flag that indicates if the haptic simulation is currently running
bool simulationRunning = false;

// a flag that indicates if the haptic simulation has terminated
bool simulationFinished = true;

// display options
bool showEdges = true;
bool showTriangles = true;
bool showNormals = false;

// display level for collision tree
int collisionTreeDisplayLevel = 0;

// a frequency counter to measure the simulation graphic rate
cFrequencyCounter freqCounterGraphics;

// a frequency counter to measure the simulation haptic rate
cFrequencyCounter freqCounterHaptics;

// haptic thread
cThread *hapticsThread;

// a handle to window display context
GLFWwindow *window = NULL;

// current width of window
int width = 0;

// current height of window
int height = 0;

// swap interval for the display context (vertical synchronization)
int swapInterval = 1;

// root resource path
string resourceRoot;

//------------------------------------------------------------------------------
// DECLARED MACROS
//------------------------------------------------------------------------------

// convert to resource path
#define RESOURCE_PATH(p) (char *)((resourceRoot + string(p)).c_str())

//------------------------------------------------------------------------------
// DECLARED FUNCTIONS
//------------------------------------------------------------------------------

// callback when the window display is resized
void windowSizeCallback(GLFWwindow *a_window, int a_width, int a_height);

// callback when an error GLFW occurs
void errorCallback(int error, const char *a_description);

// callback when a key is pressed
void keyCallback(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods);

// callback to render graphic scene
void updateGraphics(void);

// this function renders the scene
void updateGraphics(void);

// this function contains the main haptics simulation loop
void updateHaptics(void);

// this function updates the text of the labels
void updateUILabels(void);

// this function starts/restarts the game
void startGame(void);

// this function closes the application
void close(void);

//==============================================================================
/*
    DEMO:   32-protein-ring.cpp

    Press "Space" to start the demo. Then, let the haptic device move to the
    starting position. The demo will start.
    If the tool cursor is not trapped inside the mesh, you can try to enter the
    mesh by going into contact (you will get trapped inside the mesh).
    Once inside the mesh, the protein will start to spin around the Z axis slowly,
    the objective is to move the ball along the protein chain to reach the end position.
    The faster you complete the demo, the higher your score will be!
    BUT BEWARE: if you touch the walls of the protein, you will lose "life"!
    This gameplay is not visible by default for the sake of the demo in Dubai.
    By default, the music and background change while pushing against the walls
    are not displayed, you can toggle them ON using the keyboard inputs defined
    below.

*/
//==============================================================================

int main(int argc, char *argv[])
{
    //--------------------------------------------------------------------------
    // INITIALIZATION
    //--------------------------------------------------------------------------

    cout << endl;
    cout << "-----------------------------------" << endl;
    cout << "CHAI3D" << endl;
    cout << "Demo: 32-protein-ring" << endl;
    cout << "Enzo Andreacchio" << endl;
    cout << "-----------------------------------" << endl
         << endl
         << endl;
    cout << "Keyboard Options:" << endl
         << endl;
    cout << "[Space] - Start the demo" << endl;
    cout << "[f] - Toggle fullscreen mode" << endl;
    cout << "[m] - Toggle vertical mirroring" << endl;
    cout << "[i] - Toggle music playback" << endl;
    cout << "[b] - Toggle background color" << endl;
    cout << "[q] or [ESC] - Exit application" << endl;
    cout << endl
         << endl;

    // parse first arg to try and locate resources
    resourceRoot = string(argv[0]).substr(0, string(argv[0]).find_last_of("/\\") + 1);

    //--------------------------------------------------------------------------
    // OPEN GL - WINDOW DISPLAY
    //--------------------------------------------------------------------------

    // initialize GLFW library
    if (!glfwInit())
    {
        cout << "failed initialization" << endl;
        cSleepMs(1000);
        return 1;
    }

    // set error callback
    glfwSetErrorCallback(errorCallback);

    // compute desired size of window
    const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    int w = 0.8 * mode->height;
    int h = 0.5 * mode->height;
    int x = 0.5 * (mode->width - w);
    int y = 0.5 * (mode->height - h);

    // set OpenGL version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // set active stereo mode
    if (stereoMode == C_STEREO_ACTIVE)
    {
        glfwWindowHint(GLFW_STEREO, GL_TRUE);
    }
    else
    {
        glfwWindowHint(GLFW_STEREO, GL_FALSE);
    }

    // create display context
    window = glfwCreateWindow(w, h, "CHAI3D", NULL, NULL);
    if (!window)
    {
        cout << "failed to create window" << endl;
        cSleepMs(1000);
        glfwTerminate();
        return 1;
    }

    // get width and height of window
    glfwGetWindowSize(window, &width, &height);

    // set position of window
    glfwSetWindowPos(window, x, y);

    // set key callback
    glfwSetKeyCallback(window, keyCallback);

    // set resize callback
    glfwSetWindowSizeCallback(window, windowSizeCallback);

    // set current display context
    glfwMakeContextCurrent(window);

    // sets the swap interval for the current display context
    glfwSwapInterval(swapInterval);

#ifdef GLEW_VERSION
    // initialize GLEW library
    if (glewInit() != GLEW_OK)
    {
        cout << "failed to initialize GLEW library" << endl;
        glfwTerminate();
        return 1;
    }
#endif

    //--------------------------------------------------------------------------
    // WORLD - CAMERA - LIGHTING
    //--------------------------------------------------------------------------

    // create a new world.
    world = new cWorld();

    // set the background color of the environment
    world->m_backgroundColor.setBlack();

    // create a camera and insert it into the virtual world
    camera = new cCamera(world);
    world->addChild(camera);

    // define a basis in spherical coordinates for the camera
    camera->setSphericalReferences(cVector3d(0, 0, 0),  // origin
                                   cVector3d(0, 0, 1),  // zenith direction
                                   cVector3d(1, 0, 0)); // azimuth direction

    camera->setSphericalDeg(1.0,                    // spherical coordinate radius
                            65,                     // spherical coordinate polar angle
                            CURRENT_AZIMUTH_ANGLE); // spherical coordinate azimuth angle

    // set the near and far clipping planes of the camera
    // anything in front or behind these clipping planes will not be rendered
    camera->setClippingPlanes(0.01, 100);

    // set stereo mode
    camera->setStereoMode(stereoMode);

    // set stereo eye separation and focal length (applies only if stereo is enabled)
    camera->setStereoEyeSeparation(0.03);
    camera->setStereoFocalLength(1.5);

    // set vertical mirrored display mode
    camera->setMirrorVertical(mirroredDisplay);

    // enable multi-pass rendering to handle transparent objects
    camera->setUseMultipassTransparency(true);

    // create a light source
    light = new cDirectionalLight(world);

    // attach light to camera
    camera->addChild(light);

    // enable light source
    light->setEnabled(true);

    // define the direction of the light beam
    light->setDir(-3.0, -0.5, 0.0);

    // set lighting conditions
    light->m_ambient.set(0.4f, 0.4f, 0.4f);
    light->m_diffuse.set(0.8f, 0.8f, 0.8f);
    light->m_specular.set(1.0f, 1.0f, 1.0f);

    //--------------------------------------------------------------------------
    // HAPTIC DEVICES / TOOLS
    //--------------------------------------------------------------------------

    // create a haptic device handler
    handler = new cHapticDeviceHandler();

    // get access to the first available haptic device found
    handler->getDevice(hapticDevice, 0);

    // retrieve information about the current haptic device
    cHapticDeviceInfo hapticDeviceInfo = hapticDevice->getSpecifications();

    // create a tool (cursor) and insert into the world
    tool = new cToolCursor(world);
    world->addChild(tool);

    // connect the haptic device to the virtual tool
    tool->setHapticDevice(hapticDevice);

    // if the haptic device has a gripper, enable it as a user switch
    hapticDevice->setEnableGripperUserSwitch(true);

    double toolRadiusContact = 0.006; // collision radius
    double toolRadiusDisplay = 0.015; // visual sphere radius

    // set different radii for visuals vs collisions
    tool->m_hapticPoint->setRadius(toolRadiusDisplay, toolRadiusContact);

    // hide the device sphere. only show proxy.
    tool->setShowContactPoints(true, false);

    // create a white cursor
    tool->m_hapticPoint->m_sphereProxy->m_material->setBlack();
    tool->m_hapticPoint->m_sphereProxy->m_material->m_specular.set(0.3f, 0.3f, 0.3f);

    // map the physical workspace of the haptic device to a larger virtual workspace.
    tool->setWorkspaceRadius(1.0);

    // oriente tool with camera
    tool->setLocalRot(camera->getLocalRot());

    // haptic forces are enabled only if small forces are first sent to the device;
    // this mode avoids the force spike that occurs when the application starts when
    // the tool is located inside an object for instance.
    tool->setWaitForSmallForce(false);

    tool->enableDynamicObjects(true);

    // start the haptic tool
    tool->start();

    //--------------------------------------------------------------------------
    // SETUP AUDIO MATERIAL
    //--------------------------------------------------------------------------

    // create an audio device to play sounds
    audioDevice = new cAudioDevice();

    // attach audio device to camera
    camera->attachAudioDevice(audioDevice);

    // create an audio buffer and load audio wave file
    audioBuffer1 = new cAudioBuffer();
    bool fileload1 = audioBuffer1->loadFromFile(RESOURCE_PATH("../resources/sounds/classic.wav"));
    if (!fileload1)
    {
#if defined(_MSVC)
        fileload1 = audioBuffer1->loadFromFile("../../../bin/resources/sounds/metal-scraping.wav");
#endif
    }

    // create an audio buffer and load audio wave file
    audioBuffer2 = new cAudioBuffer();
    bool fileload2 = audioBuffer2->loadFromFile(RESOURCE_PATH("../resources/sounds/metal-impact.wav"));
    if (!fileload2)
    {
#if defined(_MSVC)
        fileload2 = audioBuffer2->loadFromFile("../../../bin/resources/sounds/metal-impact.wav");
#endif
    }

    // check for errors
    if (!(fileload1 && fileload2))
    {
        cout << "Error - Sound file failed to load or initialize correctly." << endl;
        close();
        return (-1);
    }

    //--------------------------------------------------------------------------
    // CREATE TOOL AUDIO SOURCE (FOR FORCE-DEPENDENT SOUND)
    //--------------------------------------------------------------------------

    // Create the audio source AFTER buffers and device are ready
    toolAudioSource = new cAudioSource();
    toolAudioSource->setAudioBuffer(audioBuffer1);
    toolAudioSource->setLoop(true);
    toolAudioSource->setGain(0.5);

    // (optional) also attach a source to the tool for collision sounds
    tool->createAudioSource(audioDevice);

    //--------------------------------------------------------------------------
    // CREATE OBJECT
    //--------------------------------------------------------------------------

    // read the scale factor between the physical workspace of the haptic
    // device and the virtual workspace defined for the tool
    tool->setWorkspaceScaleFactor(7.0);
    double workspaceScaleFactor = tool->getWorkspaceScaleFactor();

    // get properties of haptic device
    maxStiffness = hapticDeviceInfo.m_maxLinearStiffness / workspaceScaleFactor;
    double maxLinearForce = cMin(hapticDeviceInfo.m_maxLinearForce, 7.0);
    double maxDamping = hapticDeviceInfo.m_maxLinearDamping / workspaceScaleFactor;

    // create a virtual mesh
    object = new cMultiMesh();

    // add object to world
    world->addChild(object);

    // load an object file
    bool fileload;
    // fileload = object->loadFromFile(RESOURCE_PATH("../resources/models/feel/protein_long_rev_one_hole.obj"));
    fileload = object->loadFromFile(RESOURCE_PATH("../resources/models/feel/ruban/putty_flipped_normals.obj"));
    if (!fileload)
    {
#if defined(_MSVC)
        fileload = object->loadFromFile("../../../bin/resources/models/feel/protein_long_rev_holes.obj");
#endif
    }
    if (!fileload)
    {
        cout << "Error - 3D Model failed to load correctly" << endl;
        close();
        return (-1);
    }

    // disable culling so that faces are rendered on both sides
    object->setUseCulling(false);

    // get dimensions of object
    object->computeBoundaryBox(true);
    double size = cSub(object->getBoundaryMax(), object->getBoundaryMin()).length();

    // resize object to screen
    if (size > 0.001)
    {
        object->scale(1.0 / size);
    }

    cMaterialPtr material_protein = cMaterial::create();
    cColorf lightred(1.0f, 0.5f, 0.5f);
    material_protein->setColor(lightred); // initial color light red
    // object->setMaterial(material_protein);

    // compute a boundary box
    object->computeBoundaryBox(true);

    // show/hide boundary box
    object->setShowBoundaryBox(false);

    // compute collision detection algorithm
    object->createAABBCollisionDetector(toolRadiusContact);

    // define a default stiffness for the object
    object->setStiffness(0.9 * maxStiffness, true);

    // define some haptic friction properties
    object->setFriction(0.0, 0.0, true);
    // object->createEffectSurface();
    object->m_material->setViscosity(1.0 * maxDamping);
    object->createEffectViscosity();

    // enable display list for faster graphic rendering
    object->setUseDisplayList(true);

    // center object in scene
    object->setLocalPos(-1.0 * object->getBoundaryCenter());

    // rotate object in scene
    object->rotateExtrinsicEulerAnglesDeg(0, 0, 90, C_EULER_ORDER_XYZ);

    // compute all edges of object for which adjacent triangles have more than 40 degree angle
    object->computeAllEdges(40);

    // cMaterialPtr material_protein = cMaterial::create();
    // cColorf blue(0.2f, 0.2f, 0.7f);
    // material_protein->setColor(blue); // initial color blue
    // object->setMaterial(material_protein);

    // object->m_material->setRedDarkSalmon(); // initial color red

    // object->m_material->setBlack(); // initial color black

    // set line width of edges and color
    cColorf colorEdges;
    colorEdges.setBlack();
    object->setEdgeProperties(1, colorEdges);

    // set normal properties for display
    cColorf colorNormals;
    colorNormals.setOrangeTomato();
    object->setNormalsProperties(0.01, colorNormals);

    // display options
    object->setShowTriangles(showTriangles);
    object->setShowEdges(showEdges);
    object->setShowNormals(showNormals);

    cartoonMesh = new cMultiMesh();

    // load an object file
    bool fileload_cartoon;
    // fileload = object->loadFromFile(RESOURCE_PATH("../resources/models/feel/protein_long_rev_one_hole.obj"));
    fileload_cartoon = cartoonMesh->loadFromFile("../resources/models/feel/ruban/cartoon.obj");
    if (!fileload_cartoon)
    {
#if defined(_MSVC)
        fileload_cartoon = object->loadFromFile("../../../bin/resources/models/feel/protein_long_rev_holes.obj");
#endif
    }
    if (!fileload_cartoon)
    {
        cout << "Error - 3D Model failed to load correctly" << endl;
        close();
        return (-1);
    }

    cartoonMesh->computeBoundaryBox(true);
    double size_v = cSub(cartoonMesh->getBoundaryMax(), cartoonMesh->getBoundaryMin()).length();
    if (size_v > 0.001)
        cartoonMesh->scale(1.0 / size_v);

    cartoonMesh->setHapticEnabled(true);
    cartoonMesh->setLocalTransform(object->getLocalTransform());
    cartoonMesh->createAABBCollisionDetector(toolRadiusContact);

    cartoonMesh->setStiffness(0.2 * maxStiffness, true);

    // world->addChild(cartoonMesh);

    // --------------------------------------------------------------------------
    // GENERATE TRAJECTORY
    // --------------------------------------------------------------------------

    // import the cursor trajectory from protein_traj.csv
    // Load trajectory points from protein_traj.csv
    std::vector<cVector3d> trajectoryPoints;
    std::ifstream trajFile(RESOURCE_PATH("../resources/models/feel/protein_traj.csv"));
    if (!trajFile.is_open())
    {
        std::cout << "Error - Trajectory file failed to open" << std::endl;
        close();
        return (-1);
    }
    std::string line;
    while (std::getline(trajFile, line))
    {
        std::istringstream iss(line);
        double x, y, z;
        char comma;
        if (iss >> x >> comma >> y >> comma >> z)
        {
            trajectoryPoints.push_back(cVector3d(x, y, z));
        }
    }
    trajFile.close();

    //--------------------------------------------------------------------------
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    font = NEW_CFONTCALIBRI20();
    fontTime = cFont::create();
    fontTime->loadFromFile(RESOURCE_PATH("../resources/fonts/calibri-48.fnt"));

    fontCenter = cFont::create();
    fontCenter->loadFromFile(RESOURCE_PATH("../resources/fonts/calibri-72.fnt"));

    timeLabel = new cLabel(fontTime);
    timeLabel->m_fontColor.setBlack();
    // camera->m_frontLayer->addChild(timeLabel);
    timeLabel->setLocalPos(10, (int)(0.5 * (height - timeLabel->getHeight())));
    double penaltyPercentage = cClamp(wallPenalty / maxPenalty, 0.0, 1.0);

    centerLabel = new cLabel(fontCenter);
    centerLabel->m_fontColor.setBlack();
    camera->m_frontLayer->addChild(centerLabel);
    centerLabel->setText("Press 'Space' bar");
    centerLabel->setLocalPos((int)(0.5 * (width - centerLabel->getWidth())), (int)(0.7 * height - 0.5 * centerLabel->getHeight()));

    globalLabel = new cLabel(font);
    globalLabel->m_fontColor.setBlack();
    camera->m_frontLayer->addChild(globalLabel);

    // create a background
    background = new cBackground();
    camera->m_backLayer->addChild(background);

    // set background properties
    background->setCornerColors(cColorf(0.95f, 0.95f, 0.95f),
                                cColorf(0.95f, 0.95f, 0.95f),
                                cColorf(0.80f, 0.80f, 0.80f),
                                cColorf(0.80f, 0.80f, 0.80f));

    //--------------------------------------------------------------------------
    // START SIMULATION
    //--------------------------------------------------------------------------

    // create a thread which starts the main haptics rendering loop
    hapticsThread = new cThread();
    hapticsThread->start(updateHaptics, CTHREAD_PRIORITY_HAPTICS);

    // setup callback when application exits
    atexit(close);

    //--------------------------------------------------------------------------
    // MAIN GRAPHIC LOOP
    //--------------------------------------------------------------------------

    // call window size callback at initialization
    windowSizeCallback(window, width, height);

    // main graphic loop
    while (!glfwWindowShouldClose(window))
    {
        // get width and height of window
        glfwGetWindowSize(window, &width, &height);

        // render graphics
        updateGraphics();

        // swap buffers
        glfwSwapBuffers(window);

        // process events
        glfwPollEvents();

        // signal frequency counter
        freqCounterGraphics.signal(1);
    }

    // close window
    glfwDestroyWindow(window);

    // terminate GLFW library
    glfwTerminate();

    // exit
    return 0;
}

//------------------------------------------------------------------------------

void windowSizeCallback(GLFWwindow *a_window, int a_width, int a_height)
{
    // update window size
    width = a_width;
    height = a_height;
}

//------------------------------------------------------------------------------

void errorCallback(int a_error, const char *a_description)
{
    cout << "Error: " << a_description << endl;
}

//------------------------------------------------------------------------------

void keyCallback(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods)
{

    // filter calls that only include a key press
    if ((a_action != GLFW_PRESS) && (a_action != GLFW_REPEAT))
    {
        return;
    }

    // option - exit
    else if ((a_key == GLFW_KEY_ESCAPE) || (a_key == GLFW_KEY_Q))
    {
        glfwSetWindowShouldClose(a_window, GLFW_TRUE);
    }

    // option - show/hide texture
    else if (a_key == GLFW_KEY_1)
    {
        bool useTexture = object->getUseTexture();
        object->setUseTexture(!useTexture);
    }

    // option - enable/disable wire mode
    else if (a_key == GLFW_KEY_2)
    {
        bool useWireMode = object->getWireMode();
        object->setWireMode(!useWireMode, true);
    }

    // option - show/hide collision detection tree
    else if (a_key == GLFW_KEY_3)
    {
        cColorf color = cColorf(1.0, 0.0, 0.0);
        object->setCollisionDetectorProperties(collisionTreeDisplayLevel, color, true);
        bool show = object->getShowCollisionDetector();
        object->setShowCollisionDetector(!show, true);
    }

    // option - decrease depth level of collision tree
    else if (a_key == GLFW_KEY_4)
    {
        collisionTreeDisplayLevel--;
        if (collisionTreeDisplayLevel < 0)
        {
            collisionTreeDisplayLevel = 0;
        }
        cColorf color = cColorf(1.0, 0.0, 0.0);
        object->setCollisionDetectorProperties(collisionTreeDisplayLevel, color, true);
        object->setShowCollisionDetector(true, true);
    }

    // option - increase depth level of collision tree
    else if (a_key == GLFW_KEY_5)
    {
        collisionTreeDisplayLevel++;
        cColorf color = cColorf(1.0, 0.0, 0.0);
        object->setCollisionDetectorProperties(collisionTreeDisplayLevel, color, true);
        object->setShowCollisionDetector(true, true);
    }

    // option - save screenshot to file
    else if (a_key == GLFW_KEY_S)
    {
        cImagePtr image = cImage::create();
        camera->copyImageBuffer(image);
        image->saveToFile("screenshot.png");
        cout << "> Saved screenshot to file.       \r";
    }

    // option - show/hide triangles
    else if (a_key == GLFW_KEY_T)
    {
        showTriangles = !showTriangles;
        object->setShowTriangles(showTriangles);
    }

    // option - show/hide edges
    else if (a_key == GLFW_KEY_E)
    {
        showEdges = !showEdges;
        object->setShowEdges(showEdges);
    }

    // option - show/hide normals
    else if (a_key == GLFW_KEY_N)
    {
        showNormals = !showNormals;
        object->setShowNormals(showNormals);
    }

    // option - toggle fullscreen
    else if (a_key == GLFW_KEY_F)
    {
        // toggle state variable
        fullscreen = !fullscreen;

        // get handle to monitor
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();

        // get information about monitor
        const GLFWvidmode *mode = glfwGetVideoMode(monitor);

        // set fullscreen or window mode
        if (fullscreen)
        {
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
            glfwSwapInterval(swapInterval);
        }
        else
        {
            int w = 0.8 * mode->height;
            int h = 0.5 * mode->height;
            int x = 0.5 * (mode->width - w);
            int y = 0.5 * (mode->height - h);
            glfwSetWindowMonitor(window, NULL, x, y, w, h, mode->refreshRate);
            glfwSwapInterval(swapInterval);
        }

        // Update timeLabel position to respect new height
        glfwGetWindowSize(window, &width, &height);
        timeLabel->setLocalPos(10, (int)(0.5 * (height - timeLabel->getHeight())));
        centerLabel->setLocalPos((int)(0.5 * (width - centerLabel->getWidth())), (int)(0.7 * height - 0.5 * centerLabel->getHeight()));
        globalLabel->setLocalPos((int)(0.5 * (width - globalLabel->getWidth())), 15);
    }

    // option - toggle vertical mirroring
    else if (a_key == GLFW_KEY_M)
    {
        mirroredDisplay = !mirroredDisplay;
        camera->setMirrorVertical(mirroredDisplay);
    }

    // option - toggle the music
    else if (a_key == GLFW_KEY_I)
    {
        if (musicOn)
        {
            toolAudioSource->stop();
            musicOn = false;
        }
        else
        {
            toolAudioSource->play();
            musicOn = true;
        }
    }

    // option - run the demo
    else if (a_key == GLFW_KEY_SPACE)
    {
        state = GOING_TO_START;
        runStarted = false;
        startTime = glfwGetTime();
        world->removeChild(cartoonMesh);
        double penaltyPercentage = cClamp(wallPenalty / maxPenalty, 0.0, 1.0);
        updateUILabels();
        // reset mapping between device and tool workspace
        tool->initialize();
    }

    // option - toggle background
    else if (a_key == GLFW_KEY_B)
    {
        backgroundOn = !backgroundOn;
        if (!backgroundOn)
        {
            cColorf newColor(0.9f, 0.9f, 0.9f);
            background->setCornerColors(newColor, newColor, newColor, newColor);
        }
    }

    // option - toggle zoom
    else if (a_key == GLFW_KEY_Z)
    {
        zoomOn = !zoomOn;
        if (!zoomOn)
        {
            camera->setSphericalDeg(1.0,                    // spherical coordinate radius
                                    65,                     // spherical coordinate polar angle
                                    CURRENT_AZIMUTH_ANGLE); // spherical coordinate azimuth angle
        }
        else
        {
            camera->setSphericalDeg(1.0,                    // spherical coordinate radius
                                    40,                     // spherical coordinate polar angle
                                    CURRENT_AZIMUTH_ANGLE); // spherical coordinate azimuth angle
        }
    }

    // // option - rotate object positively around global Z axis
    // else if (a_key == GLFW_KEY_RIGHT)
    // {
    //     CURRENT_AZIMUTH_ANGLE -= Z_ROTATION_DEG;

    //     camera->setSphericalDeg(1.0,
    //                             65,
    //                             CURRENT_AZIMUTH_ANGLE);

    //     tool->setLocalRot(camera->getLocalRot());
    // }

    // // option - rotate object negatively around global Z axis
    // else if (a_key == GLFW_KEY_LEFT)
    // {
    //     CURRENT_AZIMUTH_ANGLE += Z_ROTATION_DEG;

    //     camera->setSphericalDeg(1.0,
    //                             65,
    //                             CURRENT_AZIMUTH_ANGLE);

    //     tool->setLocalRot(camera->getLocalRot());
    // }
}

//------------------------------------------------------------------------------

void close(void)
{
    // stop the simulation
    simulationRunning = false;

    // wait for graphics and haptics loops to terminate
    while (!simulationFinished)
    {
        cSleepMs(100);
    }

    // close haptic device
    tool->stop();

    // delete resources
    delete hapticsThread;
    delete world;
    delete handler;
}

//------------------------------------------------------------------------------

void updateGraphics(void)
{

    currentTime = glfwGetTime();
    updateUILabels();

    //------------------------------------------------------------------------------
    // RENDER SCENE
    //------------------------------------------------------------------------------

    if (runStarted)
    {

        // Start audio once when runStarted transitions to true
        static bool audioStarted = false;
        if (!audioStarted)
        {
            if (musicOn)
                toolAudioSource->play();
            audioStarted = true;
        }
        // Define thresholds
        double f_min = 0.2;       // background color threshold
        double f_max = 10.0;      // max force
        double f_sound_min = 1.0; // sound starts only after this force
        double fMag = currentForceMag;

        // Compute normalized interpolation for visuals
        double t1 = 0.0;
        if (fMag > f_min)
        {
            t1 = cClamp((fMag - f_min) / (f_max - f_min), 0.0, 0.4);
        }

        // Compute normalized interpolation for sound (starts after f_sound_min)
        double t2 = 0.0;
        if (fMag > f_sound_min)
        {
            t2 = cClamp((fMag - f_sound_min) / (f_max - f_sound_min), 0.0, 1.0);
        }

        // Smooth background color (white → light red)
        cColorf newColor(1.0, 1.0 - t1, 1.0 - t1);
        if (backgroundOn)
        {
            background->setCornerColors(newColor, newColor, newColor, newColor);
        }

        // Update 3D sound position
        toolAudioSource->setSourcePos(tool->getDeviceGlobalPos());

        // Smooth gain to avoid jitter
        static double lastGain = 0.0;
        double alpha = 0.1;
        double smoothedGain = (1.0 - alpha) * lastGain + alpha * t2;
        lastGain = smoothedGain;

        // Apply gain and pitch only if above threshold
        toolAudioSource->setGain(smoothedGain);
        toolAudioSource->setPitch(0.8 + 0.4 * smoothedGain);

        //----------------------------------------------------------------------
        // Smooth camera zoom based on force magnitude
        //----------------------------------------------------------------------
        static double lastZoom = 1.0; // default radius
        double zoomMin = 0.95;        // closer zoom (when strong force)
        double zoomMax = 1.0;         // default zoom (no force)

        // Compute normalized interpolation with same f_max
        double tZoom = cClamp(fMag / f_max, 0.0, 1.0);

        // Map it smoothly (higher force → smaller radius)
        double targetZoom = zoomMax - tZoom * (zoomMax - zoomMin);

        // Exponential smoothing (low alpha = smoother motion)
        double alphaZoom = 0.05;
        double smoothedZoom = (1.0 - alphaZoom) * lastZoom + alphaZoom * targetZoom;
        lastZoom = smoothedZoom;

        // --- derive current spherical coordinates manually ---
        cVector3d camPos = camera->getLocalPos();
        double radius = camPos.length();

        // (We already know polar and azimuth angles from your initialization)
        double polarDeg = 65.0;
        double azimuthDeg = CURRENT_AZIMUTH_ANGLE;

        // Apply new zoom radius
        if (zoomOn)
        {
            camera->setSphericalDeg(smoothedZoom, polarDeg, azimuthDeg);
        }
    }
    else
    {
        // Neutral background and sound before run starts
        background->setCornerColors(cColorf(0.95, 0.95, 0.95),
                                    cColorf(0.95, 0.95, 0.95),
                                    cColorf(0.80, 0.80, 0.80),
                                    cColorf(0.80, 0.80, 0.80));

        toolAudioSource->setGain(0.0);
        toolAudioSource->setPitch(0.8);
        camera->setSphericalDeg(1.0, 65.0, CURRENT_AZIMUTH_ANGLE);
    }

    // update shadow maps (if any)
    world->updateShadowMaps(false, mirroredDisplay);

    // render world
    camera->renderView(width, height);

    // wait until all GL commands are completed
    glFinish();

    // check for any OpenGL errors
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        cout << "Error: " << gluErrorString(err) << endl;
}

//------------------------------------------------------------------------------

void updateHaptics(void)
{
    cGenericObject *selectedObject = NULL;
    cTransform tool_T_object;

    // simulation in now running
    simulationRunning = true;
    simulationFinished = false;

    cVector3d lockedPosition;
    int counter = 0;

    cPrecisionClock rotClock;
    double lastUpdateTime = 0.0;
    static bool rotClockRunning = false;
    rotClock.start();

    // main haptic simulation loop
    while (simulationRunning)
    {
        /////////////////////////////////////////////////////////////////////
        // READ HAPTIC DEVICE
        /////////////////////////////////////////////////////////////////////

        // read current position and velocity
        cVector3d position;
        hapticDevice->getPosition(position);
        // cout << "Device position norm: " << position.length() << endl;

        // static int printCounter = 0;
        // if (printCounter++ % 100 == 0)
        //     cout << "Device position: " << position << endl;

        cVector3d linearVelocity;
        hapticDevice->getLinearVelocity(linearVelocity);

        ///////////////////////////////////////////////////////////////////////
        // HAPTIC RENDERING
        ///////////////////////////////////////////////////////////////////////

        // signal frequency counter
        freqCounterHaptics.signal(1);

        // compute global reference frames for each object
        world->computeGlobalPositions(true);

        // update tool pose
        tool->updateFromDevice();

        object->setEnabled(objectEnabled); // disables all interactions

        tool->computeInteractionForces();

        ///////////////////////////////////////////////////////////////////////
        // GOAL POSITION TRACKING WITH VELOCITY SATURATION
        ///////////////////////////////////////////////////////////////////////

        if (state == GOING_TO_START)
        {

            double kv = 20.0;
            double kp = 300.0;
            double ki = 50.0;

            // cout << "Position: " << position << endl;

            cVector3d pos_error = startPosition - position;
            double position_error = pos_error.length();

            // static int printCounter = 0;
            // if (printCounter++ % 100 == 0)
            //     cout << "Position error: " << position_error << endl;

            static cVector3d intError(0.0, 0.0, 0.0);
            static cPrecisionClock integratorClock;
            static bool clockStarted = false;
            if (!clockStarted)
            {
                integratorClock.start();
                clockStarted = true;
            }

            double dt = integratorClock.stop();
            integratorClock.start();

            // The integrator is only active when close to the goal
            if (position_error < 0.005)
            {
                intError += pos_error * dt;

                // Prevent wind-up
                double intLimit = 0.01;
                if (intError.length() > intLimit)
                    intError *= intLimit / intError.length();
            }
            else
            {
                // reset integrator when far from target
                intError.zero();
            }

            cVector3d vel_desired = (kp * pos_error + ki * intError) / kv;

            double Vmax = 0.15;
            double vel_mag = vel_desired.length();
            if (vel_mag > Vmax)
                vel_desired *= Vmax / vel_mag;

            cVector3d force_cmd = -kv * (linearVelocity - vel_desired);

            cVector3d totalForce = tool->getDeviceGlobalForce();
            totalForce.add(force_cmd);

            tool->setDeviceGlobalForce(totalForce);
            tool->applyToDevice();

            // --------------------------------------------------
            // Transition condition
            // --------------------------------------------------

            if ((position_error < 0.0008) && (linearVelocity.length() < 0.01))
            {
                state = PLAYING;
                centerLabel->setText("");
                lockedPosition = tool->getDeviceGlobalPos();
                objectEnabled = true; // enable collisions
                intError.zero();      // reset integrator on transition
            }
        }

        else if (state == PLAYING)
        {

            if (!rotClockRunning)
            {
                rotClock.reset();
                rotClock.start();
                lastUpdateTime = rotClock.getCurrentTimeSeconds();
                rotClockRunning = true;
            }

            cVector3d end_pos_error = endPosition - position;
            double end_position_error = end_pos_error.length();

            cVector3d start_pos_error = startPosition - position;
            double start_position_error = start_pos_error.length();

            double arrivalThreshold = 0.002;
            double departureThreshold = 0.002;

            if (end_position_error < arrivalThreshold)
            {
                cout << "Reached goal!" << endl;
                state = GAME_OVER;
                runStarted = false;
            }

            if (start_position_error < departureThreshold && !runStarted)
            {
                cout << "Game starting!" << endl;
                runStarted = true;
            }

            // accumulate wall contact penalty
            double dt = 1.0 / freqCounterHaptics.getFrequency(); // approximate time step
            if (currentForceMag > 1.0)
            {
                wallPenalty += currentForceMag * dt;
            }

            // UPDATE THE CAMERA ROTATION
            double currentTime = rotClock.getCurrentTimeSeconds();
            double deltaTime = currentTime - lastUpdateTime;

            if (deltaTime > 0.01) // update every 10 ms
            {
                CURRENT_AZIMUTH_ANGLE += ROTATION_SPEED_DEG_PER_SEC * deltaTime;

                camera->setSphericalDeg(1.0, 65, CURRENT_AZIMUTH_ANGLE);
                tool->setLocalRot(camera->getLocalRot());

                lastUpdateTime = currentTime;
            }
        }

        else if (state == GAME_OVER)
        {
            // stop any residual forces
            tool->setDeviceGlobalForce(cVector3d(0.0, 0.0, 0.0));
            tool->applyToDevice();
        }

        /////////////////////////////////////////////////////////////////////////
        // MANIPULATION
        /////////////////////////////////////////////////////////////////////////

        // compute transformation from world to tool (haptic device)
        cTransform world_T_tool = tool->getDeviceGlobalTransform();

        // update the magnitude of the current force
        currentForceMag = tool->getDeviceLocalForce().length();

        // send forces to haptic device
        tool->applyToDevice();

        counter++;
    }

    // exit haptics thread
    simulationFinished = true;
}

//------------------------------------------------------------------------------

void updateUILabels()
{
    // --- Update globalLabel (top status bar) ---
    globalLabel->setText(
        "Music: " + std::string(musicOn ? "ON" : "OFF") +
        " (I)    Background: " + std::string(backgroundOn ? "ON" : "OFF") +
        " (B)    Zoom: " + std::string(zoomOn ? "ON" : "OFF") +
        " (Z)    Fullscreen: " + std::string(fullscreen ? "ON" : "OFF") +
        " (F)");

    // Recenter the label horizontally
    globalLabel->setLocalPos((int)(0.5 * (width - globalLabel->getWidth())), 15);

    // --- Update timeLabel (timer + penalty) ---
    double penaltyPercentage = cClamp(100.0 - (wallPenalty / maxPenalty * 100.0), 0.0, 100.0);

    if (runStarted)
    {
        double elapsed = currentTime - startTime;
        // Set timeLabel text on two lines, aligned
        timerText = "Timer: " + cStr(elapsed, 1) + " s";
        penaltyText = "Life: " + cStr(penaltyPercentage, 2) + "%";
        timeLabel->setText(timerText + "\n" + penaltyText);

        // Align both lines to the left (x = 10), and vertically center the label
        timeLabel->setLocalPos(10, (int)(0.5 * (height - timeLabel->getHeight())));
    }
    else
    {
        penaltyText = "Life: " + cStr(penaltyPercentage, 2) + "%";
        timeLabel->setText(timerText + "\n" + penaltyText);
        timeLabel->setLocalPos(10, (int)(0.5 * (height - timeLabel->getHeight())));
    }
}

//------------------------------------------------------------------------------

void startGame()
{
    runStarted = true;
}