#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "../../build/gen/map_server.grpc.pb.h"
#include "../../build/gen/map_server.pb.h"
#include "../../build/gen/key_value.grpc.pb.h"
#include "../../build/gen/key_value.pb.h"

class MapServerClient {
private:
   std::unique_ptr<KeyMap::Stub> stub_;

public:
   MapServerClient(std::shared_ptr<grpc::ChannelInterface> channel) : stub_(KeyMap::NewStub(channel)) {}

   void CheckoutServerAddressWithKey(const int& key) {
      SearchKey req;
      req.set_key(key);

      ServerAddress res;

      grpc::ClientContext context;
      grpc::Status status = stub_->Mapping(&context, req, &res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      if (res.serveraddress() != "") {
         auto channel = grpc::CreateChannel(res.serveraddress(), grpc::InsecureChannelCredentials());
         std::unique_ptr<KeyValue::Stub> stub_(KeyValue::NewStub(channel));

         KeyToBeChecked _req;
         _req.set_key(key);

         ValueChecked _res;

         grpc::ClientContext _context;
         grpc::Status _status = stub_->CheckoutValue(&_context, _req, &_res);

         if (!_status.ok()) {
            std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
         }

         std::cout << res.serveraddress() << ":" << _res.value() << std::endl;
      }
   }

   void EndExecution() {
      NoParameterKeyMap req;

      NumberOfRegisteredKeys res;

      grpc::ClientContext context;
      grpc::Status status = stub_->EndExecution(&context, req, &res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      std::cout << res.registeredkeys() << std::endl;
   }
};

std::vector<std::string> parseCommand(std::string stringfiedCommand) {
   const std::string delimiter = ",";
   size_t pos = 0;

   std::string token;
   std::vector<std::string> parsedCommand;

   while ((pos = stringfiedCommand.find(delimiter)) != std::string::npos) {
      token = stringfiedCommand.substr(0, pos);
      stringfiedCommand.erase(0, pos + delimiter.length());
      parsedCommand.push_back(token);
   }

   parsedCommand.push_back(stringfiedCommand);

   return parsedCommand;
};

int main(int argc, char* argv[]) {
   std::string address = std::string(argv[1]);

   auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());

   MapServerClient mapServerClient(channel);

   std::cout << ">> Client connected on " << address << "..." << std::endl;

   while (1) {
      std::string command;
      std::getline(std::cin, command);

      std::vector<std::string> parsedCommand = parseCommand(command);

      if (parsedCommand[0] == "C") {
         // Checkout value stored in key parsedCommand[1]
         const int key = std::stoi(parsedCommand[1]);

         mapServerClient.CheckoutServerAddressWithKey(key);
      }  else if (parsedCommand[0] == "T") {
         // End server
         mapServerClient.EndExecution();

         return 0;
      }
   }
}