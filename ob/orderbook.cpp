#include "orderbook.h"

void OrderBook::PlaceOrder(const Order* _order, boost::asio::io_context& _ioc)
{

	std::lock_guard<std::mutex> lock(orderBookMutex);

	if (!ValidateOrder(_order->price, _order->quantity)) return;

	// copy its value
	Order order(*_order);

	if (_order->side == SIDE::BUY)
	{
		auto& b = buyOrder;
		auto& q = b[_order->price];
		q.emplace(order);
	}
	else
	{
		auto& b = sellOrder;
		auto& q = b[_order->price];
		q.emplace(order);
	}
	
	// try to match when new order arrive
	boost::asio::post([this]() {
		this->Match();
		});
}

void OrderBook::Match()
{
	std::lock_guard<std::mutex> lock(orderBookMutex);

	if (buyOrder.empty() || sellOrder.empty()) return;

	auto buyIt = buyOrder.begin();
	auto sellIt = sellOrder.begin();

	double buyPrice = buyIt->first;
	double sellPrice = sellIt->first;
	auto& buyQueue = buyIt->second;
	auto& sellQueue = sellIt->second;

	if (buyPrice >= sellPrice && !buyQueue.empty() && !sellQueue.empty())
	{
		
		Order& buy = buyQueue.front();
		Order& sell = sellQueue.front();

		int matchedQuantity = std::min(buy.quantity, sell.quantity);
		double matchPrice = sell.timestamp < buy.timestamp ? sellPrice : buyPrice;

		std::cout << std::format("Matched: Buy {} with Sell {} for {} at {:.2f}$\n", buy.id.data(), sell.id.data(), matchedQuantity, matchPrice);

		buy.quantity -= matchedQuantity;
		sell.quantity -= matchedQuantity;

		// remove order if fully matched
		if (buy.quantity == 0)
		{
			// pop current order when fully match
			buyQueue.pop();

			if (buyQueue.empty())
			{
				buyOrder.erase(buyIt);
			}
		}

		if (sell.quantity == 0)
		{
			// pop current order that fully match
			sellQueue.pop();

			if (sellQueue.empty())
			{
				sellOrder.erase(sellIt);
			}
		}

	}

}

void OrderBook::Display()
{
	std::cout << "Buy Order\n";
	if (buyOrder.empty()) std::cout << "Empty\n";

	for (const auto& p : buyOrder)
	{
		const auto& buyQueue = p.second;
		auto copyQueue = buyQueue;

		while (!copyQueue.empty())
		{
			const auto& order = copyQueue.front();

			std::cout << std::format("ID: {}. Side: Buy. Price: {:.2f}$. Qty: {}. Time: {}\n",
				order.id.data(),
				order.price,
				order.quantity,
				order.timestamp);


			copyQueue.pop();
		}
	}


	std::cout << "Sell Order\n";
	if (sellOrder.empty()) std::cout << "Empty\n";

	for (const auto& p : sellOrder)
	{
		const auto& sellQueue = p.second;
		auto copyQueue = sellQueue;

		while (!copyQueue.empty())
		{
			const auto& order = copyQueue.front();

			std::cout << std::format("ID: {}. Side: Sell. Price: {:.2f}$. Qty: {}. Time: {}\n",
				order.id.data(),
				order.price,
				order.quantity,
				order.timestamp);


			copyQueue.pop();
		}
	}

	
}


