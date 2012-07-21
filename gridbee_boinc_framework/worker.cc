#include "ppapi/cpp/var.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/completion_callback.h"

#include <cstdio>
#include <string>
#include <sstream>

#include <stdlib.h>

int my_main();

pp::Instance* globalInstance;

void PostMessageOnMainThread(void* data, int32_t err) {
  std::string* messagePtr = (std::string*)data;
  pp::Var msg = pp::Var(*messagePtr);  
  globalInstance->PostMessage(msg);
  delete messagePtr;
}
void PostMessage(const std::string &message){
  std::string* messagePtr = new std::string(message);
  pp::CompletionCallback cc(PostMessageOnMainThread, messagePtr);
  pp::Module::Get()->core()->CallOnMainThread(0, cc, PP_OK);
}



void *worker_thread_init(void *arg) {
  globalInstance = (pp::Instance*)arg;
  
  PostMessage(std::string("{\"command\":\"debug\", \"data\":{\"str\":\"Worker thread is alive.\"}}"));
  //Arrives after first printf in my_main (This is wrong!)
  my_main();
}
