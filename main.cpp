#include "server.hpp"
#include "client.hpp"

void test(std::string config) {
	Client c(config);

	std::mt19937 eng(std::random_device{}());
	std::uniform_int_distribution<std::size_t> sleepTimeDist(0, 100);
	auto randSleep = [&eng, &sleepTimeDist] {
		std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeDist(eng)));
	};

	for (int i = 0; i < 300; ++i) {
		auto str = std::to_string(i);
		c.Put(str, str);
		randSleep();
		std::cout << "Put " << i << std::endl;
	}

	for (int i = 0; i < 300; ++i) {
		auto str = std::to_string(i);
		auto res = c.Get(str);
		assert(res == str);
		std::cout << "Get: " << i << std::endl;
		randSleep();
	}
}

int main(int argc, char **argv) {

	assert(argc == 3);
	std::string config = argv[2];
	if (std::string(argv[1]) == "client") {
		getchar();
		test(config);
		return 0;
	}
	assert(std::string(argv[1]) == "server");
	std::cout << "pid = " << getpid() << std::endl;
	getchar();
	Server s(config);
	s.StartUp();
	while (1) ;


	assert(false);

	return 0;
}
