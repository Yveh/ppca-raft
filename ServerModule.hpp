#include <memory>
#include <iostream>
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>
#include "protos/raft.grpc.pb.h"

class ServerModule {
public:
	explicit ServerModule() = default;
	~ServerModule() = default;
	void ShutDown() {
		server_->Shutdown();
		cq_->Shutdown();
	}
	void Run(const std::string &server_address) {
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
			auto* proceed = static_cast<std::function<void(bool)>*>(tag);
			(*proceed)(ok);
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
		virtual void Proceed(bool ok) = 0;
	};

	class SayHelloCall final : public Call {
	public:
		explicit SayHelloCall(CallData* data)
				: data_(data), responder_(&ctx_), status_(PROCESS) {
			proceed = [&](bool ok) { Proceed(ok); };
			data_->service_->RequestSayHello(&ctx_, &request_, &responder_, data_->cq_, data_->cq_, &proceed);
		}
		void Proceed(bool ok) override {
			switch (status_) {
				case PROCESS:
					if (!ok) {
						delete this;
						break;
					}

					new SayHelloCall(data_);
					status_ = FINISH;
					reply_.set_message("Hello " + request_.name());
					std::this_thread::sleep_for(std::chrono::milliseconds(50));
					responder_.Finish(reply_, grpc::Status::OK, &proceed);
					break;
				case FINISH:
					delete this;
					break;

			}
		}
		std::function<void(bool)> proceed;
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