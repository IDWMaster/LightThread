#ifndef LIGHT_THREAD
#define LIGHT_THREAD

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
	bool hasWork;
	Thread() {
		hasWork = false;
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
			if(!hasWork) {
				std::unique_lock<std::mutex> l(mtx);
			evt.wait(l);
			}
			//It is the caller's responsibility to remove us from the list of available threads
			first = false;
			work();
			hasWork = false;
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
		bot->hasWork = true;
		bot->evt.notify_one();
	}else {
		auto bot = ((Thread*)threads.front());
		threads.pop();
		bot->work = item;
		bot->hasWork = true;
		bot->evt.notify_one();

	}

}


class TimerEvent {
public:
	std::function<void()> functor;
	size_t timeout;
	bool* cancellationToken;
	TimerEvent* next;
	bool operator<(const TimerEvent& other) const {
		return other.timeout>timeout;
	}
	TimerEvent() {
		next = 0;
	}
};

class TimerPool {
public:
	std::thread thread;
	std::set<TimerEvent> events;
	std::mutex mtx;
	std::condition_variable c;
	void Insert(TimerEvent& evt) {
		if(events.find(evt) == events.end()) {
			events.insert(evt);
			}else {
				TimerEvent found = *events.find(evt);
				evt.next = found.next;
				found.next = new TimerEvent(evt);
				events.erase(found);
				events.insert(found);
			}
	}
	TimerPool() {
		thread = std::thread([=](){
			while(true) {
				{
					{
					std::unique_lock<std::mutex> l(mtx);
					while(!events.empty()) {
						std::vector<TimerEvent> currentEvents;
						TimerEvent cevt = *events.begin();
						size_t ctimeout = cevt.timeout;
						while(true) {
							currentEvents.push_back(cevt);
							if(cevt.next == 0) {
								break;
							}
							TimerEvent* ptr = cevt.next;
							cevt = *ptr;
							delete ptr;
						}
						events.erase(events.begin());
						l.unlock();
						auto start = std::chrono::steady_clock::now();

						std::mutex mx;
						std::unique_lock<std::mutex> ml(mx);
						if(c.wait_for(ml,std::chrono::milliseconds(ctimeout)) == std::cv_status::timeout) {


						for(auto i = currentEvents.begin(); i != currentEvents.end();i++) {
							if(*(i->cancellationToken)) {
							SubmitWork(i->functor);
							}
							delete i->cancellationToken;
						}
						}else {
							auto end = std::chrono::steady_clock::now();
							auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
							for(auto i = currentEvents.begin();i!= currentEvents.end();i++) {
								i->timeout-=milliseconds;
								events.insert(*i);
							}
							//printf("Interrupt\n");
						}
						l.lock();
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
	timerPool.Insert(evt);
	timerPool.c.notify_one();
	}
	return evt.cancellationToken;
}
static void CancelTimer(bool* cancellationToken) {
	*cancellationToken = false;
	timerPool.c.notify_one();
}





class Event {
public:
	std::mutex mtx;
	bool triggered;
	std::condition_variable evt_int;
	Event() {
		triggered = false;
	}
	void signal() {
		std::unique_lock<std::mutex> l(mtx);
		triggered = true;
		evt_int.notify_all();
	}
	void wait() {
		std::unique_lock<std::mutex> l(mtx);
		while(!triggered) {
		evt_int.wait(l);
		}
		triggered = false;
	}
};


//Retries something until it succeeds.
template<typename F, typename Y>
static void RetryOperation(const F& functor, size_t retryMS, size_t retryCount, const Y& onFailure) {

	std::function<void()>* fptr = new std::function<void()>();
	bool** retryTimer = new bool*();
	size_t* count = new size_t(retryCount);
	auto cleanup = [=](){
		delete retryTimer;
		delete fptr;
		delete count;
	};
	*fptr = [=](){
		bool abrt = false;
		auto cancel = [&](){
			if(*retryTimer) {
				CancelTimer(*retryTimer);
			}
			abrt = true;

		};

		functor(cancel);
		if(abrt) {
			cleanup();
			return;
		}
		if(*count == 0) {
			cleanup();
			onFailure();
		}else {
			(*count)--;
			*retryTimer = CreateTimer(*fptr,retryMS);
		}
	};
	(*fptr)();


}


#endif
