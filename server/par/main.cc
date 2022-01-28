#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <future>
#include <grpcpp/grpcpp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "../../build/gen/key_value.grpc.pb.h"
#include "../../build/gen/key_value.pb.h"
#include "../../build/gen/map_server.grpc.pb.h"
#include "../../build/gen/map_server.pb.h"

#define DEBUG 0
#define DEFAULT_PORT "41410"
#define DEFAULT_ARGS 2

std::string PORT = DEFAULT_PORT;
int ARGS = DEFAULT_ARGS;

/* Stores value that will be used in the future by another thread */
std::promise<void> shutdownServerReq;

struct ServerInfoStruct {
   std::string serverAddress;
   std::vector<int> keys;
};

std::string checkoutHostname() {
   char buffer[HOST_NAME_MAX];
   
   if (gethostname(buffer, HOST_NAME_MAX)) {
      std::cout << "Could not hostname" << std::endl;
   }

   return std::string(buffer);
}

class KeyValueImpl final : public KeyValue::Service {
   std::map<int, std::string> keyValueTable;

   ServerInfoStruct GetAllServerKeysAndAddress() {
      ServerInfoStruct serverInfo;

      for (auto it = this->keyValueTable.begin(); it != this->keyValueTable.end(); ++it) {
         serverInfo.keys.push_back(it->first);
      }

      serverInfo.serverAddress = checkoutHostname() + ":" + PORT;

      return serverInfo;
   }

   grpc::Status InsertKeyValue(grpc::ServerContext* context, const KeyValuePair* req, InsertionStatus* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function InsertKeyValue()" << std::endl;
#endif

      const auto [newPairIterator, success] = this->keyValueTable.insert({req->key(), req->value()});

      (success) ? res->set_status(1) : res->set_status(-1);

      return grpc::Status::OK;
   };

   grpc::Status CheckoutValue(grpc::ServerContext* context, const KeyToBeChecked* req, ValueChecked* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function CheckoutValue()" << std::endl;
#endif

      const auto it = this->keyValueTable.find(req->key());

      (it != this->keyValueTable.end()) ? res->set_value(it->second) : res->set_value("");

      return grpc::Status::OK;
   };

   grpc::Status Activate(grpc::ServerContext* context, const ServiceIdentifier* req, ServiceStatus* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function Activate()" << std::endl;
#endif
      if (ARGS < 3) {
         res->set_status(0);

         return grpc::Status::OK;
      }

      ServerInfo _req;
      NumberOfProcessedKeys _res;
      grpc::ClientContext _context;

      ServerInfoStruct serverInfo = GetAllServerKeysAndAddress();

      _req.set_serveraddress(serverInfo.serverAddress);
      
      for (auto it = serverInfo.keys.begin(); it != serverInfo.keys.end(); ++it) {
         _req.add_keys(*it);
      }

      std::string address = req->serviceidentifier();

      auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
      std::unique_ptr<KeyMap::Stub> stub_(KeyMap::NewStub(channel));

      grpc::Status status = stub_->Register(&_context, _req, &_res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      res->set_status(_res.processedkeys());

      return grpc::Status::OK;
   };

   grpc::Status EndExecution(grpc::ServerContext* context, const NoParameterKeyValue* req, EndExecutionStatus* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function EndExecution()" << std::endl;
#endif

      res->set_status(0);

      shutdownServerReq.set_value();
      
      return grpc::Status::OK;
   };
};

int main(int argc, char* argv[]) {
   std::string port = std::string(argv[1]);
   PORT = port;
   std::string address = std::string("0.0.0.0:") + port;

   KeyValueImpl service;

   grpc::ServerBuilder builder;
   builder.AddListeningPort(address, grpc::InsecureServerCredentials());
   builder.RegisterService(&service);

   std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
   std::cout << ">> Running server on " << address << "..." << std::endl;

   /* Create lambda function to run server */
   auto serverWait = [&]() {
      server->Wait();
   };

   /* shutdownServerFuture will receive the value from the child thread */
   auto shutdownServerFuture = shutdownServerReq.get_future();

   /* Run the lambda function in child thread */
   std::thread serverThread(serverWait);

   /* Blocks until the child thread gives the value */
   shutdownServerFuture.wait();

   /* Shutdown server after 'shutdownServerFuture' gets the value */
   server->Shutdown();

   /* Blocks until the child thread ends */
   serverThread.join();

   return 0;
}