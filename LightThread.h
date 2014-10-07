#include <thread>
#include <mutex>
#include <queue>
static std::mutex mtx;

static std::queue<void*> threads;
class Thread {
public:
	std::thread thread;
	std::mutex mtx;
	std::condition_variable evt;
	std::function<void()> work;
	Thread() {
		thread = std::thread([=](){
			bool first = true;
			while(true) {
				{
					if(!first) {
					//Add ourselves to the availability list
					std::unique_lock<std::mutex> ml(::mtx);
					threads.push(this);
					}
				}
			if(!first) {


				std::unique_lock<std::mutex> l(mtx);
			evt.wait(l);
			}
			//It is the caller's responsibility to remove us from the list of available threads
			first = false;
			work();
			//Free the lambda by swapping it out
			work = std::function<void()>();
			}
		});
	}
	~Thread() {

	}
};
static void SubmitWork(const std::function<void()>& item) {
	std::unique_lock<std::mutex> l(mtx);
	//We don't like to be kept waiting! Add a new thread immediately!
	if(threads.empty()) {
		auto bot = new Thread();
		bot->work = item;
		bot->evt.notify_one();
	}else {
		auto bot = ((Thread*)threads.front());
		threads.pop();
		bot->work = item;
		bot->evt.notify_one();

	}

}

