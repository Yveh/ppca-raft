#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <vector>
#include <map>
#include <unistd.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <grpcpp/grpcpp.h>
#include <protos/raft.pb.h>
#include <raft.pb.h>
#include "protos/raft.grpc.pb.h"

class Server {
public:
	Server(const std::string& filename) {
		srand(time(NULL) ^ getpid());
		boost::property_tree::ptree root;
		boost::property_tree::read_json<boost::property_tree::ptree>("configs/" + filename, root);

		localAddress = root.get<std::string>("LocalAddress");
		for (auto &&adr : root.get_child("ServerList"))
			serverList.emplace_back(adr.second.get_value<std::string>());
		serverList.erase(remove(serverList.begin(), serverList.end(), localAddress), serverList.end());

//		fp.open("logs/" + filename);
//		cout << fp.opend
//		fp << "erer" << std::endl;

		for (auto i : serverList) {
			stub_[i] = RAFT::NewStub(grpc::CreateChannel(i, grpc::InsecureChannelCredentials()));
			nextIndex[i] = 1;
			matchIndex[i] = 0;
		}
		votedFor = "";
		currentTerm = 0;
		lastApplied = 0;
		commitIndex = 0;
		log.emplace_back();
		status = STOP;
		heartBeatTimeout = std::chrono::milliseconds(50);
		electionTimeout = std::chrono::milliseconds(rand() % 500 + 500);
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
	void StartUp() {
		grpc::ServerBuilder builder;
		builder.AddListeningPort(localAddress, grpc::InsecureServerCredentials());
		builder.RegisterService(&service_);
		scq_ = builder.AddCompletionQueue();
		server_ = builder.BuildAndStart();

		status = FOLLOWER;

		std::cout << "Server Start at " << localAddress << std::endl;


		//multi threads here
		//caution: delete data
		data_ = new CallData(&service_, scq_.get(), this);
		new PutCall(data_);
		new GetCall(data_);
		new RequestVoteCall(data_);
		new AppendEntriesCall(data_);
		thread__ = std::thread(&Server::HandleRpcs, this);
		thread_ = std::thread(&Server::CompleteRpcs, this);

		threadMain = std::thread(&Server::Main, this);
	}
	void ShutDown() {
		StoreStatus(STOP);

		std::cout << "Shuting down..." << std::endl;
		ccq_.Shutdown();
		server_->Shutdown();
		scq_->Shutdown();
		delete data_;
		fp.close();
	}
private:
	void Main() {
		threadApply = std::thread(&Server::Apply, this);
		threadElection = std::thread(&Server::Election, this);
		threadHeartBeat = std::thread(&Server::HeartBeat, this);
		threadApply.join();
		threadElection.join();
		threadHeartBeat.join();
	}
	void Apply() {
		while (1) {
			if (LoadStatus() == STOP)
				break;
			std::unique_lock<std::mutex> l0(mutexCommitIndex);
			condApply.wait(l0, [this]{return LoadStatus() == STOP || lastApplied < commitIndex; });
			if (LoadStatus() == STOP)
				break;
			int tmp = commitIndex;
			l0.unlock();

			while (lastApplied < tmp) {
				mutexLog.lock();
				mutexDataBase.lock();
				dataBase[std::get<1>(log[lastApplied])] = std::get<2>(log[lastApplied]);
				mutexLog.unlock();
				mutexDataBase.unlock();
				mutexLastApplied.lock();
				++lastApplied;
				condGet.notify_all();
				mutexLastApplied.unlock();
			}
		}
	}
	void HeartBeat() {
		while (1) {
			std::unique_lock<std::mutex> l0(mutexStatus);
			condHeartBeat.wait(l0, [this]{return status == LEADER || status == STOP;});
			l0.unlock();
			if (LoadStatus() == STOP)
				break;
			while (1) {
				mutexMatchIndex.lock();
				mutexCommitIndex.lock();
				int tmpCnt = 0;
				for (auto channel : serverList) {
					if (matchIndex[channel] >= commitIndex + 1)
						++tmpCnt;
				}
				if (tmpCnt >= serverList.size() / 2) {
					++commitIndex;
					condApply.notify_all();
				}
				mutexMatchIndex.unlock();
				mutexCommitIndex.unlock();

				for (auto channel : serverList) {
					AppendEntriesRequest request_;
					request_.set_term(LoadCurrentTerm());
					request_.set_leaderid(localAddress);

					mutexNextIndex.lock();
					mutexLog.lock();
					request_.set_prevlogterm(std::get<0>(log[nextIndex[channel] - 1]));
					mutexLog.unlock();
					request_.set_prevlogindex(nextIndex[channel] - 1);
					mutexNextIndex.unlock();
					request_.set_leadercommit(LoadCommitIndex());

					mutexNextIndex.lock();
					if (nextIndex[channel] <= log.size() - 1) {
						Entry *entry = request_.add_entries();
						entry->set_term(std::get<0>(log[nextIndex[channel]]));
						mutexLog.lock();
						entry->set_key(std::get<1>(log[nextIndex[channel]]));
						entry->set_value(std::get<2>(log[nextIndex[channel]]));
						mutexLog.unlock();
					}
					mutexNextIndex.unlock();

					auto *receive_ = new AppendEntriesReceive(this);
					receive_->context_.set_deadline(std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(20));
					receive_->response_reader_ = stub_[channel]->PrepareAsyncAppendEntries(&receive_->context_,
																						   request_, &ccq_);
					receive_->response_reader_->StartCall();
					receive_->response_reader_->Finish(&receive_->reply_, &receive_->status_, &receive_->proceed);
				}
				l0.lock();
				condHeartBeat.wait_for(l0, heartBeatTimeout, [this]{return status != LEADER;});
				l0.unlock();
				if (LoadStatus() != LEADER)
					break;
			}
		}
	}
	void Election() {
		while (1) {
			std::unique_lock<std::mutex> l0(mutexStatus);
			condElection.wait(l0, [this]{return status == FOLLOWER || status == CANDIDATE || status == STOP;});
			l0.unlock();
			if (LoadStatus() == STOP)
				break;
			std::chrono::high_resolution_clock::duration electionExtraTimeout = std::chrono::milliseconds(0);
			StoreLastElectionTimePoint(std::chrono::high_resolution_clock::now());
			while (1) {
				auto qwq = std::chrono::high_resolution_clock::now();

				StoreLastElectionTimePoint(std::chrono::high_resolution_clock::now());
				StoreStatus(CANDIDATE);
				StoreVotedFor(localAddress);

				mutexVoteCnt.lock();
				voteCnt = 1;
				mutexVoteCnt.unlock();

				mutexCurrentTerm.lock();
				++currentTerm;
				mutexCurrentTerm.unlock();

				std::cout << localAddress << " start election term = " << LoadCurrentTerm() << " timeout = " << electionTimeout.count() << std::endl;

				for (auto channel : serverList) {
					RequestVoteRequest request_;
					request_.set_term(LoadCurrentTerm());
					request_.set_candidateid(localAddress);
					request_.set_lastlogindex(0);
					request_.set_lastlogterm(0);
					auto* receive_ = new RequestVoteReceive(this);
					receive_->context_.set_deadline(std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(20));
					receive_->response_reader_ = stub_[channel]->PrepareAsyncRequestVote(&receive_->context_, request_, &ccq_);
					receive_->response_reader_->StartCall();
					receive_->response_reader_->Finish(&receive_->reply_, &receive_->status_, &receive_->proceed);
				}
				electionTimeout = std::chrono::milliseconds(rand() % 500 + 500);
				electionExtraTimeout = LoadLastElectionTimePoint() - std::chrono::high_resolution_clock::now();
				assert(electionExtraTimeout < std::chrono::milliseconds(0));
				while (electionTimeout + electionExtraTimeout > std::chrono::milliseconds(0)) {
					std::unique_lock<std::mutex> l1(mutexStatus);
					condElection.wait_for(l1, electionTimeout + electionExtraTimeout, [this]{return status != FOLLOWER && status != CANDIDATE;});
					l1.unlock();
					if (LoadStatus() != CANDIDATE && LoadStatus() != FOLLOWER)
						break;
					electionTimeout = std::chrono::milliseconds(rand() % 500 + 500);
					electionExtraTimeout = electionTimeout - (std::chrono::high_resolution_clock::now() - LoadLastElectionTimePoint());
				}
				if (LoadStatus() != CANDIDATE && LoadStatus() != FOLLOWER)
					break;
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
				std::cout << who_->localAddress << " received vote whose term = " << reply_.term() << std::endl;
				if (reply_.term() > who_->LoadCurrentTerm()) {
					who_->StoreStatus(FOLLOWER);
					who_->StoreCurrentTerm(reply_.term());
					who_->StoreVotedFor("");
				}
				if (who_->LoadStatus() != CANDIDATE) {
					delete this;
					return;
				}
				if (reply_.votegranted()) {
					who_->mutexVoteCnt.lock();
					++who_->voteCnt;
					if (who_->voteCnt > who_->serverList.size() / 2) {
						std::cout << who_->localAddress << " is Leader term = " << who_->LoadCurrentTerm() << std::endl;
						who_->StoreStatus(LEADER);
					}
					who_->mutexVoteCnt.unlock();
				}
			}
//			else
//				std::cout << "RPC faild" << std::endl;
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
				if (reply_.term() > who_->LoadCurrentTerm()) {
					who_->StoreStatus(FOLLOWER);
					who_->StoreCurrentTerm(reply_.term());
					who_->StoreVotedFor("");
				}
				if (who_->LoadStatus() != LEADER) {
					delete this;
					return;
				}
				who_->mutexNextIndex.lock();
				if (reply_.success()) {
					who_->nextIndex[reply_.receiver()] += reply_.cnt();
					who_->mutexMatchIndex.lock();
					who_->matchIndex[reply_.receiver()] = who_->nextIndex[reply_.receiver()] - 1;
					who_->mutexMatchIndex.unlock();
				}
				else {
					--who_->nextIndex[reply_.receiver()];
				}
				who_->mutexNextIndex.unlock();
			}
//			else
//				std::cout << "RPC faild" << std::endl;
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

	class PutCall final : public Call {
	public:
		explicit PutCall(CallData* data)
				: data_(data), responder_(&ctx_), status_(PROCESS), who_(data->who_) {
			proceed = [&](bool ok) { Proceed(ok); };
			data_->service_->RequestPut(&ctx_, &request_, &responder_, data_->cq_, data_->cq_, &proceed);
			writeIndex = -1;
		}
		void Proceed(bool ok) override {
			switch (status_) {
				case PROCESS:
					if (!ok) {
						delete this;
						break;
					}
					new PutCall(data_);
					status_ = FINISH;
					reply_.set_success(true);

					if (who_->LoadStatus() != LEADER) {
						reply_.set_success(false);
					}
					else {
						who_->mutexLog.lock();
						writeIndex = who_->log.size();
						who_->log.emplace_back(who_->LoadCurrentTerm(), request_.key(), request_.value());
						who_->mutexLog.unlock();
						std::unique_lock<std::mutex> l(who_->mutexCommitIndex);
						who_->condApply.wait(l, [this] {return who_->commitIndex >= writeIndex; });
						l.unlock();
						reply_.set_success(true);
					}
					responder_.Finish(reply_, grpc::Status::OK, &proceed);
					break;
				case FINISH:
					delete this;
					break;
			}
		}
		std::function<void(bool)> proceed;
	private:
		int writeIndex;
		Server* who_;
		CallData* data_;
		PutRequest request_;
		PutReply reply_;
		grpc::ServerContext ctx_;
		grpc::ServerAsyncResponseWriter<PutReply> responder_;
		enum CallStatus {PROCESS, FINISH};
		CallStatus status_;
	};

	class GetCall final : public Call {
	public:
		explicit GetCall(CallData* data)
				: data_(data), responder_(&ctx_), status_(PROCESS), who_(data->who_) {
			proceed = [&](bool ok) { Proceed(ok); };
			data_->service_->RequestGet(&ctx_, &request_, &responder_, data_->cq_, data_->cq_, &proceed);
			readIndex = who_->LoadCommitIndex();
		}
		void Proceed(bool ok) override {
			switch (status_) {
				case PROCESS:
					if (!ok) {
						delete this;
						break;
					}
					new GetCall(data_);
					status_ = FINISH;
					if (who_->LoadStatus() != LEADER) {

						reply_.set_success(false);

					}
					else {
						std::unique_lock<std::mutex> l(who_->mutexLastApplied);
						who_->condGet.wait(l, [this]{return who_->lastApplied >= readIndex; });
						l.unlock();
						who_->mutexDataBase.lock();
						if (who_->dataBase.count(request_.key()))
							reply_.set_value(who_->dataBase[request_.key()]);
						who_->mutexDataBase.unlock();
						reply_.set_success(true);
					}
					responder_.Finish(reply_, grpc::Status::OK, &proceed);

					break;
				case FINISH:
					delete this;
					break;
			}
		}
		std::function<void(bool)> proceed;
	private:
		int readIndex;
		Server* who_;
		CallData* data_;
		GetRequest request_;
		GetReply reply_;
		grpc::ServerContext ctx_;
		grpc::ServerAsyncResponseWriter<GetReply> responder_;
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

					if (request_.term() < who_->LoadCurrentTerm())
						reply_.set_votegranted(false);
					else {
						if (request_.term() > who_->LoadCurrentTerm()) {
							who_->StoreStatus(FOLLOWER);
							who_->StoreCurrentTerm(request_.term());
							who_->StoreVotedFor("");
						}
						if (who_->LoadVotedFor() == "" || who_->LoadVotedFor() == request_.candidateid()) {
							who_->StoreVotedFor(request_.candidateid());
							reply_.set_votegranted(true);
						}
						else {
							reply_.set_votegranted(false);
						}
					}
					reply_.set_term(who_->LoadCurrentTerm());
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
				case PROCESS: {
					if (!ok) {
						delete this;
						break;
					}
					new AppendEntriesCall(data_);
					status_ = FINISH;

					reply_.set_receiver(who_->localAddress);
					reply_.set_cnt(0);
					if (request_.term() < who_->LoadCurrentTerm()) {
						reply_.set_success(false);
						reply_.set_term(who_->LoadCurrentTerm());
					}
					else {
						if (request_.term() > who_->LoadCurrentTerm()) {
							who_->StoreStatus(FOLLOWER);
							who_->StoreCurrentTerm(request_.term());
							who_->StoreVotedFor("");
						}
						reply_.set_term(who_->LoadCurrentTerm());
						who_->StoreLastElectionTimePoint(std::chrono::high_resolution_clock::now());
						if (request_.entries_size()) {
							who_->mutexLog.lock();
							if (who_->log.size() - 1 < request_.prevlogindex() || std::get<0>(who_->log[request_.prevlogindex()]) != request_.prevlogterm()) {
								reply_.set_success(false);
							}
							else {
								while (who_->log.size() - 1 > request_.prevlogindex())
									who_->log.pop_back();
								for (int i = 0; i < request_.entries_size(); ++i)
									who_->log.emplace_back(request_.entries(i).term(), request_.entries(i).key(), request_.entries(i).value());
								reply_.set_cnt(request_.entries().size());
								reply_.set_success(true);
							}
							who_->mutexLog.unlock();
						}
						else {
							reply_.set_success(true);
						}
						if (request_.leadercommit() > who_->LoadCommitIndex()) {
							who_->StoreCommitIndex(std::min(int(request_.leadercommit()), int(who_->log.size() - 1)));
							who_->condApply.notify_all();
						}
					}

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
		AppendEntriesRequest request_;
		AppendEntriesReply reply_;
		grpc::ServerContext ctx_;
		grpc::ServerAsyncResponseWriter<AppendEntriesReply> responder_;
		enum CallStatus {PROCESS, FINISH};
		CallStatus status_;
	};

	//grpc variable
	std::thread thread_, thread__, threadMain, threadHeartBeat, threadElection, threadApply;
	CallData* data_;
	std::shared_ptr<grpc::Server> server_;
	std::shared_ptr<grpc::ServerCompletionQueue> scq_;
	RAFT::AsyncService service_;
	std::map<std::string, std::unique_ptr<RAFT::Stub>> stub_;
	grpc::CompletionQueue ccq_;

	std::vector<std::string> serverList;
	std::fstream fp;
	std::string localAddress;
	std::chrono::high_resolution_clock::duration electionTimeout, heartBeatTimeout;
	std::map<std::string, std::string> dataBase;
	int lastApplied;

	//need lock
	enum Status_t {STOP, FOLLOWER, CANDIDATE, LEADER};
	Status_t status;
	std::chrono::high_resolution_clock::time_point lastElectionTimePoint;
	int currentTerm, voteCnt, commitIndex;
	std::map<std::string, int> nextIndex, matchIndex;
	std::vector<std::tuple<int, std::string, std::string>> log;
	std::string votedFor;

	std::mutex mutexStatus, mutexVoteCnt, mutexCurrentTerm, mutexVotedFor, mutexLastElectionTimePoint, mutexLog, mutexNextIndex, mutexCommitIndex, mutexMatchIndex, mutexDataBase, mutexLastApplied;
	std::condition_variable condHeartBeat, condElection, condApply, condGet;

	//lastElectionTimePoint
	std::chrono::high_resolution_clock::time_point LoadLastElectionTimePoint() {
		std::lock_guard<std::mutex> l(mutexLastElectionTimePoint);
		return lastElectionTimePoint;
	}
	void StoreLastElectionTimePoint(std::chrono::high_resolution_clock::time_point lastElectionTimePoint_) {
		std::lock_guard<std::mutex> l(mutexLastElectionTimePoint);
		lastElectionTimePoint = lastElectionTimePoint_;
	}
	//status
	Status_t LoadStatus() {
		std::lock_guard<std::mutex> l(mutexStatus);
		return status;
	}
	void StoreStatus(const Status_t& status_) {
		std::lock_guard<std::mutex> l(mutexStatus);
		std::cout << localAddress << " from " << status << " to " << status_ << std::endl;
		condElection.notify_all();
		condHeartBeat.notify_all();
		condApply.notify_all();
		status = status_;
	}
	//currentTerm
	int LoadCurrentTerm() {
		std::lock_guard<std::mutex> l(mutexCurrentTerm);
		return currentTerm;
	}
	void StoreCurrentTerm(int currentTerm_) {
		std::lock_guard<std::mutex> l(mutexCurrentTerm);
		currentTerm = currentTerm_;
	}
	std::string LoadVotedFor() {
		//votedFor
		std::lock_guard<std::mutex> l(mutexVotedFor);
		return votedFor;
	}
	void StoreVotedFor(const std::string &votedFor_) {
		std::lock_guard<std::mutex> l(mutexVotedFor);
		votedFor = votedFor_;
	}
	//commitIndex
	int LoadCommitIndex() {
		std::lock_guard<std::mutex> l(mutexCommitIndex);
		return commitIndex;
	}
	void StoreCommitIndex(int commitIndex_) {
		std::lock_guard<std::mutex> l(mutexCommitIndex);
		commitIndex = commitIndex_;
	}
	//lastApplied
	int LoadLastApplied() {
		std::lock_guard<std::mutex> l(mutexLastApplied);
		return lastApplied;
	}
};
