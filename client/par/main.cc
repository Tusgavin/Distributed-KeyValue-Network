#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "../../build/gen/key_value.grpc.pb.h"
#include "../../build/gen/key_value.pb.h"

struct KeyValuePairStruct {
   int key;
   std::string value;
};

class KeyValueClient {
private:
   std::unique_ptr<KeyValue::Stub> stub_;

public:
   KeyValueClient(std::shared_ptr<grpc::ChannelInterface> channel) : stub_(KeyValue::NewStub(channel)) {}

   /**
    * @brief Mmonta a mensagem de requisição e faz chamada do procedimento remoto InsertKeyValue do servidor e imprime o status
    * 
    * @param requestData Struct com os campos da mensagem KeyValuePair
    */
   void InsertKeyValue(const KeyValuePairStruct& requestData) {
      KeyValuePair req;
      req.set_key(requestData.key);
      req.set_value(requestData.value);
      
      InsertionStatus res;

      grpc::ClientContext context;
      grpc::Status status = stub_->InsertKeyValue(&context, req, &res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      std::cout << res.status() << std::endl;
   }

   /**
    * @brief Consulta o valor de uma chave em um servidor por meio do procedimento CheckoutValue e imprime o valor retornado
    * 
    * @param key Chave a ser consultada
    */
   void CheckoutValue(const int& key) {
      KeyToBeChecked req;
      req.set_key(key);

      ValueChecked res;

      grpc::ClientContext context;
      grpc::Status status = stub_->CheckoutValue(&context, req, &res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      std::cout << res.value() << std::endl;
   }

   /**
    * @brief Faz uma requisição para o servidor de pares enviar suas chaves para o servidor central e imprime o status
    * retornado
    * 
    * @param serviceId Endereço do servidor central
    */
   void Activate(const std::string& serviceId) {
      ServiceIdentifier req;
      req.set_serviceidentifier(serviceId);

      ServiceStatus res;

      grpc::ClientContext context;
      grpc::Status status = stub_->Activate(&context, req, &res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      std::cout << res.status() << std::endl;
   }

   /**
    * @brief Faz uma requisição para encerrar o servidor, imprime o status retornado
    * 
    */
   void EndExecution() {
      NoParameterKeyValue req;

      EndExecutionStatus res;

      grpc::ClientContext context;
      grpc::Status status = stub_->EndExecution(&context, req, &res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      std::cout << res.status() << std::endl;
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

   KeyValueClient keyValueClient(channel);

   std::cout << ">> Client connected on " << address << "..." << std::endl;

   
   /* Lê a entrada do usuário até digite o comando de encerrar o servidor */
   while (1) {
      std::string command;
      std::getline(std::cin, command);

      std::vector<std::string> parsedCommand = parseCommand(command);

      if (parsedCommand[0] == "I") {
         // Insere o valor parsedCommand[2] na chave parsedCommand[1]
         KeyValuePairStruct newKeyValuePair;
         newKeyValuePair.key = std::stoi(parsedCommand[1]);
         newKeyValuePair.value = std::string(parsedCommand[2]);

         keyValueClient.InsertKeyValue(newKeyValuePair);
      } else if (parsedCommand[0] == "C") {
         // Checa o valor na chave parsedCommand[1]
         const int key = std::stoi(parsedCommand[1]);

         keyValueClient.CheckoutValue(key);
      } else if (parsedCommand[0] == "A") {
         // Ativa o serviço de parsedCommand[1]
         const std::string serviceId = std::string(parsedCommand[1]);

         keyValueClient.Activate(serviceId);
      } else if (parsedCommand[0] == "T") {
         // Termina servidor
         keyValueClient.EndExecution();

         return 0;
      }
   }
}