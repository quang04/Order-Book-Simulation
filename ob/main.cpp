// main.cpp : Defines the entry point for the application.
//

#include <boost/asio/thread_pool.hpp>
#include <boost/asio.hpp>
#include <boost/pool/object_pool.hpp>

#include "common.h"
#include "orderqueue.h"
#include "orderbook.h"
#include "spinlock.h"

constexpr int gTotalThreadForMatching = 4;
constexpr int gTotalProducer = 5;
constexpr int gTotalConsumer = 2;
constexpr int gTotalOrderPerUser = 200;
constexpr int gBatchSize = 10;
constexpr int gPreAllocateOrderSize = 65534;
// number of spin lock will spin when queue empty
constexpr int gQueueEmptySpinCount = 1000000;
std::mutex gMutexConsummer;

std::atomic<int> gCpuIndex = 0;


inline uint64_t GetTimeStamp()
{
	return __rdtsc();
}

void PinThread(DWORD coreID)
{
	// keep move to next core
	DWORD_PTR m = (1ULL << coreID);
	HANDLE hThread = GetCurrentThread();

	auto r = SetThreadAffinityMask(hThread, m);

	if (r == 0)
	{
		std::cout << "Failed to set thread affinity\n";
		std::cout << "Error: " << GetLastError() << std::endl;
	}
	else {
		std::cout << "Succeed to set thread affinity to cpu core: " << coreID << "\n";
	}
}

// thread for maching
void MatchingEngine(OrderBook& _book, boost::asio::io_context& _ioc, std::atomic_bool& _isRunning)
{
	while (_isRunning.load(std::memory_order_acquire) && _book.IsRunning())
	{
		boost::asio::post(_ioc, [&_book]() {

			// continously check matching with thread pool
			_book.Match();
			});

		// very slight delay using spin lock
		SpinLock::Delay(100000);
	}
}

// batch processing to add oder to book
void Consumer(OrderBook& _book,
	boost::asio::io_context& _ioc,
	std::atomic_bool& _running,
	OrderQueue& _queue,
	boost::lockfree::queue<Order*>& _freeQueue)
{
	std::vector<Order*> batchOrder;
	batchOrder.reserve(gBatchSize);

	while (_running.load(std::memory_order_acquire))
	{
		batchOrder.clear();

		int i = 0;
		Order* order = nullptr;
		while (i++ < gBatchSize && _queue.Pop(order))
		{
			batchOrder.push_back(order);
		}
		
		// do spin lock rather than sleep.
		if (batchOrder.empty())
		{
			int spinCount = 0;

			// using weak order rather than strong order memory
			while (!_queue.GetHasOrder().load(std::memory_order_acquire) &&
				_running.load(std::memory_order_acquire))
			{
				if (spinCount++ < gQueueEmptySpinCount)
				{
					// tell other thread just do their work
					std::this_thread::yield();
				}
				else
				{
					// when it exceeds threshold(low-activity) can perform slowly
					std::this_thread::sleep_for(std::chrono::milliseconds(1));
					spinCount = 0;
				}
			}
			continue;
		}

		{
			std::lock_guard<std::mutex> lock(gMutexConsummer);
			for (const auto* item : batchOrder)
			{
				// quantity == 0 normally mean add default Order so skip it
				if (item->quantity == 0) continue;

				_book.PlaceOrder(item, _ioc);
			}
		}

		// try return to freeQueue for reuse
		// dont want to put this one under lock so will create another loop
		for (auto* item : batchOrder)
		{
			_freeQueue.push(item);
		}

	}
}

