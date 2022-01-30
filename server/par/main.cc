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

/* Variáveis globais */
std::string PORT = DEFAULT_PORT;
int ARGS = DEFAULT_ARGS;

/* Armazena o valor que será usado no futuro por outra thread */
std::promise<void> shutdownServerReq;

struct ServerInfoStruct {
   std::string serverAddress;
   std::vector<int> keys;
};

/**
 * @brief Consulta o hostname da máquina
 * 
 * @return std::string
 */
std::string checkoutHostname() {
   char buffer[HOST_NAME_MAX];
   
   if (gethostname(buffer, HOST_NAME_MAX)) {
      std::cout << "Could not hostname" << std::endl;
   }

   return std::string(buffer);
}

class KeyValueImpl final : public KeyValue::Service {
   std::map<int, std::string> keyValueTable;

   /**
    * @brief Monta a struct que representa a menssagem ServerInfo definida no .proto
    * 
    * @return ServerInfoStruct 
    */
   ServerInfoStruct GetAllServerKeysAndAddress() {
      ServerInfoStruct serverInfo;

      for (auto it = this->keyValueTable.begin(); it != this->keyValueTable.end(); ++it) {
         serverInfo.keys.push_back(it->first);
      }

      serverInfo.serverAddress = checkoutHostname() + ":" + PORT;

      return serverInfo;
   }

   /**
    * @brief Função para inserir valor e chave na tabela do servidor, caso sucesso ajusta o status de resposta para 1 e
    * caso fracasso, para -1.
    * 
    * @return grpc::Status 
    */
   grpc::Status InsertKeyValue(grpc::ServerContext* context, const KeyValuePair* req, InsertionStatus* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function InsertKeyValue()" << std::endl;
#endif

      const auto [newPairIterator, success] = this->keyValueTable.insert({req->key(), req->value()});

      (success) ? res->set_status(1) : res->set_status(-1);

      return grpc::Status::OK;
   };

   /**
    * @brief Consulta uma chave no dicionário do servidor, ajusta o valor de resposta para o valor no dicionário, se não
    * existir a chave, ajusta o valor para ""
    * 
    * @return grpc::Status 
    */
   grpc::Status CheckoutValue(grpc::ServerContext* context, const KeyToBeChecked* req, ValueChecked* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function CheckoutValue()" << std::endl;
#endif

      const auto it = this->keyValueTable.find(req->key());

      (it != this->keyValueTable.end()) ? res->set_value(it->second) : res->set_value("");

      return grpc::Status::OK;
   };

   /**
    * @brief Dada a condição de argumentos passados ARGS, envia para o servidor central suas chaves junto ao seu hostname
    * e porta. Escreve no status de resposta a quantidade de chaves processadas.
    * 
    * @return grpc::Status 
    */
   grpc::Status Activate(grpc::ServerContext* context, const ServiceIdentifier* req, ServiceStatus* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function Activate()" << std::endl;
#endif
      /* Condição para comportamento da parte 1 */
      if (ARGS < 3) {
         res->set_status(0);

         return grpc::Status::OK;
      }

      /* Constrói as mensagens e contexto que serão enviadas para o servidor central */
      ServerInfo _req;
      NumberOfProcessedKeys _res;
      grpc::ClientContext _context;

      ServerInfoStruct serverInfo = GetAllServerKeysAndAddress();

      _req.set_serveraddress(serverInfo.serverAddress);
      
      for (auto it = serverInfo.keys.begin(); it != serverInfo.keys.end(); ++it) {
         _req.add_keys(*it);
      }

      std::string address = req->serviceidentifier();

      /* Cria canal e stub para acessar os processos definidos em map_server.proto e implementados no servidor central */
      auto channel = grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
      std::unique_ptr<KeyMap::Stub> stub_(KeyMap::NewStub(channel));

      /* Chama o procedimento para registrar chaves no servidor central */
      grpc::Status status = stub_->Register(&_context, _req, &_res);

      if (!status.ok()) {
         std::cout << "Error " << status.error_code() << ": " << status.error_message() << std::endl;
      }

      /* Escreve no status de resposta a quantidade de chaves processadas */
      res->set_status(_res.processedkeys());

      return grpc::Status::OK;
   };

   /**
    * @brief Termina a execução do servidor por meio de uma chamada de procedimento feita por um cliente
    * 
    * @return grpc::Status 
    */
   grpc::Status EndExecution(grpc::ServerContext* context, const NoParameterKeyValue* req, EndExecutionStatus* res) {
#if DEBUG
      std::cout << ">> " << context->peer() << " - Called remote function EndExecution()" << std::endl;
#endif

      res->set_status(0);

      /* Faz um ajuste de valor na promisse shutdownServerReq para ativar shutdownServerFuture e desbloquear o código na thread principal */
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