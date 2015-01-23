#include "LightThread.h"
#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <iostream>



int main(int argc, char** argv) {
	Event a;
	a.signal();
	a.wait();
	printf("This should trigger right away\n");
	CreateTimer([&](){
		printf("This should print after 2 seconds\n");
	},2000);
	CreateTimer([&](){
		printf("This should trigger after 200ms\n");
		a.signal();
	},200);
	CreateTimer([](){printf("This should ALSO trigger ever 200ms\n");},200);
	a.wait();
	printf("This should trigger after the first timer expires\n");

	int i = 3;
	RetryOperation([&](const std::function<void()> successNtfy){
		i--;
		printf("%i\n",i);
		if(i == 0) {
			printf("DONE\n");
			successNtfy();
		}
	},200,50,[=](){});

	a.wait();
	printf("This should NEVER trigger\n");
	return 0;
}
