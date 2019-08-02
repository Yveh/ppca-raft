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

class Client {
public:
	explicit Client(std::string filename) {
		boost::property_tree::ptree root;
		boost::property_tree::read_json<boost::property_tree::ptree>(filename, root);

		for (auto &&adr : root.get_child("ServerList")) {
			serverList.emplace_back(adr.second.get_value<std::string>());
		}

		for (auto i : serverList)
			stub_[i] = RAFT::NewStub(grpc::CreateChannel(i, grpc::InsecureChannelCredentials()));
	}
	std::string SayHello(std::string channel, std::string user) {
		HelloRequest request_;
		request_.set_name(user);
		HelloReply reply_;
		grpc::ClientContext context_;
		grpc::Status status_ = stub_[channel]->SayHello(&context_, request_, &reply_);
		if (status_.ok()) {
			return reply_.message();
		}
		else {
			std::cout << status_.error_code() << ": " << status_.error_message()
					  << std::endl;
			return "RPC failed";
		}
	}
	void Put(std::string key, std::string value) {
		while (1) {
			PutRequest request_;
			request_.set_key(key);
			request_.set_value(value);
			PutReply reply_;
			grpc::ClientContext context_;
			grpc::Status status_ = stub_[serverList[rand() % serverList.size()]]->Put(&context_, request_, &reply_);
			if (status_.ok() && reply_.success()) {
				break;
			}
			else
				std::cout << ".";
		}
	}
	std::string Get(std::string key) {
		while (1) {
			GetRequest request_;
			request_.set_key(key);
			GetReply reply_;
			grpc::ClientContext context_;
			grpc::Status status_ = stub_[serverList[rand() % serverList.size()]]->Get(&context_, request_, &reply_);
			if (status_.ok() && reply_.success()) {
				return reply_.value();
			}
			else
				std::cout << ".";
		}
	}
private:
	std::vector<std::string> serverList;
	std::map<std::string, std::unique_ptr<RAFT::Stub>> stub_;
};

int main() {
	Client c1("c1.json");
//	c1.Put("x", "1");
//	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//	c1.Put("y", "2");
//	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//	c1.Put("z", "3");
//	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//	c1.Get("x");
	std::string str1, str2, opt;
	while (1) {
		std::cin >> opt;
		if (opt == "put") {
			std::cin >> str1 >> str2;
			c1.Put(str1, str2);
			std::cout << "commited" << std::endl;
		}
		else if (opt == "get"){
			std::cin >> str1;
			str2 = c1.Get(str1);
			std::cout << "value = " << (str2.empty() ? "None" : str2) << std::endl;
		}
		else {
			std::cout << "command error!" << std::endl;
		}
	}
	return 0;
}