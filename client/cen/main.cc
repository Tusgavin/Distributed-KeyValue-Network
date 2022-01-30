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

   /**
    * @brief Busca pelo servidor de pares com a chave por meio do servidor central. Se a chave existir, faz uma requisição
    * para o endereço do servidor de pares retornado e imprime o valor
    *  
    * @param key Chave a ser consultada
    */
   void CheckoutServerAddressWithKey(const int& key) {
      SearchKey req;
      req.set_key(key);

      ServerAddress res;

      grpc::ClientContext context;
      grpc::Status status = stub_->Mapping(&context, req, &res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }
      
      /* Se o servidor dor igual "", significa que não existe a chave */
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
      } else {
         std::cout << "" << std::endl;
      }
   }

   /**
    * @brief Faz uma requisição para encerrar o servidor central, imprime o número de chaves armazenado
    * 
    */
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

/**
 * @brief Lê o comando escrito pelo usuário e separa cada valor que está entre as vírgulas
 * 
 * @param stringfiedCommand Comando digitado pelo usuário (e.g. I,1,valor1)
 * @return std::vector<std::string> (e.g. ["I", "1", "valor1"])
 */
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

   while (1) {
      std::string command;
      std::getline(std::cin, command);

      std::vector<std::string> parsedCommand = parseCommand(command);

      if (parsedCommand[0] == "C") {
         // Consulta o endereço que guarda a chave parsedCommand[1]
         const int key = std::stoi(parsedCommand[1]);

         mapServerClient.CheckoutServerAddressWithKey(key);
      }  else if (parsedCommand[0] == "T") {
         // Termina servidor
         mapServerClient.EndExecution();

         return 0;
      }
   }
}