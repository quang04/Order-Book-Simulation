#pragma once

#include "order.h"
#include "common.h"

#include <boost/asio.hpp>


class OrderBook {
public:
	void PlaceOrder(const Order* _order, boost::asio::io_context& _ioc);
	void Match();
	void Display();
	inline bool IsRunning() const {
		return running.load();
	}
	inline void Stop() {
		running = false;
	}
private:
	inline bool ValidateOrder(double _price, int _quantity) const {
		if (_price <= 0) return false;
		if (_quantity <= 0) return false;
		
		return true;
	}

	std::map<double, std::queue<Order>, std::greater<>> buyOrder;// highest price first
	std::map<double, std::queue<Order>> sellOrder;// lowest price first

	std::atomic_bool running{ true };
	std::mutex orderBookMutex;

};