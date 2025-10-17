//------------------------------------------------------------------------------
#include "chai3d.h"
//------------------------------------------------------------------------------
#include <GLFW/glfw3.h>
//------------------------------------------------------------------------------
using namespace chai3d;
using namespace std;
//------------------------------------------------------------------------------
#include "CODE.h"
//------------------------------------------------------------------------------

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

//---------------------------------------------------------------------------
// CHAI3D VARIABLES
//---------------------------------------------------------------------------

// a world that contains all objects of the virtual environment
cWorld *world;

// a camera to render the world in the window display
cCamera *camera;

// a light source to illuminate the objects in the world
cSpotLight *light;

// a haptic device handler
cHapticDeviceHandler *handler;

// a pointer to the current haptic device
shared_ptr<cGenericHapticDevice> hapticDevice;

// a virtual vaccine representing the haptic device in the scene
cToolCursor *vaccine;

// a label to display the rate [Hz] at which the simulation is running
cLabel *labelRates;

// stiffness of virtual spring
double linGain = 0.2;
double angGain = 0.03;
double linG;
double angG;
double linStiffness = 800;
double angStiffness = 30;

double currentForceMag = 0.0;

//---------------------------------------------------------------------------
// ODE MODULE VARIABLES
//---------------------------------------------------------------------------

// ODE world
cODEWorld *ODEWorld;

// ODE objects
cODEGenericBody *ODESpike;
cODEGenericBody *ODEVaccine;

//---------------------------------------------------------------------------
// AUDIO VARIABLES
//---------------------------------------------------------------------------
cAudioDevice *audioDevice;
cAudioBuffer *audioBufferImpact;
cAudioSource *audioSourceSpike;
bool spikeContactActive = false;

//---------------------------------------------------------------------------
// GENERAL VARIABLES
//---------------------------------------------------------------------------

// flag to indicate if the haptic simulation currently running
bool simulationRunning = false;

// flag to indicate if the haptic simulation has terminated
bool simulationFinished = true;

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

//---------------------------------------------------------------------------
// DECLARED MACROS
//---------------------------------------------------------------------------
// convert to resource path
#define RESOURCE_PATH(p) (char *)((resourceRoot + string(p)).c_str())

//---------------------------------------------------------------------------
// DECLARED FUNCTIONS
//---------------------------------------------------------------------------

// callback when the window display is resized
void windowSizeCallback(GLFWwindow *a_window, int a_width, int a_height);

// callback when an error GLFW occurs
void errorCallback(int error, const char *a_description);

// callback when a key is pressed
void keyCallback(GLFWwindow *a_window, int a_key, int a_scancode, int a_action, int a_mods);

// this function renders the scene
void updateGraphics(void);

// this function contains the main haptics simulation loop
void updateHaptics(void);

// this function closes the application
void close(void);

//===========================================================================
/*
    DEMO:    05-ODE-dental.cpp

    This example illustrates the use of the ODE framework for simulating
    haptic interaction with dynamic bodies. In this scene we demonstrate a
    dental application where a drill is interacting with a set of spike.
 */
//===========================================================================

