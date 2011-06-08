/****************************************************************************\
 *  CameraAxis version 0.1a                                                 *
 *  A Camera Plugin Driver for the Player/Stage robot server                *
 *                                                                          *
 *  Copyright (C) 2010 Zhanwu Xiong                                         *
 *  zhanwu at cvc dot uab dot es     http://cvc.uab.es/~zhanwu              *
 *                                                                          *
 *  A Player/Stage plugin driver for Axis 214 camera's camera device        *
 *                                                                          *
 *                                                                          *
 *  This program is free software; you can redistribute it and/or modify    *
 *  it under the terms of the GNU General Public License as published by    *
 *  the Free Software Foundation; either version 2 of the License, or       *
 *  (at your option) any later version.                                     *
 *                                                                          *
 *  This program is distributed in the hope that it will be useful,         *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *  GNU General Public License for more details.                            *
 *                                                                          *
 *  You should have received a copy of the GNU General Public License       *
 *  along with this program; if not, write to the Free Software             *
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston,                   *
 *  MA  02111-1307  USA                                                     *
 *                                                                          *
 *                                                                          *
 *  Portions based on the Player/Stage Sample Plugin Driver                 *
 *                                                                          *
 *  Many thanks to Andrew D. Bagdanov for help                              *
 *                                                                          *
\****************************************************************************/

#include <stdint.h>
#include <queue>

#include <curl/curl.h>

#include <libplayercore/playercore.h>

#define MAX_Q_SIZE         5

typedef struct _queuedframe {
  uint32_t image_count;    // Size of the image data
  uint8_t* image;          // Image data
} queuedframe;
typedef queue<queuedframe> framesqueue;

/////////////////////////////////////////////////////////////
// Class of the camera device
class CameraAxisDevice
{
public:
  CameraAxisDevice(char* url);
  ~CameraAxisDevice();

  // Check any frames in Q
  bool            framesWaiting();
  // Read a frame from the framesqueue (Q)
  queuedframe     dequeue();

private:
  char*           camera_url;      // Url of the camera
  CURL*           camera_CURL;     // CURL connection with the camera
  pthread_t       camera_thread;   // Internal thread for the camera
  static pthread_mutex_t mutex;           // Mutex to protect shared framesqueue(Q)

  framesqueue     Q;               // Queue of frames, for both reading and writing
  int             Q_size;          // Size of the queue
  int             max_Q_size;      // Capacity of the queue

  // Old static local members of grab_frame
  static uint8_t*    buffer;              // The bytes grabed by CURL, to build the jpeg
  static size_t      buffersize;          // Size of the buffer (allocated memory)
  static size_t      bufferposition;      // The write position in the buffer
  static uint8_t     state;               // The state of the fsm

  // To start the camera thread: Establish the CURL connection
  //                             Setup the responsing function
  static void*    start_camera_thread(void *ptr);

  // The responsing function for CURL
  static size_t   grab_frame(void *ptr, size_t size, size_t nmemb, void *data);

  // Write a new frame into the framesqueue(Q)
  void            enqueue(queuedframe f);
};

CameraAxisDevice::CameraAxisDevice(char* url)
{
  camera_url = url;
  max_Q_size = MAX_Q_SIZE;
  Q_size     = 0;
  buffer = NULL;
  state  = 0;
  pthread_create(&camera_thread, NULL, this->start_camera_thread, (void*)this);
  puts("CameraAxisDevice: Camera thread started.");
}

CameraAxisDevice::~CameraAxisDevice()
{
  // Clean up the CURL connection
  curl_easy_cleanup(camera_CURL);
  puts("CameraAxisDevice: CURL connection cleaned up.");
  // Stop the camera thread
  pthread_cancel(camera_thread);
  puts("CameraAxisDevice: Camera thread exit.");

}

bool CameraAxisDevice::framesWaiting()
{
  return !Q.empty();
}

