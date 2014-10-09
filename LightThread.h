#include <thread>
#include <mutex>
#include <queue>
#include <set>
#include <condition_variable>
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


class TimerEvent {
public:
	std::function<void()> functor;
	size_t timeout;
	bool* cancellationToken;
	bool operator<(const TimerEvent& other) const {
		return other.timeout>timeout;
	}
};

class TimerPool {
public:
	std::thread thread;
	std::set<TimerEvent> events;
	std::mutex mtx;
	std::condition_variable c;

	TimerPool() {
		thread = std::thread([=](){
			while(true) {
				{
					std::lock_guard<std::mutex> l(mtx);
					while(!events.empty()) {
						std::vector<TimerEvent> currentEvents;
						size_t ctimeout = events.begin()->timeout;
						while(true) {
							if(events.empty()) {
								break;
							}

							if(events.begin()->timeout == ctimeout) {
								//Add to list
								currentEvents.push_back(*events.begin());
								events.erase(events.begin());
							}else {
								break;
							}
						}
						std::this_thread::sleep_for(std::chrono::milliseconds(ctimeout));
						for(auto i = currentEvents.begin(); i != currentEvents.end();i++) {
							if(*(i->cancellationToken)) {
							SubmitWork(i->functor);
							}
							delete i->cancellationToken;
						}
					}

				}
				std::mutex mx;
				std::unique_lock<std::mutex> ml(mx);
				c.wait(ml);

			}
		});
	}
};
static TimerPool timerPool;
static bool* CreateTimer(const std::function<void()>& callback, size_t timeout) {

	TimerEvent evt;
	evt.functor = callback;
	evt.timeout = timeout;
	evt.cancellationToken = new bool(true);
	{
	std::lock_guard<std::mutex> mg(timerPool.mtx);
	timerPool.events.insert(evt);
	}
	timerPool.c.notify_one();
	return evt.cancellationToken;
}
