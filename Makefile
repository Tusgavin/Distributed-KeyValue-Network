LDFLAGS += -L/usr/local/lib `pkg-config --libs grpc++ grpc` \
           -lgrpc++_reflection \
           -lprotobuf -lpthread -ldl

flag = 1

all: compile_protos compile_serv_pares compile_serv_cen compile_cli_pares compile_cli_cen

compile_serv_pares:
	g++ -std=c++17 ./build/gen/key_value.grpc.pb.cc ./build/gen/key_value.pb.cc ./build/gen/map_server.grpc.pb.cc ./build/gen/map_server.pb.cc server/par/main.cc $^ $(LDFLAGS) -o svc_par

compile_serv_cen:
	g++ -std=c++17 ./build/gen/map_server.grpc.pb.cc ./build/gen/map_server.pb.cc server/cen/main.cc $^ $(LDFLAGS) -o svc_cen

compile_cli_pares:
	g++ -std=c++17 ./build/gen/key_value.grpc.pb.cc ./build/gen/key_value.pb.cc client/par/main.cc $^ $(LDFLAGS) -o cln_par

compile_cli_cen:
	g++ -std=c++17 ./build/gen/key_value.grpc.pb.cc ./build/gen/key_value.pb.cc ./build/gen/map_server.grpc.pb.cc ./build/gen/map_server.pb.cc client/cen/main.cc $^ $(LDFLAGS) -o cln_cen

compile_protos:
	if [ -d ./build ]; then rm -rf ./build; fi;
	mkdir -p build/gen
	protoc -I ./protos --grpc_out=./build/gen --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ./protos/key_value.proto
	protoc --proto_path=protos --cpp_out=build/gen protos/key_value.proto
	protoc -I ./protos --grpc_out=./build/gen --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ./protos/map_server.proto
	protoc --proto_path=protos --cpp_out=build/gen protos/map_server.proto

run_cli_pares:
	compile_protos
	compile_cli_pares
	clear
	sudo ./cln_par $(arg)

run_serv_pares_1:
	compile_protos
	make compile_serv_pares
	clear
	sudo ./svc_par $(arg)

run_serv_pares_2:
	compile_protos
	make compile_serv_pares
	clear
	sudo ./svc_par $(arg) $(flag)

run_serv_central:
	compile_protos
	make compile_serv_cen
	clear
	sudo ./svc_cen $(arg)

run_cli_central:
	compile_protos
	make compile_cli_cen
	clear	
	sudo ./cln_cen $(arg)

clean:
	rm -rf ./build
	rm -f cln_cen
	rm -f cln_par
	rm -f svc_cen
	rm -f svc_par