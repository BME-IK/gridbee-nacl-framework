#include <cstdio>
#include <string>
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"

#include "nacl-mounts/base/Entry.h"
#include "nacl-mounts/base/UrlLoaderJob.h"
#include "nacl-mounts/console/JSPipeMount.h"
#include "nacl-mounts/console/JSPostMessageBridge.h"

#include "json/json.h"

#include "worker.h"

#include "base64.h"
#include "md5.h"

using namespace std;

extern "C" int umount(const char *path);
extern "C" int mount(const char *type, const char *dir, int flags, void *data);
extern "C" int simple_tar_extract(const char *path);
extern "C" int mkdir(const char *path, mode_t mode);

/// The Instance class.  One of these exists for each instance of your NaCl
/// module on the web page.  The browser will ask the Module object to create
/// a new Instance for each occurence of the <embed> tag that has these
/// attributes:
///     type="application/x-nacl"
///     src="client.nmf"
/// To communicate with the browser, you must override HandleMessage() for
/// receiving messages from the borwser, and use PostMessage() to send messages
/// back to the browser.  Note that this interface is entirely asynchronous.
Json::Value checkpoint_data;
class ClientInstance : public pp::Instance
{
 private:
	char* multipartData;
	char* ptr;
	int msgCount;

 public:
  /// The constructor creates the plugin-side instance.
  /// @param[in] instance the handle to the browser-side plugin instance.
  explicit ClientInstance(PP_Instance instance) : pp::Instance(instance)
  {
    worker_thread_started = false;
    jspipe_ = NULL;
    jsbridge_ = NULL;
  }
  virtual ~ClientInstance() 
	{
    if (jspipe_) delete jspipe_;
    if (jsbridge_) delete jsbridge_;
    if (runner_) delete runner_;
  }
  
  virtual bool Init(uint32_t argc, const char* argn[], const char* argv[]) 
	{
    PostMessage(std::string() + "{\"command\":\"debug\", \"data\":{\"str\":\"The framework library was compiled at " + __DATE__ + " " + __TIME__ + "\"}}");
		
    runner_ = new MainThreadRunner(this);
    jsbridge_ = new JSPostMessageBridge(runner_);
    jspipe_ = new JSPipeMount();
    jspipe_->set_outbound_bridge(jsbridge_);
    // Replace stdin, stdout, stderr with js console.
    mount("jspipe", "/jspipe", 0, jspipe_);
    close(0);
    close(1);
    close(2);
    int fd;
    fd = open("/jspipe/0", O_RDONLY);
    assert(fd == 0);
    fd = open("/jspipe/1", O_WRONLY);
    assert(fd == 1);
    fd = open("/jspipe/2", O_WRONLY);
    assert(fd == 2);
    
    mkdir("home",777);
    chdir("home");    
    
    //PostMessage("{\"debug\": {\"str\":\"ready\"}}");
    
		msgCount = 0;
		
    return true;
  }

  /// Handler for messages coming in from the browser via postMessage().
  virtual void HandleMessage(const pp::Var& var_message)
	{
    if (!var_message.is_string())
		{
			return;
		}
		
		HandleMessage2(var_message.AsString());
	}
	
	void HandleMessage2(string message)
	{
		Json::Value root;   // will contain the root value after parsing.
    Json::Reader reader;
    bool parsingSuccessful = reader.parse( message, root );
    if ( !parsingSuccessful )
		{
      PostMessage("{\"command\":\"exception\",\"data\":{\"str\":\"NaCl failed to parse inputted JSON."
                  " Ignored. " + reader.getFormattedErrorMessages() + "\"}}");
      return;
    }
		
    const Json::Value command = root["command"];
    const Json::Value data = root["data"];
		const Json::Value count = root["count"];
		const Json::Value size = root["size"];
   
    if(command==NULL && command.isString())
		{
      PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"JSON Object must contain 'command' of type string. Ignored.\"}}");
      return;
    }
    
    std::string commandStr = command.asString();
	 
