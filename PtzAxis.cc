/****************************************************************************\
 *  PtzAxis version 0.1a                                                    *
 *  A PTZ Plugin Driver for the Player/Stage robot server                   *
 *                                                                          *
 *  Copyright (C) 2010 Zhanwu Xiong                                         *
 *  zhanwu at cvc dot uab dot es     http://cvc.uab.es/~zhanwu              *
 *                                                                          *
 *  A Player/Stage plugin driver for Axis 214 camera's PTZ device           *
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

#include <stdio.h>
#include <string.h>
#include <string>

#include <pthread.h>
#include <curl/curl.h>

#include <libplayercore/playercore.h>

#define DEFAULT_PTZ_SPEED   90
#define PTZ_SLEEP_TIME_USEC 10000                // 100 Hz

typedef struct _ptz_state {
  float pan;               // degree
  float tilt;              // degree
  int   zoom;              // device specified 
} ptz_state;

// Class of the PTZ device
class PtzAxisDevice
{
public:
  PtzAxisDevice(string);
  ~PtzAxisDevice();
  
  // Interface for controling
  bool move(float pan, float tilt, int speed);             // Absolute position move
  bool continuousMove(int pan, int tilt, int speed);       // Continuous move
  bool zoom(int zoom);                                     // Continuous move

  // Interface for querying
  bool updateState();                            // Update the ptz_state of the device
  ptz_state state;                               // The ptz_state of the device

protected:
  // Tool function of updateState
  static size_t stateParser(void *ptr, size_t size, size_t nmemb, void *data);

  // Internal data
  string ptz_ip;                                 // Ip of the camera
  string ptz_cmd_prefix;                         // For wrapping command

};

PtzAxisDevice::PtzAxisDevice(string ip)
{
  puts("PtzAxisDevice: Start device");
  ptz_ip = ip;
  ptz_cmd_prefix = ptz_ip.append("/axis-cgi/com/ptz.cgi?");
}

PtzAxisDevice::~PtzAxisDevice()
{
  puts("PtzAxisDevice: Device closed");
}

bool PtzAxisDevice::move(float pan, float tilt, int speed)
{
  // Move the camera to a specified PT position
  string ptz_cmd; 
  CURL*  ptz_curl = curl_easy_init();
  char   ptz_cmd_suffix[64];
  int n = sprintf(ptz_cmd_suffix, "pan=%.1f&tilt=%.1f&speed=%d&autofocus=on",
		  pan, tilt, speed);
  ptz_cmd = ptz_cmd_prefix;
  ptz_cmd.append(ptz_cmd_suffix, n);
  
  curl_easy_setopt(ptz_curl, CURLOPT_URL, ptz_cmd.c_str());
  curl_easy_perform(ptz_curl);
  
  curl_easy_cleanup(ptz_curl); 

  puts("command:");
  puts(ptz_cmd.c_str());

  return 0;
}

//
// Execute a continuous pan/tilt/zoom move.  The three inputs are
// assumed to be speeds, the combination of which constitute the
// vector in which the camera is moving in p/t/z.
//
bool PtzAxisDevice::continuousMove(int pan, int tilt, int zoom)
{
  string ptz_cmd; 
  CURL*  ptz_curl = curl_easy_init();
  char   ptz_cmd_suffix[64];
  int n = sprintf(ptz_cmd_suffix, "continuouspantiltmove=%d,%d&continuouszoommove=%d", pan, tilt, zoom);
  ptz_cmd = ptz_cmd_prefix;
  ptz_cmd.append(ptz_cmd_suffix, n);
  
  curl_easy_setopt(ptz_curl, CURLOPT_URL, ptz_cmd.c_str());
  curl_easy_perform(ptz_curl);
  
  curl_easy_cleanup(ptz_curl); 

  puts("command:");
  puts(ptz_cmd.c_str());

  return 0;
}

bool PtzAxisDevice::zoom(int zoom)
{
  // Zoom the camera to a specified value
  string ptz_cmd; 
  CURL*  ptz_curl = curl_easy_init();
  char   ptz_cmd_suffix[64];
  int n = sprintf(ptz_cmd_suffix, "zoom=%d&autofocus=on", zoom);
  ptz_cmd = ptz_cmd_prefix;
  ptz_cmd.append(ptz_cmd_suffix, n);
  
  curl_easy_setopt(ptz_curl, CURLOPT_URL, ptz_cmd.c_str());
  curl_easy_perform(ptz_curl);
  
  curl_easy_cleanup(ptz_curl); 
  
  puts("command:");
  puts(ptz_cmd.c_str());

  return 0;
}

bool PtzAxisDevice::updateState()
{
  string ptz_cmd;                      
  CURL*  ptz_curl = curl_easy_init();
  char   ptz_cmd_suffix[] = "query=position";
  ptz_cmd = ptz_cmd_prefix;
  ptz_cmd.append(ptz_cmd_suffix, strlen(ptz_cmd_suffix));

  curl_easy_setopt(ptz_curl, CURLOPT_URL, ptz_cmd.c_str());
  curl_easy_setopt(ptz_curl, CURLOPT_WRITEFUNCTION, &PtzAxisDevice::stateParser);
  curl_easy_setopt(ptz_curl, CURLOPT_WRITEDATA, &state);

  curl_easy_perform(ptz_curl);
  curl_easy_cleanup(ptz_curl); 

  return 0;
}

size_t PtzAxisDevice::stateParser(void *ptr, size_t size, size_t nmemb, void *data)
{
  ptz_state* state = (ptz_state*) data;
  
  if(strstr((char*)ptr, "Error"))
    puts("error");
  else {
    char* pan  = strstr((char*)ptr, "pan=") + 4;
    char* tilt = strstr((char*)ptr, "tilt=") + 5;
    char* zoom = strstr((char*)ptr, "zoom=") + 5;
    pan[strchr(pan, '\n')-pan]    = '\0';
    tilt[strchr(tilt, '\n')-tilt] = '\0';
    zoom[strchr(zoom, '\n')-zoom] = '\0';
    
    state->pan  = atof(pan);
    state->tilt = atof(tilt);
    state->zoom = atoi(zoom);
  }
  return (size*nmemb);
}


/////////////////////////////////////// PART TWO ///////////////////////////////////////

// This stucture is used to pass information about the command 
// to the new thread excuting the command
typedef struct _driver_ptz_cmd {
  PtzAxisDevice*   ptz;       // The device
  player_ptz_cmd_t cmd;       // The command (p,t,z,ps,ts)
  int              zoom;      // Current zoom of the device
} driver_ptz_cmd;

// Class of the PTZ driver
class PtzAxis : public Driver
{
public:
  PtzAxis(ConfigFile* cf, int section);   
  
  // Driver's life cycle
  virtual int  Setup();
  virtual void Main();         // This will be run in a new thread
  virtual int  Shutdown();
  
  // Message Handler
  int          ProcessMessage(QueuePointer &resp_queue, player_msghdr* hdr, void* data);
  static void* ExecuteCommand(void *ptr);   // This will be run in a new thread
  static void* ExecuteContinuousCommand(void *ptr);   // This too, for continuous
  static       pthread_mutex_t mutex; 
  
private:
  PtzAxisDevice*    Axis214;    // The ptz device
  char              ptz_ip[64]; // IP address of the ptz device
  
  player_ptz_cmd_t* cmd;        // Command received from player
  player_ptz_data_t data;       // Data to be published to player
  int _mode;
};

pthread_mutex_t PtzAxis::mutex; 

// Plugin driver routines: PtzAxis_Init
//                         PtzAxis_Register
//                         player_driver_init

Driver* PtzAxis_Init(ConfigFile* cf, int section)
{
  return static_cast<Driver*> (new PtzAxis(cf, section));
}

void PtzAxis_Register(DriverTable* table)
{
  table->AddDriver("PtzAxis", PtzAxis_Init);
}

extern "C" {
  int player_driver_init(DriverTable* table)
  {
    puts("PtzAxis: Registering driver...");
    PtzAxis_Register(table);
    return 0;
  }
}

// Driver life cycle
//   PtzAxis::PtzAxis()        // Read configure from files
//   PtzAxis::Setup()          // When first subscriber comes
//   PtzAxis::Main()           // 
//   PtzAxis::Shutdown()       // When last subscriber left

PtzAxis::PtzAxis(ConfigFile* cf, int section) 
  : Driver(cf, section, true, PLAYER_MSGQUEUE_DEFAULT_MAXLEN, PLAYER_PTZ_CODE)
{
  strncpy(ptz_ip, cf->ReadString(section, "ip", DEFAULT_PTZ_IP),
	  sizeof(ptz_ip));
}

int PtzAxis::Setup ()
{
  puts("PtzAxis: Setting up driver...");  
    
  // Connect to the ptz device
  Axis214 = new PtzAxisDevice(ptz_ip);

  // Initialization 
  Axis214->updateState();
  data.zoom = Axis214->state.zoom;

  // Start the main thread
  StartThread();

  return 0;
}

void PtzAxis::Main()
{
  while(true) {
    pthread_testcancel();

    ProcessMessages();

    // Publish the PTZ device's state to the server
    Axis214->updateState();
    data.pan  = Axis214->state.pan;
    data.tilt = Axis214->state.tilt;
    data.zoom = Axis214->state.zoom;

    Publish(device_addr, PLAYER_MSGTYPE_DATA, PLAYER_PTZ_DATA_STATE,
	    &data, sizeof (player_ptz_data_t), NULL);

    //    printf("Publish:\npan:%.1f, tilt:%.1f, zoom:%d, speed:%d\n", 
    //	   data.pan, data.tilt, (int)data.zoom, (int)data.panspeed);

    // Repeat frequency (default to 10 Hz)
    usleep (PTZ_SLEEP_TIME_USEC);
  }
}

int PtzAxis::Shutdown ()
{
  puts("\nPtzAxis: Shutting down driver..."); 
  
  StopThread ();
  delete Axis214;
  
  return 0;
}

int PtzAxis::ProcessMessage(QueuePointer &resp_queue, player_msghdr * hdr, void * data)
{
  assert (hdr);
  
  // REQ_GENERIC
  if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, PLAYER_PTZ_REQ_GENERIC, device_addr))
    Publish(device_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, hdr->subtype);
  else
    
    // REQ_CONTROL_MODE
    if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, PLAYER_PTZ_REQ_CONTROL_MODE, device_addr)) {
      //      Publish(device_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, hdr->subtype);
      player_ptz_req_control_mode* new_mode = reinterpret_cast<player_ptz_req_control_mode*> (data);
      _mode = new_mode->mode;
      fprintf(stderr, "Setting mode to: %d\n", _mode);
      Publish(device_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, hdr->subtype);
    }
    else
      
      // CMD mode:
      if (Message::MatchMessage(hdr, PLAYER_MSGTYPE_CMD, PLAYER_PTZ_CMD_STATE, device_addr)) {
	cmd = reinterpret_cast<player_ptz_cmd_t*> (data);
	
	printf("Message:\npan:%.1f, tilt:%.1f, zoom:%d, speed:%d\n", 
	       cmd->pan, cmd->tilt, (int)cmd->zoom, (int)cmd->panspeed);

	// Deep copy the data needed by excuting a command
	pthread_mutex_lock(&mutex); 
	driver_ptz_cmd *command = new driver_ptz_cmd;    // Delete in void* PtzAxis::ExecuteCommand(void *ptr)
	command->ptz  = Axis214;                         
	command->zoom = this->data.zoom;
	command->cmd.pan  = cmd->pan;
	command->cmd.tilt = cmd->tilt;
	command->cmd.zoom = cmd->zoom;
	command->cmd.panspeed  = ((int)cmd->panspeed == 0) ? DEFAULT_PTZ_SPEED : (int)cmd->panspeed;
	command->cmd.tiltspeed = command->cmd.panspeed;
	pthread_mutex_unlock(&mutex);  	
		
	// We have to switch on the mode, since the thread methods are static.
	pthread_t foo;
	if (_mode == PLAYER_PTZ_POSITION_CONTROL) {
	  pthread_create(&foo, NULL, &PtzAxis::ExecuteCommand, (void*)command);
	}
	else {
	  pthread_create(&foo, NULL, &PtzAxis::ExecuteContinuousCommand, (void*)command);
	}
      }
      else
	return -1;
  return 0;
}

// Position control mode...  Run in own thread.
void* PtzAxis::ExecuteCommand(void *ptr)
{
  driver_ptz_cmd* command = (driver_ptz_cmd*) ptr;

  // Only excute a zoom action when a different zoom is request
  // Because the zoom action will block the camera from excuting other action
  if ((int)command->cmd.zoom != (int)command->zoom) {
    command->ptz->zoom((int)command->cmd.zoom);
  }
  
  command->ptz->move(command->cmd.pan, command->cmd.tilt, (int)command->cmd.panspeed);
  delete command;                      
  return (NULL);
}

// Velocity control mode commands.  Run in own thread.
void* PtzAxis::ExecuteContinuousCommand(void *ptr)
{
  driver_ptz_cmd* command = (driver_ptz_cmd*) ptr;
  command->ptz->continuousMove((int) command->cmd.pan, (int) command->cmd.tilt, (int) command->cmd.zoom);
  delete command;                      
  return (NULL);
}

