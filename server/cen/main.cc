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

/* Armazena o valor que será usado no futuro por outra thread */
std::promise<void> shutdownServerReq;

class MapServerImpl final : public KeyMap::Service {
   std::map<int, std::string> keyAddressTable;

   /**
    * @brief Insere ou atualiza o pares de chave-servidor no dicionário de mapeamento de servidores.
    * Ajusta a response para o numero de pares inseridos/atualizados.
    * 
    * @return grpc::Status 
    */
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

   /**
    * @brief Busca pelo valor do endereço de servidor correspondente à chave enviada na requisição. Se na chave não
    * existir no dicionário, o endereço na response é "".
    * 
    * @return grpc::Status 
    */
   grpc::Status Mapping(grpc::ServerContext* context, const SearchKey* req, ServerAddress* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function Mapping()" << std::endl;
#endif
      const auto it = this->keyAddressTable.find(req->key());

      (it != this->keyAddressTable.end()) ? res->set_serveraddress(it->second) : res->set_serveraddress("");

      return grpc::Status::OK;
   };

   /**
    * @brief Termina a execução do servidor por meio de uma chamada de procedimento feita por um cliente
    * 
    * @return grpc::Status 
    */
   grpc::Status EndExecution(grpc::ServerContext* context, const NoParameterKeyMap* req, NumberOfRegisteredKeys* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function Activate()" << std::endl;
#endif

      res->set_registeredkeys(keyAddressTable.size());

      /* Faz um ajuste de valor na promisse shutdownServerReq para ativar shutdownServerFuture e desbloquear o código na thread principal */
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

   /* Cria função lambda para rodar o sevrer */
   auto serverWait = [&]() {
      server->Wait();
   };

   /* shutdownServerFuture vai receber um valor da thread filha */
   auto shutdownServerFuture = shutdownServerReq.get_future();

   /* Roda a função lambda em uma thread filha */
   std::thread serverThread(serverWait);

   /* Bloqueia até que a thread filha ajuste o valor de shutdownServerFuture */
   shutdownServerFuture.wait();

   /* Encerra o server */
   server->Shutdown();

   /* Bloqueia até que a thread filha encerre */
   serverThread.join();

   return 0;
}