#include <string>

#include "ServerModule.hpp"
#include "ClientModule.hpp"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

class Server {
public:
	Server(const std::string& filename) {
		std::cout << "read json from " << filename << std::endl;
		boost::property_tree::ptree root;
		boost::property_tree::ptree items;
		boost::property_tree::read_json<boost::property_tree::ptree>(filename, root);

		LocalAddress = root.get<std::string>("LocalAddress");

		for (auto &&adr : root.get_child("ServerList")) {
			ServerList.emplace_back(adr.second.get_value<std::string>());
		}

		ServerList.erase(remove(ServerList.begin(), ServerList.end(), LocalAddress), ServerList.end());
		std::cout << "LocalAddress : " << LocalAddress << std::endl;
		for (auto i : ServerList)
			std::cout << "Server " << i << std::endl;

		Cmodule.Init(ServerList);
	}
	void ShutDown() {
		std::cout << "Shuting down..." << std::endl;
		Cmodule.ShutDown();
		Smodule.ShutDown();
	}
	~Server() {
		ShutDown();
		if (thread_.joinable())
			thread_.join();
		if (thread__.joinable())
			thread__.join();
	}
	void StartUp() {
		std::cout << "Server Start" << std::endl;
		thread__ = std::thread(&ServerModule::Run, &Smodule, LocalAddress);
		thread_ = std::thread(&ClientModule::CompleteRpc, &Cmodule);
	}
	void dosomething() {
		for (auto i : ServerList)
			Cmodule.SayHello(i, "from" + LocalAddress);
	}
private:
	std::string LocalAddress;
	std::vector<std::string> ServerList;
	ServerModule Smodule;
	ClientModule Cmodule;
	std::thread thread_, thread__;
};

int main() {
	Server s1("s1.json");
	Server s2("s2.json");
	Server s3("s3.json");
	s1.StartUp();
	s2.StartUp();
	s3.StartUp();
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	s1.dosomething();
	s2.dosomething();
	s3.dosomething();
	std::this_thread::sleep_for(std::chrono::milliseconds(5000));

//	while (1);
	//std::this_thread::sleep_for(std::chrono::milliseconds(5000));
	return 0;
}