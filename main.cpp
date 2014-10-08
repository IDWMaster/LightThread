#include "LightThread.h"
#include <iostream>


int main(int argc, char** argv) {
	CreateTimer([=](){
		std::cout<<"This should execute after 200 milliseconds\n";
	},200);
	CreateTimer([=](){std::cout<<"This should ALSO execute after 200 milliseconds\n";},200);
	CreateTimer([=](){std::cout<<"This should execute after 50 milliseconds\n";},50);
	CreateTimer([=](){std::cout<<"This should execute after 2 seconds\n";},2000);

return 0;
}
