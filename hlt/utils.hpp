//
// Created by aiden on 16/11/18.
//

#ifndef MYBOT_UTILS_H
#define MYBOT_UTILS_H

#include <sys/time.h>
#include <string>
#include <bits/stdc++.h>

using namespace std;

double getTime();

struct Timer {
	double total = 0;
	double startTime;
	bool running = false;
	string name = "";

	Timer(string _name = "") {
		name = _name;
	}

	void start() {
		startTime = getTime();
		running = true;
	}

	void stop() {
		total += getTime() - startTime;
		running = false;
	}

	double elapsed() {
		return total + (running ? getTime() - startTime : 0);
	}

	string tostring() {
		return "Timer" + name + ": " + to_string(elapsed());
	}
};


class EMA {



};


#endif //MYBOT_UTILS_H