void Producer(OrderQueue& _queue,
	int _id,
	int _numOrders,
	std::atomic_bool& _running,
	boost::lockfree::queue<Order*>& _freeQueue,
	boost::object_pool<Order>& _orderPool)
{
	// try to create random order
	std::random_device rd;
	std::mt19937 gen(rd());
	
	std::uniform_real_distribution<double> priceRange(50.0, 150.0);
	std::uniform_int_distribution qtyRange(100, 10000);
	std::uniform_int_distribution sideRange(0, 1);
	std::uniform_int_distribution delayRange(20, 100);

	for (int i = 0; i < _numOrders && _running; i++)
	{
		Order* order = nullptr;

		std::string id = (std::format("User{}_Ord{}", _id, i)).data();
		SIDE side = sideRange(gen) ? SIDE::BUY : SIDE::SELL;
		// get 2 value decimal
		double price = std::round(priceRange(gen) * 100) / 100;
		int qty = qtyRange(gen);
		uint64_t ts = GetTimeStamp();
		std::array<char, 64> idConvert{ '\0' };
		std::copy(id.begin(), id.end(), idConvert.data());

		// try to reuse from freequeue
		if (_freeQueue.pop(order))
		{
			order->id = idConvert;
			order->price = price;
			order->quantity = qty;
			order->side = side;
			order->timestamp = ts;
		}
		else
		{
			// when free queue is run out
			// try to use from _orderPool
			void* borrowMemory = _orderPool.malloc();
			order = new(borrowMemory) Order(idConvert, side, price, qty, ts);
		}


		_queue.Push(order);

		std::this_thread::sleep_for(std::chrono::milliseconds(delayRange(gen)));
	}
}


int main()
{

	OrderQueue queue;
	OrderBook book;

	// preallocate order to avoid allocate/deallocate frequently
	boost::lockfree::queue<Order*> freeQueue{ gPreAllocateOrderSize };

	// create object pool, when run out data in freeQueue will use directly from pool
	// each producer has there own object pool
	std::vector<std::unique_ptr<boost::object_pool<Order>>> orderObjectPools(gTotalProducer);
	for (int i = 0; i < gTotalProducer; i++)
	{
		orderObjectPools[i] = std::make_unique<boost::object_pool<Order>>();
	}

	// for shut down thread
	std::atomic_bool running{ true };

	boost::asio::io_context ioc;
	ioc.restart();

	// exclusively keep pool runing
	auto workGuard = boost::asio::make_work_guard(ioc);

	// starting thread pool
	std::vector<std::thread> vectorPoolThread;
	for (int i = 0; i < gTotalThreadForMatching; i++)
	{
		vectorPoolThread.emplace_back([&ioc]() {
			PinThread(gCpuIndex++);
			ioc.run();
			});
	}

	// starting matching thread, to accumulate order then feed to matching pool
	std::thread matchingThread([&book, &ioc, &running]() {
		PinThread(gCpuIndex++);
		MatchingEngine(book, ioc, running);
		});


	// starting producer to simulate the user
	std::vector<std::thread> vectorProducer;
	for (int i = 0; i < gTotalProducer; i++)
	{
		auto& orderPool = *orderObjectPools[i];

		vectorProducer.emplace_back([&queue, &running, &freeQueue, &orderPool, i]() {
			PinThread(gCpuIndex++);
			Producer(queue, i, gTotalOrderPerUser, running, freeQueue, orderPool);
			});
	}

	// starting consumer for receiving order then push to order book
	std::vector<std::thread> vectorConsumer;
	for (int i = 0; i < gTotalConsumer; i++)
	{
		vectorConsumer.emplace_back([&book, &ioc, &running, &queue, &freeQueue]() {
			PinThread(gCpuIndex++);
			Consumer(book, ioc, running, queue, freeQueue);
			});
	}

	// sleep amount of time to let it run
	std::this_thread::sleep_for(std::chrono::seconds(30));


	// wait producer complete
	for (auto& t : vectorProducer)
	{
		if (t.joinable()) t.join();
	}


	book.Stop();
	running.store(false, std::memory_order::memory_order_release);

	ioc.stop();
	workGuard.reset();


	// wait consumer
	for (auto& t : vectorConsumer)
	{
		if (t.joinable()) t.join();
	}

	// matching thread
	if(matchingThread.joinable()) matchingThread.join();

	// maching thread pool
	for (auto& t : vectorPoolThread)
	{
		if (t.joinable()) t.join();
		else std::cout << "Thread can not join\n";
	}

	// display remain order
	book.Display();

	// clean everything in free queue
	Order* ord = nullptr;
	while (freeQueue.pop(ord)) {};


	std::cout << "Press any key to close\n";
	auto _ = std::getchar();

	return 0;
}