queuedframe CameraAxisDevice::dequeue()
{
//  fprintf(stderr, "Getting lock (dequeue)\n");
  pthread_mutex_lock(&mutex);
//  fprintf(stderr, "Got lock (dequeue)\n");
  queuedframe f = Q.front();   
  Q.pop();
  Q_size--; 
  pthread_mutex_unlock(&mutex);
  return f;
}

void* CameraAxisDevice::start_camera_thread(void* ptr)
{
  CameraAxisDevice* cad = (CameraAxisDevice*)ptr;

  puts("CameraAxisDevice: Starting CURL connection...");

  cad->camera_CURL = curl_easy_init();
  if(cad->camera_CURL) {
    curl_easy_setopt(cad->camera_CURL, CURLOPT_URL, cad->camera_url);
    curl_easy_setopt(cad->camera_CURL, CURLOPT_WRITEFUNCTION, &CameraAxisDevice::grab_frame);
    curl_easy_setopt(cad->camera_CURL, CURLOPT_WRITEDATA, cad);
    curl_easy_perform(cad->camera_CURL);
  }
  else
    puts(">CameraAxisDevice: CURL connection failed");
  return (NULL);
}


uint8_t*    CameraAxisDevice::buffer;              // The bytes grabed by CURL, to build the jpeg
size_t      CameraAxisDevice::buffersize;          // Size of the buffer (allocated memory)
size_t      CameraAxisDevice::bufferposition;      // The write position in the buffer
uint8_t     CameraAxisDevice::state;               // The state of the fsm
pthread_mutex_t CameraAxisDevice::mutex;           // Mutex to protect shared framesqueue(Q)

size_t CameraAxisDevice::grab_frame(void *ptr, size_t size, size_t nmemb, void *data) 
{
  CameraAxisDevice* me = (CameraAxisDevice*) data;
  
  size_t realsize = size * nmemb;

//  static uint8_t*    buffer;              // The bytes grabed by CURL, to build the jpeg
//  static size_t      buffersize;          // Size of the buffer (allocated memory)
//  static size_t      bufferposition;      // The write position in the buffer
//  static uint8_t     state;               // The state of the fsm

  if( buffer != NULL ) {
    bufferposition = buffersize;
    buffersize += realsize;
    buffer = (uint8_t*)realloc((void*)buffer, buffersize);
  }
  
  // searching FFD8 - jpeg start
  for(size_t i=0; i<(realsize); i++) {
    
    uint8_t presentbyte = ((uint8_t *)ptr)[i];
    uint8_t nextbyte = ((uint8_t *)ptr)[i+1];
    
    switch( state ) {
      
    case 0:         // frame not started: search for start tag
      if( presentbyte == 0xFF && nextbyte == 0xD8 ) {
	bufferposition = 0;
	buffersize = realsize - i;
	buffer = (uint8_t*)malloc(buffersize);
	buffer[bufferposition++] = presentbyte;
	state = 1;
      }
      break;
      
    case 1:         // frame started: search for end tag
      buffer[bufferposition++] = presentbyte;

      if( presentbyte == 0xFF && nextbyte == 0xD9 ) {
	buffer[bufferposition++] = nextbyte;
	
	// build the frame and push it to the queue
	queuedframe f = {buffersize, buffer};
	me->enqueue(f);

	// release the buffer memory
	free(buffer);
	buffer = NULL;
	bufferposition = 0;
	buffersize = 0;
	state = 0;
	i++;
      }
      break;
      
    default:       
      // this should never be reached 
      break;
    }
  }   
  return realsize;
}

void CameraAxisDevice::enqueue(queuedframe f)
{
  if (Q_size < max_Q_size) {
    // Deep copy from "f" to "image"
    // Because memory of "f" will be released after enqueue, see: CameraAxisDevice::grab_frame()
    // Dont forget to release "image" after dequeue, see: CameraAxis::Main()
    uint8_t* image = (uint8_t*)malloc(f.image_count);
    memcpy(image, f.image, f.image_count);
    queuedframe frame = {f.image_count, image};
//    fprintf(stderr, "Getting lock (enqueue)\n");
    pthread_mutex_lock(&mutex);
//    fprintf(stderr, "Got lock (enqueue)\n");
    Q.push(frame);
    Q_size++;
    pthread_mutex_unlock(&mutex);
  }
}

