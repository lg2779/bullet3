#include "../SharedMemory/SharedMemoryInProcessPhysicsC_API.h"
#include "../SharedMemory/PhysicsClientC_API.h"
#include "../SharedMemory/PhysicsDirectC_API.h"


#ifdef __APPLE__
#include <Python/Python.h>
#else
#include <Python.h>
#endif

enum eCONNECT_METHOD
{
	eCONNECT_GUI=1,
	eCONNECT_DIRECT=2,
	eCONNECT_SHARED_MEMORY=3,
};

static PyObject *SpamError;
static  b3PhysicsClientHandle sm=0;


static PyObject *
pybullet_stepSimulation(PyObject *self, PyObject *args)
{
    if (0==sm)
    {
        PyErr_SetString(SpamError, "Not connected to physics server.");
        return NULL;
    }

    {
        b3SharedMemoryStatusHandle statusHandle;
        int statusType;

        if (b3CanSubmitCommand(sm))
        {
            statusHandle = b3SubmitClientCommandAndWaitStatus(sm, b3InitStepSimulationCommand(sm));
            statusType = b3GetStatusType(statusHandle);
        } 
    }

	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
pybullet_connectPhysicsServer(PyObject *self, PyObject *args)
{
    if (0!=sm)
    {
        PyErr_SetString(SpamError, "Already connected to physics server, disconnect first.");
        return NULL;
    }
    
    {
		int method=eCONNECT_GUI;
		if (!PyArg_ParseTuple(args, "i", &method))
		{
			PyErr_SetString(SpamError, "connectPhysicsServer expected argument  eCONNECT_GUI, eCONNECT_DIRECT or eCONNECT_SHARED_MEMORY");
	        return NULL;
		}

		switch (method)
		{
			case eCONNECT_GUI:
			{
				int argc=0;
				char* argv[1]={0};

#ifdef __APPLE__
				sm = b3CreateInProcessPhysicsServerAndConnectMainThread(argc, argv);
#else
				sm = b3CreateInProcessPhysicsServerAndConnect(argc, argv);
#endif
				break;
			}
			case eCONNECT_DIRECT:
			{
				sm = b3ConnectPhysicsDirect();
				break;
			}
			case eCONNECT_SHARED_MEMORY:
			{
				sm = b3ConnectSharedMemory(SHARED_MEMORY_KEY);
				break;
			}


			default:
				{
					PyErr_SetString(SpamError, "connectPhysicsServer unexpected argument");
					return NULL;
				}
		};
        
	
    }
    
	Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pybullet_disconnectPhysicsServer(PyObject *self, PyObject *args)
{
    if (0==sm)
    {
        PyErr_SetString(SpamError, "Not connected to physics server.");
        return NULL;
    }
    {
        b3DisconnectSharedMemory(sm);
        sm = 0;
    }
    
	Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *
pybullet_loadURDF(PyObject* self, PyObject* args)
{
    
	int size= PySequence_Size(args);
	
	int bodyIndex = -1;
	const char* urdfFileName="";
	float startPosX =0;
    float startPosY =0;
    float startPosZ = 1;
	float startOrnX = 0;
	float startOrnY = 0;
	float startOrnZ = 0;
	float startOrnW = 1;
	//printf("size=%d\n", size);
	if (0==sm)
    {
        PyErr_SetString(SpamError, "Not connected to physics server.");
        return NULL;
    }
	if (size==1)
	{
		if (!PyArg_ParseTuple(args, "s", &urdfFileName))
       	 	return NULL;            
	}
	if (size == 4)
	{
	  if (!PyArg_ParseTuple(args, "sfff", &urdfFileName,
		&startPosX,&startPosY,&startPosZ))
                return NULL;
        }
	if (size==8)
	{
		if (!PyArg_ParseTuple(args, "sfffffff", &urdfFileName,
		&startPosX,&startPosY,&startPosZ,
			&startOrnX,&startOrnY,&startOrnZ, &startOrnW))
                return NULL;
		
	}
    {
        
        b3SharedMemoryStatusHandle statusHandle;
        int statusType;
        b3SharedMemoryCommandHandle command = b3LoadUrdfCommandInit(sm, urdfFileName);
		//printf("urdf filename = %s\n", urdfFileName);
        //setting the initial position, orientation and other arguments are optional
        b3LoadUrdfCommandSetStartPosition(command, startPosX,startPosY,startPosZ);
        statusHandle = b3SubmitClientCommandAndWaitStatus(sm, command);
        statusType = b3GetStatusType(statusHandle);
        if (statusType!=CMD_URDF_LOADING_COMPLETED)
		{
			PyErr_SetString(SpamError, "Cannot load URDF file.");
			return NULL;
		}
        bodyIndex = b3GetStatusBodyIndex(statusHandle);
    }
	return PyLong_FromLong(bodyIndex);
}
static PyObject *
pybullet_resetSimulation(PyObject* self, PyObject* args)
{
    if (0==sm)
    {
        PyErr_SetString(SpamError, "Not connected to physics server.");
        return NULL;
    }

    {
        b3SharedMemoryStatusHandle statusHandle;
        statusHandle = b3SubmitClientCommandAndWaitStatus(sm, b3InitResetSimulationCommand(sm));
    }
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
pybullet_setGravity(PyObject* self, PyObject* args)
{
    if (0==sm)
    {
        PyErr_SetString(SpamError, "Not connected to physics server.");
        return NULL;
    }

	{
		float gravX=0;
		float gravY=0;
		float gravZ=-10;
		int ret;

		b3SharedMemoryCommandHandle command = b3InitPhysicsParamCommand(sm);
		b3SharedMemoryStatusHandle statusHandle;

		if (!PyArg_ParseTuple(args, "fff", &gravX,&gravY,&gravZ))
		{
			PyErr_SetString(SpamError, "setGravity expected (x,y,z) values.");
			return NULL;
		}
		ret = b3PhysicsParamSetGravity(command,  gravX,gravY, gravZ);
		//ret = b3PhysicsParamSetTimeStep(command,  timeStep);
		statusHandle = b3SubmitClientCommandAndWaitStatus(sm, command);
		//ASSERT_EQ(b3GetStatusType(statusHandle), CMD_CLIENT_COMMAND_COMPLETED);
	}

	Py_INCREF(Py_None);
	return Py_None;
}


static void pybullet_internalGetBasePositionAndOrientation(int bodyIndex, double basePosition[3],double baseOrientation[3])
{
    basePosition[0] = 0.;
    basePosition[1] = 0.;
    basePosition[2] = 0.;
    
    baseOrientation[0] = 0.;
    baseOrientation[1] = 0.;
    baseOrientation[2] = 0.;
    baseOrientation[3] = 1.;

	{
		
		
		{
	      b3SharedMemoryCommandHandle cmd_handle =
                b3RequestActualStateCommandInit(sm, bodyIndex);
            b3SharedMemoryStatusHandle status_handle =
                    b3SubmitClientCommandAndWaitStatus(sm, cmd_handle);

            const int status_type = b3GetStatusType(status_handle);
            
            const double* actualStateQ;
            b3GetStatusActualState(status_handle, 0/* body_unique_id */,
                               0/* num_degree_of_freedom_q */,
                               0/* num_degree_of_freedom_u */, 0 /*root_local_inertial_frame*/,
                               &actualStateQ , 0 /* actual_state_q_dot */,
                               0 /* joint_reaction_forces */);

            //now, position x,y,z = actualStateQ[0],actualStateQ[1],actualStateQ[2]
            //and orientation x,y,z,w = actualStateQ[3],actualStateQ[4],actualStateQ[5],actualStateQ[6]
            basePosition[0] = actualStateQ[0];
            basePosition[1] = actualStateQ[1];
            basePosition[2] = actualStateQ[2];
            
            baseOrientation[0] = actualStateQ[3];
            baseOrientation[1] = actualStateQ[4];
            baseOrientation[2] = actualStateQ[5];
            baseOrientation[3] = actualStateQ[6];
		}
	}
}

static PyObject *
pybullet_getBasePositionAndOrientation(PyObject* self, PyObject* args)
{
    if (0==sm)
    {
        PyErr_SetString(SpamError, "Not connected to physics server.");
        return NULL;
    }
    
    int bodyIndex = -1;
    if (!PyArg_ParseTuple(args, "i", &bodyIndex ))
    {
        PyErr_SetString(SpamError, "Expected a body index (integer).");
        return NULL;
    }
    
    double basePosition[3];
    double baseOrientation[4];
    
    pybullet_internalGetBasePositionAndOrientation(bodyIndex,basePosition,baseOrientation);
    PyObject *pylistPos; 
    {
    
        PyObject *item; 
        int i;
        int num=3;
        pylistPos = PyTuple_New(num);
        for (i = 0; i < num; i++) 
        {
            item = PyFloat_FromDouble(basePosition[i]);
            PyTuple_SetItem(pylistPos, i, item);
        }
    
    }
    PyObject *pylistOrientation; 
    {
        
        PyObject *item; 
        int i;
        int num=4;
        pylistOrientation = PyTuple_New(num);
        for (i = 0; i < num; i++) 
        {
            item = PyFloat_FromDouble(baseOrientation[i]);
            PyTuple_SetItem(pylistOrientation, i, item);
        }
        
    }
    
    {
        PyObject *pylist; 
        pylist = PyTuple_New(2);
        PyTuple_SetItem(pylist,0,pylistPos);
        PyTuple_SetItem(pylist,1,pylistOrientation);
        return pylist;
    }
    
}


static PyObject *
pybullet_getNumJoints(PyObject* self, PyObject* args)
{
    if (0==sm)
    {
        PyErr_SetString(SpamError, "Not connected to physics server.");
        return NULL;
    }

	{
		int bodyIndex = -1;
		int numJoints=0;
		if (!PyArg_ParseTuple(args, "i", &bodyIndex ))
		{
			PyErr_SetString(SpamError, "Expected a body index (integer).");
			return NULL;
		}
		numJoints = b3GetNumJoints(sm,bodyIndex);

#if PY_MAJOR_VERSION >= 3
		return PyLong_FromLong(numJoints);
#else
		return PyInt_FromLong(numJoints);
#endif
	}
}

static PyObject*
pybullet_setJointPositions(PyObject* self, PyObject* args)
{
	if (0==sm)
	{
		PyErr_SetString(SpamError, "Not connected to physics server.");
		return NULL;
	}
	
	Py_INCREF(Py_None);
        return Py_None;
}

       // const unsigned char* m_rgbColorData;//3*m_pixelWidth*m_pixelHeight bytes
       // const float* m_depthValues;//m_pixelWidth*m_pixelHeight floats

static PyObject* pybullet_renderImage(PyObject* self, PyObject* args)
{
	if (0==sm)
	{
		PyErr_SetString(SpamError, "Not connected to physics server.");
		return NULL;
	}
	
	///request an image from a simulated camera, using a software renderer.
	struct b3CameraImageData imageData;
	PyObject* objViewMat,* objProjMat;
	int width,  height;
	
	if (PyArg_ParseTuple(args, "iiOO", &width, &height, &objViewMat, &objProjMat))
	{
	
		PyObject* seq;
		int i, len;
		PyObject* item;
		float viewMatrix[16];
		float projectionMatrix[16];
		int valid = 1;
		{
    		seq = PySequence_Fast(objViewMat, "expected a sequence");
    		len = PySequence_Size(objViewMat);
		//printf("objViewMat size = %d\n", len);
			if (len==16)
			{
				
				if (PyList_Check(seq))
				{
					for (i = 0; i < len; i++) 
					{
            			item = PyList_GET_ITEM(seq, i);
						viewMatrix[i] = PyFloat_AsDouble(item);
						float v = viewMatrix[i]; 
	 			//printf("view %d to %f\n", i,v);
	
					}
				}
				else
				{
					for (i = 0; i < len; i++)
					{
        			item = PyTuple_GET_ITEM(seq,i);
						viewMatrix[i] = PyFloat_AsDouble(item);
					}
				}
			} else
			{
				valid = 0;
			}
		}
		
		
		{
			seq = PySequence_Fast(objProjMat, "expected a sequence");
			len = PySequence_Size(objProjMat);
	//printf("projMat len = %d\n", len);
			if (len==16)
			{
				if (PyList_Check(seq))
				{
					for (i = 0; i < len; i++)
					{
							item = PyList_GET_ITEM(seq, i);
							projectionMatrix[i] = PyFloat_AsDouble(item);
					}
				}
				else
				{
					for (i = 0; i < len; i++)
					{
							item = PyTuple_GET_ITEM(seq,i);
							projectionMatrix[i] = PyFloat_AsDouble(item);
					}
				}
			} else
			{
					valid = 0;
			}
		}

    		Py_DECREF(seq);	
		{
			b3SharedMemoryCommandHandle command;
			
			command = b3InitRequestCameraImage(sm);
			if (valid)
			{
			//printf("set b3RequestCameraImageSetCameraMatrices\n");
			b3RequestCameraImageSetCameraMatrices(command, viewMatrix, projectionMatrix);
			}

			b3RequestCameraImageSetPixelResolution(command,width,height);
		
			if (b3CanSubmitCommand(sm))
			{
				b3SharedMemoryStatusHandle statusHandle;
				int statusType;
				statusHandle = b3SubmitClientCommandAndWaitStatus(sm, command);
				statusType = b3GetStatusType(statusHandle);
				if (statusType==CMD_CAMERA_IMAGE_COMPLETED)
				{
					PyObject *item2;
					PyObject* pyResultList;//store 4 elements in this result: width, height, rgbData, depth

					b3GetCameraImageData(sm, &imageData);
					//todo: error handling if image size is 0
					pyResultList =  PyTuple_New(4);
					PyTuple_SetItem(pyResultList, 0, PyInt_FromLong(imageData.m_pixelWidth));
					PyTuple_SetItem(pyResultList, 1, PyInt_FromLong(imageData.m_pixelHeight));	
			
					PyObject *pylistPos;
					PyObject* pylistDep;
					int i,j,p;

						//printf("image width = %d, height = %d\n", imageData.m_pixelWidth, imageData.m_pixelHeight);
						{

							PyObject *item;
						int bytesPerPixel = 4;//Red, Green, Blue, each 8 bit values
							int num=bytesPerPixel*imageData.m_pixelWidth*imageData.m_pixelHeight;
							pylistPos = PyTuple_New(num);
						pylistDep = PyTuple_New(imageData.m_pixelWidth*imageData.m_pixelHeight);
			
						for (i=0;i<imageData.m_pixelWidth;i++)
						{
							for (j=0;j<imageData.m_pixelHeight;j++)
							{
								int depIndex = i+j*imageData.m_pixelWidth;
								item = PyFloat_FromDouble(imageData.m_depthValues[depIndex]);
								PyTuple_SetItem(pylistDep, depIndex, item);
								for (p=0;p<bytesPerPixel;p++)
								{
									int pixelIndex = bytesPerPixel*(i+j*imageData.m_pixelWidth)+p;
									item = PyInt_FromLong(imageData.m_rgbColorData[pixelIndex]);
											PyTuple_SetItem(pylistPos, pixelIndex, item);
								}
							}
						}	
					}

					PyTuple_SetItem(pyResultList, 2,pylistPos);
					PyTuple_SetItem(pyResultList, 3,pylistDep);
					return pyResultList;	
				}	
			}
			
		}

	}

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef SpamMethods[] = {
    {"loadURDF",  pybullet_loadURDF, METH_VARARGS,
        "Create a multibody by loading a URDF file."},
    
    {"connect",  pybullet_connectPhysicsServer, METH_VARARGS,
        "Connect to an existing physics server (using shared memory by default)."},
	
    {"disconnect",  pybullet_disconnectPhysicsServer, METH_VARARGS,
        "Disconnect from the physics server."},

    {"resetSimulation",  pybullet_resetSimulation, METH_VARARGS,
        "Reset the simulation: remove all objects and start from an empty world."},
	
	{"stepSimulation",  pybullet_stepSimulation, METH_VARARGS,
        "Step the simulation using forward dynamics."},

	{"setGravity",  pybullet_setGravity, METH_VARARGS,
        "Set the gravity acceleration (x,y,z)."},

	{"initializeJointPositions", pybullet_setJointPositions, METH_VARARGS,
	"Initialize the joint positions for all joints. This method skips any physics simulation and teleports all joints to the new positions."},
	
	{"renderImage", pybullet_renderImage, METH_VARARGS,
	"Render an image (given the pixel resolution width & height and camera view & projection matrices), and return the 8-8-8bit RGB pixel data and floating point depth values"},	
	
	{"getBasePositionAndOrientation",  pybullet_getBasePositionAndOrientation, METH_VARARGS,
        "Get the world position and orientation of the base of the object. (x,y,z) position vector and (x,y,z,w) quaternion orientation."},
	
	{"getNumsetGravity",  pybullet_setGravity, METH_VARARGS,
        "Set the gravity acceleration (x,y,z)."},
	{
		"getNumJoints", pybullet_getNumJoints, METH_VARARGS,
        "Get the number of joints for an object."},
	
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

#if PY_MAJOR_VERSION >= 3
    static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "pybullet",     /* m_name */
        "Python bindings for Bullet",  /* m_doc */
        -1,                  /* m_size */
        SpamMethods,    /* m_methods */
        NULL,                /* m_reload */
        NULL,                /* m_traverse */
        NULL,                /* m_clear */
        NULL,                /* m_free */
    };
#endif

PyMODINIT_FUNC
#if PY_MAJOR_VERSION >= 3
PyInit_pybullet(void)
#else
initpybullet(void)
#endif
{

    PyObject *m;
#if PY_MAJOR_VERSION >= 3
    m = PyModule_Create(&moduledef);
#else
    m = Py_InitModule3("pybullet",
        SpamMethods, "Python bindings for Bullet");
#endif

#if PY_MAJOR_VERSION >= 3
    if (m == NULL)
        return m;
#else
    if (m == NULL)
        return;
#endif
    
	
	PyModule_AddIntConstant (m, "SHARED_MEMORY", eCONNECT_SHARED_MEMORY); // user read
	PyModule_AddIntConstant (m, "DIRECT", eCONNECT_DIRECT); // user read 
	PyModule_AddIntConstant (m, "GUI", eCONNECT_GUI); // user read 

    SpamError = PyErr_NewException("pybullet.error", NULL, NULL);
    Py_INCREF(SpamError);
    PyModule_AddObject(m, "error", SpamError);
#if PY_MAJOR_VERSION >= 3
	return m;
#endif
}

