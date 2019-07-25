#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <vector>
#include <map>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <grpcpp/grpcpp.h>
#include <protos/raft.pb.h>
#include "protos/raft.grpc.pb.h"

class Server {
public:
	Server(const std::string& filename) {
		std::cout << "read json from " << filename << std::endl;
		boost::property_tree::ptree root;
		boost::property_tree::ptree items;
		boost::property_tree::read_json<boost::property_tree::ptree>(filename, root);

		localAddress = root.get<std::string>("LocalAddress");

		for (auto &&adr : root.get_child("ServerList")) {
			serverList.emplace_back(adr.second.get_value<std::string>());
		}

		serverList.erase(remove(serverList.begin(), serverList.end(), localAddress), serverList.end());
		std::cout << "localAddress : " << localAddress << std::endl;
		for (auto i : serverList)
			std::cout << "Server " << i << std::endl;

		for (auto i : serverList)
			stub_[i] = RAFT::NewStub(grpc::CreateChannel(i, grpc::InsecureChannelCredentials()));

		receivedHeartBeat.store(false);
		status.store(STOP);

		heartBeatTimeout = std::chrono::milliseconds(50);
		electionTimeout = std::chrono::milliseconds(rand() % 150 + 150);
	}
	~Server() {
		if (status.load() != STOP)
			ShutDown();
		if (thread_.joinable())
			thread_.join();
		if (thread__.joinable())
			thread__.join();
		if (threadMain.joinable())
			threadMain.join();
	}
	void SayHello(const std::string& channel, const std::string& user) {
		HelloRequest request;
		request.set_name(user);

		auto* receive = new SayHelloReceive();

		receive->response_reader_ = stub_[channel]->PrepareAsyncSayHello(&receive->context_, request, &ccq_);
		receive->response_reader_->StartCall();
		receive->response_reader_->Finish(&receive->reply_, &receive->status_, &receive->proceed);
	}
	void StartUp() {
		std::cout << "Server Start" << std::endl;

		grpc::ServerBuilder builder;
		builder.AddListeningPort(localAddress, grpc::InsecureServerCredentials());
		builder.RegisterService(&service_);
		scq_ = builder.AddCompletionQueue();
		server_ = builder.BuildAndStart();
		std::cout << "Server listening on " << localAddress << std::endl;
		//multi threads here
		//caution: delete data
		data_ = new CallData(&service_, scq_.get());
		new SayHelloCall(data_);
		new RequestVoteCall(data_);

		thread__ = std::thread(&Server::HandleRpcs, this);
		thread_ = std::thread(&Server::CompleteRpcs, this);

		status.store(FOLLOWER);
		threadMain = std::thread(&Server::Main, this);
	}
	void ShutDown() {
		status.store(STOP);

		std::cout << "Shuting down..." << std::endl;
		ccq_.Shutdown();
		server_->Shutdown();
		scq_->Shutdown();
		delete data_;
	}
private:
	void Main() {
		threadStartElection = std::thread(&Server::StartElection, this);




		threadStartElection.join();
	}
	void HeartBeat() {

	}
	void StartElection() {
		std::chrono::high_resolution_clock::duration elctionExtraTimeout = std::chrono::milliseconds(0);
		lastElectionTimePoint = std::chrono::high_resolution_clock::now();
		while (1) {
			std::this_thread::sleep_for(electionTimeout + elctionExtraTimeout);
			if (status.load() == STOP)
				break;
			if (receivedHeartBeat.load()) {
				elctionExtraTimeout = std::chrono::high_resolution_clock::now() - lastElectionTimePoint - electionTimeout;
				receivedHeartBeat.store(false);
			}
			else {
				std::cout << localAddress << " send !!" << std::endl;

//				RequestVoteRequest request_;
//				request_.set_term(currentTerm);
//				request_.set_candidateid(localAddress);
//				request_.set_lastlogindex(lastLogIndex);
//				request_.set_lastlogterm(lastLogTerm);
//				auto* receive_ = new RequestVoteReceive();
//
//				for (auto channel : serverList) {
//					receive_->response_reader_ = stub_[channel]->PrepareAsyncRequestVote(&receive_->context_, request_, &ccq_);
//					receive_->response_reader_->StartCall();
//					receive_->response_reader_->Finish(&receive_->reply_, &receive_->status_, &receive_->proceed);
//				}
			}
		}
	}
	void HandleRpcs() {
		void* tag;
		bool ok;
		while (scq_->Next(&tag, &ok)) {
			auto* proceed = static_cast<std::function<void(bool)>*>(tag);
			(*proceed)(ok);
		}
	}
	void CompleteRpcs() {
		void* tag;
		bool ok = false;
		while (ccq_.Next(&tag, &ok)) {
			auto* proceed = static_cast<std::function<void(bool)>*>(tag);
			(*proceed)(ok);
		}
	}
	class Receive {
	public:
		virtual void Proceed(bool ok) = 0;
	};

