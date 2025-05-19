#pragma once

#include "boost/lockfree/queue.hpp"
#include "order.h"
#include "common.h"

#define MAX_QUEUE_CAPCITY 65534

class OrderQueue
{
public:
	inline bool Push(Order* _order)
	{
		// TODO: mayneed check threshold capacity
		bool isOk = q.push(_order);

		// using weak order rather than strong order
		if (isOk) hasOrder.store(true, std::memory_order::release);

		return isOk;
	}
	inline bool Pop(Order*& _order)
	{
		bool isOk = q.pop(_order);

		// using weak order rather than strong order
		if (!isOk) hasOrder.store(false, std::memory_order::release);

		return isOk;
	}

	inline std::atomic_bool& GetHasOrder() {
		return hasOrder;
	}
	
private:
	boost::lockfree::queue<Order*> q{MAX_QUEUE_CAPCITY};
	std::atomic_bool hasOrder{ false };

};