int main(int argc, char *argv[])
{
    //-----------------------------------------------------------------------
    // INITIALIZATION
    //-----------------------------------------------------------------------

    cout << endl;
    cout << "-----------------------------------" << endl;
    cout << "CHAI3D" << endl;
    cout << "Demo: Covid demo" << endl;
    cout << "Enzo Andreacchio" << endl;
    cout << "-----------------------------------" << endl
         << endl
         << endl;
    cout << "Keyboard Options:" << endl
         << endl;
    cout << "[h] - Display help menu" << endl;
    cout << "[1] - Enable gravity" << endl;
    cout << "[2] - Disable gravity" << endl
         << endl;
    cout << "[3] - decrease linear haptic gain" << endl;
    cout << "[4] - increase linear haptic gain" << endl;
    cout << "[5] - decrease angular haptic gain" << endl;
    cout << "[6] - increase angular haptic gain" << endl
         << endl;
    cout << "[7] - decrease linear stiffness" << endl;
    cout << "[8] - increase linear stiffness" << endl;
    cout << "[9] - decrease angular stiffness" << endl;
    cout << "[0] - increase angular stiffness" << endl
         << endl;
    cout << "[q] - Exit application\n"
         << endl;
    cout << endl
         << endl;

    // parse first arg to try and locate resources
    resourceRoot = string(argv[0]).substr(0, string(argv[0]).find_last_of("/\\") + 1);

    //-----------------------------------------------------------------------
    // OPEN GL - WINDOW DISPLAY
    //-----------------------------------------------------------------------

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

    // initialize GLEW library
#ifdef GLEW_VERSION
    if (glewInit() != GLEW_OK)
    {
        cout << "failed to initialize GLEW library" << endl;
        glfwTerminate();
        return 1;
    }
#endif

    //-----------------------------------------------------------------------
    // WORLD - CAMERA - LIGHTING
    //-----------------------------------------------------------------------

    // create a new world.
    world = new cWorld();

    // set the background color of the environment
    // the color is defined by its (R,G,B) components.
    world->setBackgroundColor(1.0, 1.0, 1.0);

    // create a camera and insert it into the virtual world
    camera = new cCamera(world);
    world->addChild(camera);

    // position and oriente the camera
    camera->set(cVector3d(2.0, 0.0, 0.2),  // camera position (eye)
                cVector3d(0.0, 0.0, -0.1), // lookat position (target)
                cVector3d(0.0, 0.0, 1.0)); // direction of the "up" vector

    // set the near and far clipping planes of the camera
    // anything in front/behind these clipping planes will not be rendered
    camera->setClippingPlanes(0.01, 10.0);

    // set stereo mode
    camera->setStereoMode(stereoMode);

    // set stereo eye separation and focal length (applies only if stereo is enabled)
    camera->setStereoEyeSeparation(0.02);
    camera->setStereoFocalLength(2.0);

    // set vertical mirrored display mode
    camera->setMirrorVertical(mirroredDisplay);

    // create a light source
    light = new cSpotLight(world);

    // attach light to camera
    world->addChild(light);

    // enable light source
    light->setEnabled(true);

    // position the light source
    light->setLocalPos(2.0, 0.0, 1.0);

    // define the direction of the light beam
    light->setDir(-2.0, 0.0, -1.0);

    // set uniform concentration level of light
    light->setSpotExponent(1.0);

    // enable this light source to generate shadows
    light->setShadowMapEnabled(false);

    // set the resolution of the shadow map
    // light->m_shadowMap->setQualityLow();
    light->m_shadowMap->setQualityMedium();

    // set light cone half angle
    light->setCutOffAngleDeg(30);

    //-----------------------------------------------------------------------
    // HAPTIC DEVICES / TOOLS
    //-----------------------------------------------------------------------

    // create a haptic device handler
    handler = new cHapticDeviceHandler();

    // get access to the first available haptic device
    handler->getDevice(hapticDevice, 0);

    // retrieve information about the current haptic device
    cHapticDeviceInfo hapticDeviceInfo = hapticDevice->getSpecifications();

    // create a 3D vaccine and add it to the world
    vaccine = new cToolCursor(world);
    world->addChild(vaccine);

    // connect the haptic device to the vaccine
    vaccine->setHapticDevice(hapticDevice);

    vaccine->setWaitForSmallForce(false);

    // initialize vaccine by connecting to haptic device
    vaccine->start();

    // map the physical workspace of the haptic device to a larger virtual workspace.
    vaccine->setWorkspaceRadius(1.3);

    // define a radius for the vaccine
    vaccine->setRadius(0.0);

    // hide the device sphere. only show proxy.
    vaccine->setShowContactPoints(false, false);

    // haptic forces are enabled only if small forces are first sent to the device;
    // this mode avoids the force spike that occurs when the application starts when
    // the vaccine is located inside an object for instance.
    vaccine->setWaitForSmallForce(true);

    // start the haptic vaccine
    vaccine->start();

    //--------------------------------------------------------------------------
    // WIDGETS
    //--------------------------------------------------------------------------

    // create a font
    cFontPtr font = NEW_CFONTCALIBRI20();

    // create a label to display the haptic and graphic rate of the simulation
    labelRates = new cLabel(font);
    labelRates->m_fontColor.setBlack();
    camera->m_frontLayer->addChild(labelRates);

    // create a background
    cBackground *background = new cBackground();
    // camera->m_backLayer->addChild(background);

    // load a texture file
    bool fileloadBackground = background->loadFromFile(RESOURCE_PATH("../resources/images/covid_bg.jpg"));
    if (!fileloadBackground)
    {
#if defined(_MSVC)
        fileloadBackground = background->loadFromFile("../../../bin/resources/images/earth.jpg");
#endif
    }
    if (!fileloadBackground)
    {
        cout << "Error - Image failed to load correctly." << endl;
        close();
        return (-1);
    }

    // set background properties
    background->setCornerColors(cColorf(1.0f, 1.0f, 1.0f),
                                cColorf(1.0f, 1.0f, 1.0f),
                                cColorf(0.8f, 0.8f, 0.8f),
                                cColorf(0.8f, 0.8f, 0.8f));

    //-----------------------------------------------------------------------
    // CREATE ODE WORLD AND OBJECTS
    //-----------------------------------------------------------------------

    // read the scale factor between the physical workspace of the haptic
    // device and the virtual workspace defined for the vaccine
    double workspaceScaleFactor = vaccine->getWorkspaceScaleFactor();

    // stiffness properties
    double maxStiffness = hapticDeviceInfo.m_maxLinearStiffness / workspaceScaleFactor;

    // clamp the force output gain to the max device stiffness
    linGain = cMin(linGain, maxStiffness / linStiffness);

    // create an ODE world to simulate dynamic bodies
    ODEWorld = new cODEWorld(world);

    // add ODE world as a node inside world
    world->addChild(ODEWorld);

    // set some gravity
    ODEWorld->setGravity(cVector3d(0.0, 0.0, 0.0));

    // create a new ODE object that is automatically added to the ODE world
    ODESpike = new cODEGenericBody(ODEWorld);

    // create a virtual mesh  that will be used for the geometry
    // representation of the dynamic body
    cMultiMesh *imgSpike = new cMultiMesh();

    // load model
    bool fileload;
    // fileload = imgSpike->loadFromFile(RESOURCE_PATH("../resources/models/dental/spike-bottom.3ds"));
    fileload = imgSpike->loadFromFile(RESOURCE_PATH("../resources/models/feel/spike/spike_new.obj"));
    if (!fileload)
    {
        cout << "ERRROR - 3D Model failed to load correctly." << endl;
#if defined(_MSVC)
        fileload = imgSpike->loadFromFile("../../../bin/resources/models/dental/spike-bottom.3ds");
#endif
    }

    // scale object
    // imgSpike->scale(0.004);
    // imgSpike->scale(0.01);
    imgSpike->scale(0.05);

    // create collision detetctor
    imgSpike->createAABBCollisionDetector(0.0);

    // assign haptic properties
    cMaterial matSpike;
    matSpike.setStiffness(0.3 * maxStiffness);
    matSpike.setHapticTriangleSides(true, false);
    matSpike.setShininess(1.0);
    // imgSpike->setMaterial(matSpike, true);

    // light->setShadowMapEnabled(false);

    // add mesh to ODE object
    ODESpike->setImageModel(imgSpike);

    // create a dynamic model of the ODE object. Here we decide to use a box just like
    // the object mesh we just defined
    ODESpike->createDynamicMesh(true);

    // position and orient model
    ODESpike->setLocalPos(0.0, 0.0, -0.3);
    // ODESpike->rotateAboutGlobalAxisDeg(cVector3d(0, 0, 1), 90);
    // ODESpike->rotateAboutGlobalAxisDeg(cVector3d(0, 1, 0), 20);

    // 1. Create a quaternion from your axis-angle
    cQuaternion quat;
    cVector3d axis(-0.669, -0.569, 0.478);
    double angleDeg = 68.4526;
    quat.fromAxisAngle(axis, cDegToRad(angleDeg)); // axis-angle -> quaternion

    // 2. Convert quaternion to rotation matrix
    cMatrix3d rot;
    quat.toRotMat(rot);

    // 3. Apply rotation matrix to spike
    ODESpike->setLocalRot(rot);

    // dBodySetAngularDamping(ODESpike->m_ode_body, 0.3);
    // dBodySetLinearDamping(ODESpike->m_ode_body, 0.1);

    // create a virtual vaccine
    ODEVaccine = new cODEGenericBody(ODEWorld);
    cMultiMesh *imgVaccine = new cMultiMesh();

    // fileload = imgVaccine->loadFromFile(RESOURCE_PATH("../resources/models/dental/drill.obj"));
    fileload = imgVaccine->loadFromFile(RESOURCE_PATH("../resources/models/feel/vaccine/vaccine_new.obj"));
    if (!fileload)
    {
#if defined(_MSVC)
        fileload = imgVaccine->loadFromFile("../../../bin/resources/models/dental/drill.obj");
#endif
    }
    // imgVaccine->scale(0.01);
    imgVaccine->scale(0.05);

    // define some material properties for each cube
    cMaterial matVaccine;
    matVaccine.setGrayLevel(0.5);
    // imgVaccine->setMaterial(matVaccine, true);
    imgVaccine->setHapticEnabled(false);

    // add mesh to ODE object
    ODEVaccine->setImageModel(imgVaccine);
    ODEVaccine->createDynamicMesh(false);

    // define some mass properties for each cube
    ODEVaccine->setMass(0.01);
    dBodySetAngularDamping(ODEVaccine->m_ode_body, 0.06);
    dBodySetLinearDamping(ODEVaccine->m_ode_body, 0.06);

    //-----------------------------------------------------------------------
    // START SIMULATION
    //-----------------------------------------------------------------------

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

//---------------------------------------------------------------------------

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

//---------------------------------------------------------------------------

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

    // help menu
    else if (a_key == GLFW_KEY_H)
    {
        cout << "Keyboard Options:" << endl
             << endl;
        cout << "[h] - Display help menu" << endl;
        cout << "[1] - Enable gravity" << endl;
        cout << "[2] - Disable gravity" << endl
             << endl;
        cout << "[3] - decrease linear haptic gain" << endl;
        cout << "[4] - increase linear haptic gain" << endl;
        cout << "[5] - decrease angular haptic gain" << endl;
        cout << "[6] - increase angular haptic gain" << endl
             << endl;
        cout << "[7] - decrease linear stiffness" << endl;
        cout << "[8] - increase linear stiffness" << endl;
        cout << "[9] - decrease angular stiffness" << endl;
        cout << "[0] - increase angular stiffness" << endl
             << endl;
        cout << "[q] - Exit application\n"
             << endl;
        cout << endl
             << endl;
    }

    // option - enable gravity:
    else if (a_key == GLFW_KEY_1)
    {
        // enable gravity
        ODEWorld->setGravity(cVector3d(0.0, 0.0, -9.81));
        cout << "gravity ON:" << endl;
    }

    // option - disable gravity:
    else if (a_key == GLFW_KEY_2)
    {
        // disable gravity
        ODEWorld->setGravity(cVector3d(0.0, 0.0, 0.0));
        cout << "gravity OFF:" << endl;
    }

    // option - decrease linear haptic gain
    else if (a_key == GLFW_KEY_3)
    {
        linGain = linGain - 0.05;
        if (linGain < 0)
            linGain = 0;
        printf("linear haptic gain:  %f\n", linGain);
    }

    // option - increase linear haptic gain
    else if (a_key == GLFW_KEY_4)
    {
        linGain = linGain + 0.05;
        printf("linear haptic gain:  %f\n", linGain);
    }

    // option - decrease angular haptic gain
    else if (a_key == GLFW_KEY_5)
    {
        angGain = angGain - 0.005;
        if (angGain < 0)
            angGain = 0;
        printf("angular haptic gain:  %f\n", angGain);
    }

    // option - increase angular haptic gain
    else if (a_key == GLFW_KEY_6)
    {
        angGain = angGain + 0.005;
        printf("angular haptic gain:  %f\n", angGain);
    }

    // option - decrease linear stiffness
    else if (a_key == GLFW_KEY_7)
    {
        linStiffness = linStiffness - 50;
        if (linStiffness < 0)
            linStiffness = 0;
        printf("linear stiffness:  %f\n", linStiffness);
    }

    // option - increase linear stiffness
    else if (a_key == GLFW_KEY_8)
    {
        linStiffness = linStiffness + 50;
        printf("linear stiffness:  %f\n", linStiffness);
    }

    // option - decrease angular stiffness
    else if (a_key == GLFW_KEY_9)
    {
        angStiffness = angStiffness - 1;
        if (angStiffness < 0)
            angStiffness = 0;
        printf("angular stiffness:  %f\n", angStiffness);
    }

    // option - increase angular stiffness
    else if (a_key == GLFW_KEY_0)
    {
        angStiffness = angStiffness + 1;
        printf("angular stiffness:  %f\n", angStiffness);
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
    }

    // option - toggle vertical mirroring
    else if (a_key == GLFW_KEY_M)
    {
        mirroredDisplay = !mirroredDisplay;
        camera->setMirrorVertical(mirroredDisplay);
    }
}

