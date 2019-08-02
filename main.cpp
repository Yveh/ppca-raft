#include "server.hpp"
#include "client.hpp"

void test(std::size_t n) {
	Client c("configs/c1.json");

	std::mt19937 eng(std::random_device{}());
	std::uniform_int_distribution<std::size_t> sleepTimeDist(0, 100);
	auto randSleep = [&eng, &sleepTimeDist] {
		std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeDist(eng)));
	};

	for (int i = 0; i < 30; ++i) {
		auto str = std::to_string(i);
		c.Put(str, str);
		randSleep();
		std::cout << "Put " << i << std::endl;
	}

	for (int i = 0; i < 30; ++i) {
		auto str = std::to_string(i);
		auto res = c.Get(str);
		assert(res == str);
		std::cout << "Get: " << i << std::endl;
		randSleep();
	}
}

int main() {
	Server s1("configs/s1.json");
	Server s2("configs/s2.json");
	Server s3("configs/s3.json");
	Server s4("configs/s4.json");
	Server s5("configs/s5.json");
	s1.StartUp();
	s2.StartUp();
	s3.StartUp();
	s4.StartUp();
	s5.StartUp();

	const std::size_t NClient = 5;
	std::vector<std::thread> ts;
	ts.reserve(NClient);
	for (std::size_t i = 0; i < NClient; ++i) {
		ts.emplace_back(std::bind(test, i));
	}
	for (auto & t : ts)
		t.join();

	return 0;
}
