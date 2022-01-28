#include <iostream>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <future>
#include <grpcpp/grpcpp.h>

#define DEBUG 0

#include "../../build/gen/map_server.grpc.pb.h"
#include "../../build/gen/map_server.pb.h"

/* Stores value that will be used in the future by another thread */
std::promise<void> shutdownServerReq;

class MapServerImpl final : public KeyMap::Service {
   std::map<int, std::string> keyAddressTable;

   grpc::Status Register(grpc::ServerContext* context, const ServerInfo* req, NumberOfProcessedKeys* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function Register()" << std::endl;
#endif
      int processedKeys = 0;

      auto outputProcessedKeys = [](const auto &insertionStatus) {
         const auto [newPairIterator, success] = insertionStatus;

         (success) ? std::cout << ">> Inserção: " : std::cout << ">> Atualização: ";

         std::cout << "Chave " << (*newPairIterator).first << " com Valor " << (*newPairIterator).second << std::endl;
      };

      for (auto it = req->keys().begin(); it != req->keys().end(); ++it) {
#if DEBUG
         outputProcessedKeys(this->keyAddressTable.insert_or_assign(*it, req->serveraddress()));
#else
         this->keyAddressTable.insert_or_assign(*it, req->serveraddress());
#endif
         ++processedKeys;
      }

      res->set_processedkeys(processedKeys);

      return grpc::Status::OK;
   };

   grpc::Status Mapping(grpc::ServerContext* context, const SearchKey* req, ServerAddress* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function Mapping()" << std::endl;
#endif
      const auto it = this->keyAddressTable.find(req->key());

      (it != this->keyAddressTable.end()) ? res->set_serveraddress(it->second) : res->set_serveraddress("");

      return grpc::Status::OK;
   };

   grpc::Status EndExecution(grpc::ServerContext* context, const NoParameterKeyMap* req, NumberOfRegisteredKeys* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function Activate()" << std::endl;
#endif

      res->set_registeredkeys(keyAddressTable.size());

      shutdownServerReq.set_value();
      
      return grpc::Status::OK;
   };
};

int main(int argc, char* argv[]) {
   std::string port = std::string(argv[1]);
   std::string address = std::string("0.0.0.0:") + port;

   MapServerImpl service;

   grpc::ServerBuilder builder;
   builder.AddListeningPort(address, grpc::InsecureServerCredentials());
   builder.RegisterService(&service);

   std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
   std::cout << ">> Running center server on " << address << "..." << std::endl;

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