//---------------------------------------------------------------------------

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
    hapticDevice->close();

    // delete resources
    delete hapticsThread;
    delete world;
    delete handler;
}

//---------------------------------------------------------------------------

void updateGraphics(void)
{
    /////////////////////////////////////////////////////////////////////
    // UPDATE WIDGETS
    /////////////////////////////////////////////////////////////////////

    // update haptic and graphic rate data
    labelRates->setText(cStr(freqCounterGraphics.getFrequency(), 0) + " Hz / " +
                        cStr(freqCounterHaptics.getFrequency(), 0) + " Hz");

    // update position of label
    labelRates->setLocalPos((int)(0.5 * (width - labelRates->getWidth())), 15);

    /////////////////////////////////////////////////////////////////////
    // RENDER SCENE
    /////////////////////////////////////////////////////////////////////

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

//---------------------------------------------------------------------------

void updateHaptics(void)
{
    // simulation in now running
    simulationRunning = true;

    // simulation clock
    cPrecisionClock simClock;
    simClock.start(true);

    cMatrix3d prevRotVaccine;
    prevRotVaccine.identity();

    // main haptic simulation loop
    while (simulationRunning)
    {
        // update frequency counter
        freqCounterHaptics.signal(1);

        // retrieve simulation time and compute next interval
        double time = simClock.getCurrentTimeSeconds();
        double nextSimInterval = 0.0001; // cClamp(time, 0.00001, 0.0002);

        // reset clock
        simClock.reset();
        simClock.start();

        // compute global reference frames for each object
        world->computeGlobalPositions(true);

        // update position and orientation of vaccine
        vaccine->updateFromDevice();

        // compute interaction forces
        vaccine->computeInteractionForces();

        // update position and orientation of vaccine
        cVector3d posDevice;
        cMatrix3d rotDevice;
        // hapticDevice->getPosition(posDevice);
        // hapticDevice->getRotation(rotDevice);
        posDevice = vaccine->m_hapticPoint->getGlobalPosProxy();
        rotDevice = vaccine->getDeviceGlobalRot();

        // read position of vaccine
        cVector3d posVaccine = ODEVaccine->getLocalPos();
        cMatrix3d rotVaccine = ODEVaccine->getLocalRot();

        // compute position and angular error between vaccine and haptic device
        cVector3d deltaPos = (posDevice - posVaccine);
        cMatrix3d deltaRot = cMul(cTranspose(rotVaccine), rotDevice);
        double angle;
        cVector3d axis;
        deltaRot.toAxisAngle(axis, angle);

        // compute force and torque to apply to vaccine
        cVector3d force, torque;
        force = linStiffness * deltaPos;
        ODEVaccine->addExternalForce(force);

        torque = cMul((angStiffness * angle), axis);
        rotVaccine.mul(torque);
        ODEVaccine->addExternalTorque(torque);

        // compute force and torque to apply to haptic device
        force = -linG * force;
        torque = -angG * torque;

        // add force contribution from ODE model
        vaccine->addDeviceGlobalForce(force);
        vaccine->addDeviceGlobalTorque(torque);

        // send forces to haptic device.
        vaccine->applyToDevice();

        if (linG < linGain)
        {
            linG = linG + 0.1 * time * linGain;
        }
        else
        {
            linG = linGain;
        }

        if (angG < angGain)
        {
            angG = angG + 0.1 * time * angGain;
        }
        else
        {
            angG = angGain;
        }

        //-----------------------------------------------------------------------
        // SPRING FORCE TO KEEP SPIKE NEAR ORIGIN
        //-----------------------------------------------------------------------

        // get position
        cVector3d posSpike = ODESpike->getLocalPos();

        // print spike position and orientation
        // cMatrix3d rotSpike = ODESpike->getLocalRot();
        // cVector3d axis2;
        // double angle2;
        // rotSpike.toAxisAngle(axis2, angle2);
        // cout << "Spike Position: " << posSpike.str(3)
        //      << " | Axis: " << axis2.str(3)
        //      << " | Angle (deg): " << cRadToDeg(angle2) << endl;

        // get linear velocity from ODE
        const dReal *v = dBodyGetLinearVel(ODESpike->m_ode_body);
        cVector3d velSpike(v[0], v[1], v[2]);

        // spring parameters
        const double K_spring = 200.0; // stiffness
        const double D_damping = 30.0; // damping

        // compute spring force
        cVector3d springForce = -K_spring * posSpike - D_damping * velSpike;

        currentForceMag = vaccine->getDeviceLocalForce().length();

        // apply to ODE body
        // ODESpike->addExternalForce(springForce);

        // update simulation
        ODEWorld->updateDynamics(nextSimInterval);
    }

    // exit haptics thread
    simulationFinished = true;
}
