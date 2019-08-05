#include <string>
#include <memory>
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <map>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <grpcpp/grpcpp.h>
#include <protos/raft.pb.h>
#include "protos/raft.grpc.pb.h"

class Client {
public:
	explicit Client(std::string filename) {
		boost::property_tree::ptree root;
		boost::property_tree::read_json<boost::property_tree::ptree>("configs/" + filename, root);

		for (auto &&adr : root.get_child("ServerList")) {
			serverList.emplace_back(adr.second.get_value<std::string>());
		}

		for (auto i : serverList)
			stub_[i] = RAFT::NewStub(grpc::CreateChannel(i, grpc::InsecureChannelCredentials()));
	}
	void Put(std::string key, std::string value) {
		while (1) {
			PutRequest request_;
			request_.set_key(key);
			request_.set_value(value);
			PutReply reply_;
			grpc::ClientContext context_;
			context_.set_deadline(std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(200));
			grpc::Status status_ = stub_[serverList[rand() % serverList.size()]]->Put(&context_, request_, &reply_);
			if (status_.ok() && reply_.success()) {
				break;
			}
//			else
//				std::cout << ".";
		}
	}
	std::string Get(std::string key) {
		while (1) {
			GetRequest request_;
			request_.set_key(key);
			GetReply reply_;
			grpc::ClientContext context_;
			context_.set_deadline(std::chrono::high_resolution_clock::now() + std::chrono::milliseconds(200));
			grpc::Status status_ = stub_[serverList[rand() % serverList.size()]]->Get(&context_, request_, &reply_);
			if (status_.ok() && reply_.success()) {
				return reply_.value();
			}
//			else
//				std::cout << ".";
		}
	}
private:
	std::vector<std::string> serverList;
	std::map<std::string, std::unique_ptr<RAFT::Stub>> stub_;
};