    if(commandStr == "run")
		{
      if(worker_thread_started)
			{
        PostMessage("{\"exception\":{\"str\":\"'run' received twice. Second ignored.\"}}");
      }
			else
			{
        PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"Starting worker thread.\"}}");
        pthread_create(&worker_thread_, NULL, worker_thread_init, this);
        worker_thread_started = true;  
      }
    }
    else
    if(commandStr == "data")
		{
      if(data["openname"]==NULL && data["openname"].isString())
			{
        PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"No 'openname' of type string provided in command 'data'.\"}}");
        return;
      }
			else
			if(data["inputdata"]==NULL && data["inputdata"].isString())
			{
        PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"No 'inputdata' of type string provided in command 'data'.\"}}");
        return;
      }
			else
			{			
				string inputData = data["inputdata"].asString();
				std::string decoded = r_base64_decode(inputData.c_str());

				// ----------------------------		
				char hash[2*16];	// ebbe kerül majd az md5 hash stringje (16 bájt, bájtonként 2 karakteren)
				const char* md5Data = decoded.c_str();	// erre kell md5-öt számolni (a fájlba írandó adatra)
				md5_state_t state;
				md5_byte_t digest[16];
				md5_init(&state);
				md5_append(&state, (const md5_byte_t *)md5Data, strlen(md5Data));
				md5_finish(&state, digest);				
				//printf("MD5 of received data: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x \n", digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);
				sprintf(hash, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7], digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]);
				// ----------------------------
				
				const char* fileName = data["openname"].asString().c_str();
				FILE* file = fopen(fileName,"w");
				fputs(decoded.c_str(), file);
				fclose(file);
				
				const char* ss = decoded.c_str();
				printf("Data file written to memory disk. (filename: %s, size: %d, md5: %s) ", fileName, strlen(ss), hash);
								
        //PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"Data file written to memory disk.\"}}");
      }
    }
    else
    if(commandStr == "program")
		{
      PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"Command 'program' ignored for now.\"}}");
    }    
    else
    if(commandStr == "checkpoint")
		{
      PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"Checkpoint data received.\"}}");
      checkpoint_data = Json::Value(data);
    }
		else
    if(commandStr == "multipart")
		{
			PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"MULTIPART OK\"}}");
			
			if (count == NULL || !count.isString())
			{
				PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"count == null || !count.isString()\"}}");
			}
			else
			if (size == NULL || !size.isString())
			{
				PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"size == null || !size.isString()\"}}");
			}
			else
			{
				int s = atoi(size.asString().c_str());						// maximum ekkora üzenetre számítok összesen
				multipartData = (char*)malloc(s * sizeof(char));		// itt kezdõdik majd az összerakott üzenet
				if (multipartData == NULL)
				{
					PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"NEM SIKERULT HELYET FOGLALNI!!!!\"}}");
				}
				else
				{
					PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"MALLOC OK\"}}");
				}
				
				ptr = multipartData;															// ide kell rakni a következõ üzenet darabot
				msgCount = atoi(count.asString().c_str());				// ennyi darab üzenet fog jönni
			}
    }
		else
    if(commandStr == "nextpart")
		{
			PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"NEXTPART OK\"}}");
		
			if (msgCount == 0)
			{
				PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"PARA VAN! NEM VARTAM ILYEN UZENETET!\"}}");
				return;
			}
			
			const char* cdata = data.asString().c_str();
			strcpy(ptr, cdata);
			ptr = ptr + strlen(cdata);
			msgCount--;
			
			if (msgCount == 0)
			{
				PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"UTOLSOT KAPTAM.\"}}");
				std::string decoded = r_base64_decode(multipartData);
				free(multipartData);
				//PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"BASE64 DECODED multipart: "+decoded+"\"}}");
				HandleMessage2(decoded);							// megjött az összes része a nagy üzenetnek, ideje feldolgozni				
			}
			else
			{
				PostMessage("{\"command\":\"debug\", \"data\":{\"str\":\"MEG KELL KAPNOM\"}}");
			}
    }
    else
		{
      PostMessage("{\"command\":\"debug\": {\"str\":\"Unknown command.\"}}");    
    }
  }
  
  private:
  pthread_t worker_thread_;
  bool worker_thread_started;
  JSPipeMount *jspipe_;
  JSPostMessageBridge *jsbridge_;
  MainThreadRunner *runner_;
};

/// The Module class.  The browser calls the CreateInstance() method to create
/// an instance of your NaCl module on the web page.  The browser creates a new
/// instance for each <embed> tag with type="application/x-nacl".
class ClientModule : public pp::Module
{
 public:
  ClientModule() : pp::Module() {}
  virtual ~ClientModule() {}

  /// Create and return a ClientInstance object.
  /// @param[in] instance The browser-side instance.
  /// @return the plugin-side instance.
  virtual pp::Instance* CreateInstance(PP_Instance instance) 
	{
    return new ClientInstance(instance);
  }
};

namespace pp 
{
	/// Factory function called by the browser when the module is first loaded.
	/// The browser keeps a singleton of this module.  It calls the
	/// CreateInstance() method on the object you return to make instances.  There
	/// is one instance per <embed> tag on the page.  This is the main binding
	/// point for your NaCl module with the browser.
	Module* CreateModule() 
	{
		return new ClientModule();
	}
}  // namespace pp


