#include "LightThread.h"
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <iostream>

using namespace std;

int counter = 5;

int main(int argc, char** argv) {
	Event a;
	CreateTimer([=](){std::cout<<"2 seconds\n";},2000);
	CreateTimer([=](){std::cout<<"200 milliseconds\n";},200);
	CreateTimer([=](){std::cout<<"200 milliseconds\n";
	CreateTimer([=](){std::cout<<"400 milliseconds\n";},200);
	},200);
	/*
	RetryOperation([=](std::function<void()> success){
		counter--;
			if(counter == 0) {
				success();
			}
			cout<<"Retry\n";
	},1000,10,[=](){});*/
	a.wait();
	return 0;
}
