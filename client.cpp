#include <memory>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <thread>

class A {
public:
	~A() {
		th_.join();
	}
	void Run() {
		th_ = std::thread(&A::do_something, this);
	}

private:
	void do_something() {
		printf("2333");
	}
	std::thread th_;
};

int main() {
	A A_;
	A_.Run();
	return 0;
}

