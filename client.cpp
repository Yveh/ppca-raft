#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "protos/raft.grpc.pb.h"

class RaftClient {
public:
	explicit RaftClient(std::shared_ptr<grpc::Channel> channel)
				: stub_(RAFT::NewStub(channel)) {}
	void SayHello(const std::string& user) {
		HelloRequest request;
		request.set_name(user);

		SayHelloCall* call = new SayHelloCall();

		call->response_reader = stub_->PrepareAsyncSayHello(&call->context, request, &cq_);
		call->response_reader->StartCall();
		call->response_reader->Finish(&call->reply, &call->status, (void*)call);
	}
	void CompleteRpc() {
		void* tag;
		bool ok = false;
		while (cq_.Next(&tag, &ok)) {
			SayHelloCall* call = static_cast<SayHelloCall*>(tag);
			GPR_ASSERT(ok);
			if (call->status.ok())
				std::cout << "received: " << call->reply.message() << std::endl;
			else
				std::cout << "RPC faild" << std::endl;
			delete call;
		}
	}
private:
	struct SayHelloCall {
		grpc::ClientContext context;
		HelloReply reply;
		grpc::Status status;
		std::unique_ptr<grpc::ClientAsyncResponseReader<HelloReply>> response_reader;
	};


	std::unique_ptr<RAFT::Stub> stub_;
	grpc::CompletionQueue cq_;
};
int main() {
	RaftClient raft(grpc::CreateChannel("localhost:50001", grpc::InsecureChannelCredentials()));

	std::thread thread_ = std::thread(&RaftClient::CompleteRpc, &raft);
	for (int i = 0; i < 100000; ++i) {
		std::string user("world" + std::to_string(i));
		raft.SayHello(user);
		std::cout << "send " << i << std::endl;
	}
	std::cout << "Press any key to continue" << std::endl;
	thread_.join();
}