	class SayHelloReceive final : public Receive {
	public:
		explicit SayHelloReceive() {
			proceed = [&](bool ok) { Proceed(ok); };
		}
		std::function<void(bool)> proceed;

		grpc::ClientContext context_;
		HelloReply reply_;
		grpc::Status status_;
		std::unique_ptr<grpc::ClientAsyncResponseReader<HelloReply>> response_reader_;

		void Proceed(bool ok) override {

			if (!ok) {
				delete this;
				return;
			}
			if (status_.ok())
				std::cout << "received: " << reply_.message() << std::endl;
			else
				std::cout << "RPC faild" << std::endl;
			delete this;
		}
	};

	class RequestVoteReceive final : public Receive {
	public:
		explicit RequestVoteReceive() {
			proceed = [&](bool ok) { Proceed(ok); };
		}
		std::function<void(bool)> proceed;

		grpc::ClientContext context_;
		RequestVoteReply reply_;
		grpc::Status status_;
		std::unique_ptr<grpc::ClientAsyncResponseReader<RequestVoteReply>> response_reader_;

		void Proceed(bool ok) override {
			if (!ok) {
				delete this;
				return;
			}
			if (status_.ok())
				std::cout << "received: votegranted = " << reply_.votegranted() << std::endl;
			else
				std::cout << "RPC faild" << std::endl;
			delete this;
		}
	};

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

	class RequestVoteCall final : public Call {
	public:
		explicit RequestVoteCall(CallData* data)
				: data_(data), responder_(&ctx_), status_(PROCESS) {
			proceed = [&](bool ok) { Proceed(ok); };
			data_->service_->RequestRequestVote(&ctx_, &request_, &responder_, data_->cq_, data_->cq_, &proceed);
		}
		void Proceed(bool ok) override {
			switch (status_) {
				case PROCESS:
					if (!ok) {
						delete this;
						break;
					}

					new RequestVoteCall(data_);
					status_ = FINISH;

					reply_.set_votegranted(true);
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
		RequestVoteRequest request_;
		RequestVoteReply reply_;
		grpc::ServerContext ctx_;
		grpc::ServerAsyncResponseWriter<RequestVoteReply> responder_;
		enum CallStatus {PROCESS, FINISH};
		CallStatus status_;
	};


	std::thread thread_, thread__, threadMain, threadHeartBeat, threadStartElection;
	CallData* data_;
	std::shared_ptr<grpc::Server> server_;
	std::shared_ptr<grpc::ServerCompletionQueue> scq_;
	RAFT::AsyncService service_;
	std::map<std::string, std::unique_ptr<RAFT::Stub>> stub_;
	grpc::CompletionQueue ccq_;

	enum Status_t {STOP, FOLLOWER, CANDIDATE, LEADER};
	std::atomic<Status_t> status;
	std::chrono::high_resolution_clock::duration electionTimeout, heartBeatTimeout;
	std::chrono::high_resolution_clock::time_point lastElectionTimePoint;
	std::string localAddress;
	std::vector<std::string> serverList;
	int currentTerm, lastApplied, lastLogIndex, lastLogTerm;
	std::string votedFor;

	std::atomic<bool> receivedHeartBeat;
};

int main() {
	Server s1("s1.json");
	Server s2("s2.json");
	Server s3("s3.json");
	s1.StartUp();
	s2.StartUp();
	s3.StartUp();
//	std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	while (1);
	s1.ShutDown();
//	s2.ShutDown();
//	s3.ShutDown();
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	return 0;
}