/////////////////////////////////////////////////////////////
// Class of the camera driver
class CameraAxis : public Driver
{
public:
  // Constructor and deconstructor
  CameraAxis(ConfigFile* cf, int section);
  ~CameraAxis();

  // Driver's life cycle
  virtual int  Setup();
  virtual void Main();
  virtual int  Shutdown();

private:
  CameraAxisDevice*  Axis214;       // Camera device used: Axis 214
  char               camera_url[64];// URL of the camera
  player_camera_data camera_data;   // The data to be finally published to the server
};

// Plugin driver routines: CameraAxis_Init
//                         CameraAxis_Register
//                         player_driver_init

Driver* CameraAxis_Init(ConfigFile* cf, int section)
{
  // Create and return a new instance of this driver
  return((Driver*)(new CameraAxis(cf, section)));
}

void CameraAxis_Register(DriverTable* table)
{
  // Add this driver to the driver table
  table->AddDriver("CameraAxis", CameraAxis_Init);
}

extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("CameraAxis: Registering driver...");
    CameraAxis_Register(table);
    return(0);
  }
}

// Driver life cycle
//   CameraAxis::CameraAxis()   reads the config file
//   CameraAxis::Setup()        setups the camera and start CameraAxis::Main()
//   CameraAxis::Main()         publishes the data obtained from the camera 
//   CameraAxis::Shutdown()     stop CameraAxis::Main() and close the camera
//   CameraAxis::~CameraAxis()  currently not used
CameraAxis::CameraAxis(ConfigFile* cf, int section)
  : Driver(cf, section, true, PLAYER_MSGQUEUE_DEFAULT_MAXLEN, PLAYER_CAMERA_CODE)
{
  // Read the specified URL of the camera from configure file
  strncpy(this->camera_url, cf->ReadString(section, "ip", DEFAULT_CAMERA_IP), 
	  sizeof(this->camera_url));
  strcat(this->camera_url, "/axis-cgi/mjpg/video.cgi");

  // Currently the camera data is not configurable via configure file
  // Todo: add more field to configure file
  camera_data.width = 768;
  camera_data.height = 576;
  
  camera_data.bpp         = 24;                     
  camera_data.format      = PLAYER_CAMERA_FORMAT_RGB888;
  camera_data.fdiv        = 1;
  camera_data.compression = PLAYER_CAMERA_COMPRESS_JPEG;
}

CameraAxis::~CameraAxis()
{
  //nothing to do yet
}

int CameraAxis::Setup()
{   
  puts("CameraAxis: Setting up driver...");
  
  Axis214 = new CameraAxisDevice(camera_url);

  StartThread();
  return(0);
}

// This function will be run in a separate thread
void CameraAxis::Main() 
{
  while (true) {
    // Test if we are supposed to cancel this thread.
    pthread_testcancel();
    
    if (Axis214->framesWaiting()) {
      // Request image from camera and wrap to camera_data format
      queuedframe f = Axis214->dequeue();
      camera_data.image_count = f.image_count;
      camera_data.image = f.image;
      
      // Send the data to the server
      Publish(device_addr, PLAYER_MSGTYPE_DATA, PLAYER_CAMERA_DATA_STATE, &camera_data);
      
      // Free the memory, see: CameraAxisDevice::enqueue()
      free(f.image);
    }
  }
}

int CameraAxis::Shutdown()
// This function is called with the last client unsubscribes
{
  puts("\nCameraAxis: Shutting down driver...");

  StopThread ();
  delete Axis214;

  return(0);
}
