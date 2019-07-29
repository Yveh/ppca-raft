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
private:
	std::vector<std::string> serverList;
	std::map<std::string, std::unique_ptr<RAFT::Stub>> stub_;
};

int main() {
	Client c1("c1.json");
	while (1) {
		std::cout << c1.SayHello("0.0.0.0:50001", "test") << std::endl;
	}
	return 0;
}