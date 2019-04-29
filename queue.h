#ifndef _COLA_
#define _COLA_ 4.0
#include<mutex>
#include<thread>
#include<condition_variable>
#include<queue>

using namespace std;

template<class T, size_t N>
class cola 
{
	private:
		queue<T> q;
		mutex m;
		condition_variable cond_var_put;
		condition_variable cond_var_get;
	public:
		cola(){}
		~cola(){}
		void push(const T &t)
		{
			unique_lock<mutex> lock(m);
			while( q.size() == N ){
			  cond_var_put.wait(lock);
			}
			q.push(t);
			lock.unlock();
			cond_var_get.notify_one();
		}
		void pop(T *t)
		{
			unique_lock<mutex>lock(m);
			while(q.size() == 0){
				cond_var_get.wait(lock);
			}
			lock.unlock();
			cond_var_put.notify_one();
			*t = q.front();
			q.pop();
			return;
		}
};

#endif
