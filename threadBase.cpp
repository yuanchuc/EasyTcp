#include<iostream>
#include<thread>
#include<mutex> 
#include<atomic>//ԭ��
#include"CELLTimestamp.hpp"
using namespace std;
//ԭ�Ӳ���
mutex m;
const int tCount = 4;
atomic_int sum = 0;
void workFun(int index) {
	//cout << index << endl;
	
	for (int n = 0; n < 20000000; n++) {
		//�Խ���
		//lock_guard<mutex> lg(m);
		//m.lock();
		//�ٽ�����-��ʼ 
		sum++;
		//�ٽ�����-����
		//m.unlock();
	}//�̰߳�ȫ  �̲߳���ȫ
	//cout << index << "=hello ,other thread" << endl;
	
}//��ռʽ

int main() {
	thread* t[tCount];
	for (int n = 0; n < tCount; n++) {
		t[n] =new thread(workFun,n);
	}
	CELLTimestamp tTime;
	for (int n = 0; n < tCount; n++) {
		
		t[n]->join();		//����
		//t[n]->detach();	//������
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