#pragma once
#include "common.h"

enum class SIDE : int {
	BUY = 0,
	SELL
};


struct Order {
	// boost not allow dynamic size data type, so fix id size
	std::array<char, 64> id;
	SIDE side;
	double price;
	int quantity;
	uint64_t timestamp;
	
	Order()
		: side(SIDE::BUY)
		, price(0.0)
		, quantity(0)
		, timestamp(0)
		, id{ '\0' }
	{
	}

	Order(std::array<char, 64> _id, SIDE _side, double _price, int _quantity, uint64_t _timestamp)
		: side(_side)
		, price(_price)
		, quantity(_quantity)
		, timestamp(_timestamp)
		, id(_id)
	{
	}

	Order(const Order& _other) noexcept
		: side(_other.side)
		, price(_other.price)
		, quantity(_other.quantity)
		, timestamp(_other.timestamp)
		, id(_other.id)
	{
	}

	Order(Order&& _other) noexcept
		: side(std::move(_other.side))
		, price(std::move(_other.price))
		, quantity(std::move(_other.quantity))
		, timestamp(std::move(_other.timestamp))
		, id(std::move(_other.id))
	{
	}


	Order& operator=(const Order& _other) noexcept
	{
		this->side = _other.side;
		this->price = _other.price;
		this->quantity = _other.quantity;
		this->timestamp = _other.timestamp;
		this->id = _other.id;
	}



};