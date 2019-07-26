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
//		std::cout << "read json from " << filename << std::endl;
		boost::property_tree::ptree root;
		boost::property_tree::ptree items;
		boost::property_tree::read_json<boost::property_tree::ptree>(filename, root);

		localAddress = root.get<std::string>("LocalAddress");
		for (auto &&adr : root.get_child("ServerList")) {
			serverList.emplace_back(adr.second.get_value<std::string>());
		}
		serverList.erase(remove(serverList.begin(), serverList.end(), localAddress), serverList.end());
//		std::cout << "localAddress : " << localAddress << std::endl;
//		for (auto i : serverList)
//			std::cout << "Server " << i << std::endl;

		for (auto i : serverList)
			stub_[i] = RAFT::NewStub(grpc::CreateChannel(i, grpc::InsecureChannelCredentials()));

		heartBeatTimeout = std::chrono::milliseconds(50);
		electionTimeout = std::chrono::milliseconds(rand() % 150 + 150);

		StoreStatus(STOP);
		StoreVotedFor(std::string());
		StoreCurrentTerm(0);
	}
	~Server() {
		if (LoadStatus() != STOP)
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

		auto* receive = new SayHelloReceive(this);

		receive->response_reader_ = stub_[channel]->PrepareAsyncSayHello(&receive->context_, request, &ccq_);
		receive->response_reader_->StartCall();
		receive->response_reader_->Finish(&receive->reply_, &receive->status_, &receive->proceed);
	}
	void StartUp() {
		grpc::ServerBuilder builder;
		builder.AddListeningPort(localAddress, grpc::InsecureServerCredentials());
		builder.RegisterService(&service_);
		scq_ = builder.AddCompletionQueue();
		server_ = builder.BuildAndStart();

		std::cout << "Server Start at " << localAddress << " with Election Timeout = " << electionTimeout.count() << std::endl;

		//multi threads here
		//caution: delete data
		data_ = new CallData(&service_, scq_.get(), this);
		new SayHelloCall(data_);
		new RequestVoteCall(data_);
		new AppendEntriesCall(data_);

		thread__ = std::thread(&Server::HandleRpcs, this);
		thread_ = std::thread(&Server::CompleteRpcs, this);

		StoreStatus(FOLLOWER);
		threadMain = std::thread(&Server::Main, this);
	}
	void ShutDown() {
		StoreStatus(STOP);

		std::cout << "Shuting down..." << std::endl;
		ccq_.Shutdown();
		server_->Shutdown();
		scq_->Shutdown();
		delete data_;
	}
