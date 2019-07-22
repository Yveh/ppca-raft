#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "protos/raft.grpc.pb.h"


class RaftServer {
public:
	~RaftServer() {
		server_->Shutdown();
		cq_->Shutdown();
	}
	void run(std::string ip, std::string port) {
		std::string server_address = ip + ':' + port;
		grpc::ServerBuilder builder;
		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
		builder.RegisterService(&service_);
		cq_ = builder.AddCompletionQueue();
		server_ = builder.BuildAndStart();
		std::cout << "Server listening on " << server_address << std::endl;

		CallData data = CallData(&service_, cq_.get());
		new SayHelloCall(&data);
		//

		HandleRpcs();
	}
	void HandleRpcs() {
		void* tag;
		bool ok;
		while (cq_->Next(&tag, &ok)) {
			GPR_ASSERT(ok);
			auto proceed = static_cast<std::function<void()>*>(tag);
			(*proceed)();
		}
	}
private:
	struct CallData {
		CallData(RAFT::AsyncService* service, grpc::ServerCompletionQueue* cq)
				: service_(service), cq_(cq) {}
		RAFT::AsyncService* service_;
		grpc::ServerCompletionQueue* cq_;
	};

	class Call {
	public:
		virtual void Proceed() = 0;
	};

	class SayHelloCall final : public Call {
	public:
		explicit SayHelloCall(CallData* data)
				: data_(data), responder_(&ctx_), status_(PROCESS) {
			proceed = [&]() {Proceed(); };
			data_->service_->RequestSayHello(&ctx_, &request_, &responder_, data_->cq_, data_->cq_, &proceed);
		}
		void Proceed() {
			switch (status_) {
				case PROCESS:
					new SayHelloCall(data_);
					status_ = FINISH;
					reply_.set_message("Hello " + request_.name());
					responder_.Finish(reply_, grpc::Status::OK, &proceed);
					break;
				case FINISH:
					delete this;
					break;

			}
		}
		std::function<void()> proceed;
	private:
		CallData* data_;
		HelloRequest request_;
		HelloReply reply_;
		grpc::ServerContext ctx_;
		grpc::ServerAsyncResponseWriter<HelloReply> responder_;
		enum CallStatus {PROCESS, FINISH};
		CallStatus status_;
	};

	std::shared_ptr<grpc::Server> server_;
	std::shared_ptr<grpc::ServerCompletionQueue> cq_;
	RAFT::AsyncService service_;
};

int main() {
	RaftServer Raft;
	Raft.run("localhost", "50001");
	return 0;
}

