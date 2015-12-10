#ifndef LIGHT_THREAD
#define LIGHT_THREAD

#include <thread>
#include <mutex>
#include <queue>
#include <set>
#include <condition_variable>
#include <memory>


namespace LightThread {

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
					std::unique_lock<std::mutex> ml(LightThread::mtx);
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
	uint64_t timeout;
	bool cancellationToken;
	std::shared_ptr<TimerEvent> next;
	bool operator<(const TimerEvent& other) const {
		return other.timeout>timeout;
	}
	TimerEvent() {
		next = 0;
	}
};
template<typename T>
class shared_ref {
public:
	std::shared_ptr<T> val;
	shared_ref(const std::shared_ptr<T>& obj) {
		val = obj;
	}
	T& operator*() const {
		return *val;
	}
	T* operator->() const {
		return val.get();
	}
	operator std::shared_ptr<T>() const {
		return val;
	}
	bool operator<(const shared_ref<T>& other) const {
		return (*val)<(*other.val);
	}
};

class TimerPool {
public:
	std::thread thread;
	std::set<shared_ref<TimerEvent>> events;
	std::mutex mtx;
	std::condition_variable c;
	void Insert(std::shared_ptr<TimerEvent> evt) {
		if(events.find(evt) == events.end()) {
			events.insert(evt);
			}else {
				std::shared_ptr<TimerEvent> found = *events.find(evt);
				events.erase(found);
				evt->next = found;
				events.insert(evt);
			}
	}
	TimerPool() {
		thread = std::thread([=](){
			while(true) {
				{
					{
					std::unique_lock<std::mutex> l(mtx);
					while(!events.empty()) {
						std::vector<std::shared_ptr<TimerEvent>> currentEvents;
						std::shared_ptr<TimerEvent> cevt = *events.begin();
						uint64_t ctimeout = cevt->timeout;
						while(true) {
							currentEvents.push_back(cevt);
							if(cevt->next == 0) {
								break;
							}
							std::shared_ptr<TimerEvent> ptr = cevt->next;
							cevt->next = 0;
							cevt = ptr;
						}
						events.erase(events.begin());
						l.unlock();
						auto start = std::chrono::steady_clock::now();

						std::mutex mx;
						std::unique_lock<std::mutex> ml(mx);

						if(c.wait_for(ml,std::chrono::milliseconds(ctimeout)) == std::cv_status::timeout) {
							for(auto i = events.begin();i != events.end();i++) {
								if(ctimeout>i->val->timeout) {
									//Execute immediately
									i->val->timeout = 0;
								}else {
									//Defer execution
									i->val->timeout-=(ctimeout);
								}
							}
						for(auto i = currentEvents.begin(); i != currentEvents.end();i++) {
							if((*i)->cancellationToken) {
							SubmitWork((*i)->functor);
							}
						}
						}else {
							this->mtx.lock();
							for(auto i = currentEvents.begin();i != currentEvents.end(); i++) {
								Insert(*i);
							}
							auto end = std::chrono::steady_clock::now();
							auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
							//TODO: Subtract milliseconds from all timers
							for(auto i = events.begin();i!= events.end();i++) {
								if(milliseconds>i->val->timeout) {
									i->val->timeout = 0;
								}else {
									i->val->timeout-=milliseconds;
								}
							}
							this->mtx.unlock();


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
static std::shared_ptr<TimerEvent> CreateTimer(const std::function<void()>& callback, size_t timeout) {

	std::shared_ptr<TimerEvent> evt = std::make_shared<TimerEvent>();
	evt->functor = callback;
	evt->timeout = timeout;
	evt->cancellationToken = true;
	{
	std::lock_guard<std::mutex> mg(timerPool.mtx);
	timerPool.Insert(evt);
	timerPool.c.notify_one();
	}
	return evt;
}
static void CancelTimer(std::shared_ptr<TimerEvent> timer) {
	timer->cancellationToken = false;
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


class RetryOp {
public:

	std::function<void()> retrydgate;
	std::function<void()> cancel;
	size_t counter;

};

//Retries something until it succeeds.
template<typename F, typename Y>
static void RetryOperation(const F& functor, size_t retryMS, size_t retryCount, const Y& onFailure) {
	std::shared_ptr<RetryOp> data = std::make_shared<RetryOp>();
	data->counter = retryCount;
	data->cancel = [=]() {
		data->cancel = [=](){};
		data->retrydgate = [=](){};

	};
	data->retrydgate = [=](){
		functor(data->cancel);
		if(data->counter) {
			data->counter--;
			CreateTimer(data->retrydgate,retryMS);
		}else {
			onFailure();
		}
	};
	data->retrydgate();


}

//A simple, easy-to-use "virtual stack"-based memory allocator for short-lived objects
//An advantage to this allocator is that it works like a safer version of alloca,
//and can grow dynamically if necessary.
class SafeStack {
public:
	unsigned char* start;
	unsigned char* ptr;
	size_t sz;
	SafeStack(size_t defaultSize) {
		start = (unsigned char*)malloc(defaultSize);
		ptr = start;
		sz = defaultSize;
	}
	void* Allocate(size_t bytes, size_t& methodStack) {
		ptr+=bytes;
		size_t a = (size_t)ptr;
		size_t b = (size_t)start;
		size_t dif = a-b;
		while(dif>sz) {
			sz*=2;
			start = (unsigned char*)realloc(start,sz);
			ptr = start+dif;
		}
		methodStack+=bytes;
		return ptr;
	}
	void Free(const size_t& methodStack) {
		ptr-=methodStack;
	}
	~SafeStack() {
		delete ptr;
	}
};
class SafeStack_Allocator {
public:
	SafeStack* ptr;
	size_t method;
	SafeStack_Allocator(SafeStack& stack) {
		ptr = &stack;
		method = 0;
	}
	void* Allocate(const size_t& sz) {
		return ptr->Allocate(sz,method);
	}
	~SafeStack_Allocator() {
		ptr->Free(method);
	}
};
}
#endif
