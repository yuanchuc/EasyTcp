#include<iostream>
#include<thread>
#include<mutex> 
#include<atomic>//原子
#include"CELLTimestamp.hpp"
using namespace std;
//原子操作
mutex m;
const int tCount = 4;
atomic_int sum = 0;
void workFun(int index) {
	//cout << index << endl;
	
	for (int n = 0; n < 20000000; n++) {
		//自解锁
		//lock_guard<mutex> lg(m);
		//m.lock();
		//临界区域-开始 
		sum++;
		//临界区域-结束
		//m.unlock();
	}//线程安全  线程不安全
	//cout << index << "=hello ,other thread" << endl;
	
}//抢占式

int main() {
	thread* t[tCount];
	for (int n = 0; n < tCount; n++) {
		t[n] =new thread(workFun,n);
	}
	CELLTimestamp tTime;
	for (int n = 0; n < tCount; n++) {
		
		t[n]->join();		//阻塞
		//t[n]->detach();	//非阻塞
	}
	cout<< tTime.getElapsedTimeInMilliSec() <<" sum=" << sum << endl;
	int sum = 0;
	tTime.update();
	for (int n = 0; n < 80000000; n++) {
		sum++;
	}
	cout << tTime.getElapsedTimeInMilliSec() << " sum=" << sum << endl;
	cout << "hello ,main thread" << endl;

	return 0;
}