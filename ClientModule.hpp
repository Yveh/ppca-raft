#include <memory>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <map>

#include <grpcpp/grpcpp.h>
#include "protos/raft.grpc.pb.h"

class ClientModule {
public:
	explicit ClientModule() = default;
	~ClientModule() = default;
	void ShutDown() {
		cq_.Shutdown();
	}
	void Init(const std::vector<std::string> &channel) {
		for (auto i : channel)
			stub_[i] = RAFT::NewStub(grpc::CreateChannel(i, grpc::InsecureChannelCredentials()));
	}
	void SayHello(const std::string& channel, const std::string& user) {
		HelloRequest request;
		request.set_name(user);

		auto* call = new SayHelloCall();

		call->response_reader = stub_[channel]->PrepareAsyncSayHello(&call->context, request, &cq_);
		call->response_reader->StartCall();
		call->response_reader->Finish(&call->reply, &call->status, (void*)call);
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

		if (call->status.ok())
			std::cout << "received: " << call->reply.message() << std::endl;
		else
			std::cout << "RPC faild" << std::endl;
	}
	void CompleteRpc() {
		void* tag;
		bool ok = false;
		while (cq_.Next(&tag, &ok)) {
//			GPR_ASSERT(ok);
			if (!ok)
				break;
			auto* call = static_cast<SayHelloCall*>(tag);
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


	std::map<std::string, std::unique_ptr<RAFT::Stub>> stub_;
	grpc::CompletionQueue cq_;
};