private:
	void Main() {
		while (1) {
			switch (LoadStatus()) {
			case FOLLOWER: case CANDIDATE:
				threadTimer = std::thread(&Server::Election, this);
				threadTimer.join();
				break;
			case LEADER:
				threadTimer = std::thread(&Server::HeartBeat, this);
				threadTimer.join();
				break;
			case STOP:
				return;
			}
		}

	}
	void HeartBeat() {
		while (1) {

		}
	}
	void Election() {
		std::chrono::high_resolution_clock::duration elctionExtraTimeout = std::chrono::milliseconds(0);
		lastElectionTimePoint = std::chrono::high_resolution_clock::now();
		while (1) {
			if (LoadStatus() == STOP && LoadStatus() == LEADER)
				break;
			std::cout << localAddress << " Start Election term = " << currentTerm << std::endl;
			lastElectionTimePoint = std::chrono::high_resolution_clock::now();
			StoreStatus(CANDIDATE);
			mutexCurrentTerm.lock();
			++currentTerm;
			mutexCurrentTerm.unlock();
			StoreVoteCnt(1);
			StoreVotedFor(localAddress);
			for (auto channel : serverList) {
				RequestVoteRequest request_;
				request_.set_term(LoadCurrentTerm());
				request_.set_candidateid(localAddress);
				request_.set_lastlogindex(lastLogIndex);
				request_.set_lastlogterm(lastLogTerm);
				auto* receive_ = new RequestVoteReceive(this);
				receive_->response_reader_ = stub_[channel]->PrepareAsyncRequestVote(&receive_->context_, request_, &ccq_);
				receive_->response_reader_->StartCall();
				receive_->response_reader_->Finish(&receive_->reply_, &receive_->status_, &receive_->proceed);
			}
			std::this_thread::sleep_for(electionTimeout + elctionExtraTimeout);
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
		explicit SayHelloReceive(Server* who) : who_(who) {
			proceed = [&](bool ok) { Proceed(ok); };
		}
		std::function<void(bool)> proceed;

		grpc::ClientContext context_;
		HelloReply reply_;
		grpc::Status status_;
		std::unique_ptr<grpc::ClientAsyncResponseReader<HelloReply>> response_reader_;
		Server* who_;

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
		explicit RequestVoteReceive(Server *who) : who_(who){
			proceed = [&](bool ok) { Proceed(ok); };
		}
		std::function<void(bool)> proceed;

		grpc::ClientContext context_;
		RequestVoteReply reply_;
		grpc::Status status_;
		std::unique_ptr<grpc::ClientAsyncResponseReader<RequestVoteReply>> response_reader_;
		Server* who_;

		void Proceed(bool ok) override {
			if (!ok) {
				delete this;
				return;
			}
			if (status_.ok()) {
				std::lock_guard<std::mutex> l1(who_->mutexVoteCnt);
				std::lock_guard<std::mutex> l2(who_->mutexCurrentTerm);
				if (reply_.votegranted()) {
					++who_->voteCnt;
					std::cout << who_->localAddress << " now get " << who_->voteCnt << " votes. ";
					if (who_->voteCnt > who_->serverList.size() / 2) {
						std::cout << who_->localAddress << who_->localAddress << " is Leader" << std::endl;
						who_->StoreStatus(LEADER);
					}
				}
				if (reply_.term() > who_->currentTerm) {
					who_->currentTerm = reply_.term();
					who_->StoreStatus(FOLLOWER);
				}

			}
			else
				std::cout << "RPC faild" << std::endl;
			delete this;
		}
	};

	class AppendEntriesReceive final : public Receive {
	public:
		explicit AppendEntriesReceive(Server *who) : who_(who){
			proceed = [&](bool ok) { Proceed(ok); };
		}
		std::function<void(bool)> proceed;

		grpc::ClientContext context_;
		AppendEntriesReply reply_;
		grpc::Status status_;
		std::unique_ptr<grpc::ClientAsyncResponseReader<AppendEntriesReply>> response_reader_;
		Server* who_;

		void Proceed(bool ok) override {
			if (!ok) {
				delete this;
				return;
			}
			if (status_.ok()) {

			}
			else
				std::cout << "RPC faild" << std::endl;
			delete this;
		}
	};

	struct CallData {
		CallData(RAFT::AsyncService* service, grpc::ServerCompletionQueue* cq, Server* who)
				: service_(service), cq_(cq), who_(who) {}
		RAFT::AsyncService* service_;
		grpc::ServerCompletionQueue* cq_;
		Server* who_;
	};

	class Call {
	public:
		virtual void Proceed(bool ok) = 0;
	};

	class SayHelloCall final : public Call {
	public:
		explicit SayHelloCall(CallData* data)
				: data_(data), responder_(&ctx_), status_(PROCESS), who_(data->who_) {
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
		Server* who_;
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
				: data_(data), responder_(&ctx_), status_(PROCESS), who_(data->who_) {
			proceed = [&](bool ok) { Proceed(ok); };
			data_->service_->RequestRequestVote(&ctx_, &request_, &responder_, data_->cq_, data_->cq_, &proceed);
		}
		void Proceed(bool ok) override {
			switch (status_) {
				case PROCESS: {
					if (!ok) {
						delete this;
						break;
					}
					new RequestVoteCall(data_);
					status_ = FINISH;

					std::lock_guard<std::mutex> l1(who_->mutexVotedFor);
					std::lock_guard<std::mutex> l2(who_->mutexCurrentTerm);

					if (request_.term() < who_->currentTerm)
						reply_.set_votegranted(false);
					else if (request_.term() > who_->currentTerm) {
						who_->StoreStatus(FOLLOWER);
						who_->currentTerm = request_.term();
						who_->votedFor = request_.candidateid();
						reply_.set_votegranted(true);
					}
					else if (!who_->votedFor.empty() && who_->votedFor != request_.candidateid())
						reply_.set_votegranted(false);
					else {
						who_->votedFor = request_.candidateid();
						reply_.set_votegranted(true);
					}
					reply_.set_term(who_->currentTerm);
					responder_.Finish(reply_, grpc::Status::OK, &proceed);
					break;
				}
				case FINISH:
					delete this;
					break;

			}
		}
		std::function<void(bool)> proceed;
	private:
		Server* who_;
		CallData* data_;
		RequestVoteRequest request_;
		RequestVoteReply reply_;
		grpc::ServerContext ctx_;
		grpc::ServerAsyncResponseWriter<RequestVoteReply> responder_;
		enum CallStatus {PROCESS, FINISH};
		CallStatus status_;
	};

	class AppendEntriesCall final : public Call {
	public:
		explicit AppendEntriesCall(CallData* data)
				: data_(data), responder_(&ctx_), status_(PROCESS), who_(data->who_) {
			proceed = [&](bool ok) { Proceed(ok); };
			data_->service_->RequestAppendEntries(&ctx_, &request_, &responder_, data_->cq_, data_->cq_, &proceed);
		}
		void Proceed(bool ok) override {
			switch (status_) {
				case PROCESS:
					if (!ok) {
						delete this;
						break;
					}
					new AppendEntriesCall(data_);
					status_ = FINISH;

					//

					responder_.Finish(reply_, grpc::Status::OK, &proceed);
					break;
				case FINISH:
					delete this;
					break;

			}
		}
		std::function<void(bool)> proceed;
	private:
		Server* who_;
		CallData* data_;
		AppendEntriesRequest request_;
		AppendEntriesReply reply_;
		grpc::ServerContext ctx_;
		grpc::ServerAsyncResponseWriter<AppendEntriesReply> responder_;
		enum CallStatus {PROCESS, FINISH};
		CallStatus status_;
	};

	std::thread thread_, thread__, threadMain, threadTimer;
	CallData* data_;
	std::shared_ptr<grpc::Server> server_;
	std::shared_ptr<grpc::ServerCompletionQueue> scq_;
	RAFT::AsyncService service_;
	std::map<std::string, std::unique_ptr<RAFT::Stub>> stub_;
	grpc::CompletionQueue ccq_;

	enum Status_t {STOP, FOLLOWER, CANDIDATE, LEADER};
	Status_t status;
	std::chrono::high_resolution_clock::duration electionTimeout, heartBeatTimeout;
	std::chrono::high_resolution_clock::time_point lastElectionTimePoint;
	std::string localAddress;
	std::vector<std::string> serverList;
	int currentTerm, lastApplied, lastLogIndex, lastLogTerm, voteCnt;
	std::string votedFor;

	std::mutex mutexStatus, mutexVoteCnt, mutexCurrentTerm, mutexVotedFor;
	Status_t LoadStatus() {
		std::lock_guard<std::mutex> l(mutexStatus);
		return status;
	}
	void StoreStatus(const Status_t& status_) {
		std::lock_guard<std::mutex> l(mutexStatus);
		status = status_;
	}
	int LoadVoteCnt() {
		std::lock_guard<std::mutex> l(mutexVoteCnt);
		return voteCnt;
	}
	void StoreVoteCnt(int voteCnt_) {
		std::lock_guard<std::mutex> l(mutexVoteCnt);
		voteCnt = voteCnt_;
	}
	int LoadCurrentTerm() {
		std::lock_guard<std::mutex> l(mutexCurrentTerm);
		return currentTerm;
	}
	void StoreCurrentTerm(int currentTerm_) {
		std::lock_guard<std::mutex> l(mutexCurrentTerm);
		currentTerm = currentTerm_;
	}
	std::string LoadVotedFor() {
		std::lock_guard<std::mutex> l(mutexVotedFor);
		return votedFor;
	}
	void StoreVotedFor(const std::string& votedFor_) {
		std::lock_guard<std::mutex> l(mutexVotedFor);
		votedFor = votedFor_;
	}
};

int main() {
	Server s1("s1.json");
	Server s2("s2.json");
	Server s3("s3.json");
	s1.StartUp();
	s2.StartUp();
	s3.StartUp();
	while (1);
//	s1.ShutDown();
//	s2.ShutDown();
//	s3.ShutDown();
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	return 0;
}