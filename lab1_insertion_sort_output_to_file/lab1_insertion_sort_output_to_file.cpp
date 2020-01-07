// lab1_insertion_sort_output_to_file.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <time.h>
#include <algorithm>
#include <fstream>
#include <iterator>
#include <functional>
#include <experimental/filesystem>

#include <mutex>
#include <future> 
#include <chrono> 
#include <stack>
#include <chrono>
#include <ratio>
#include <unordered_map>
#include <queue>
#include <shared_mutex>
#include <thread>

#include "../include/sort.h"
#include "../include/log.h"
#include "../include/ptr.h"


namespace asio
{
	namespace container
	{
		template<class _Ty,
			class _Container = std::deque<_Ty> >
			class queue
			: std::queue<_Ty, _Container>
		{
			using baseT = typename std::queue<_Ty, _Container>;
		public:
			bool empty() const
			{
				_STD shared_lock<_STD shared_mutex> lock(_mtx);
				return (baseT::empty());
			}

			void push(baseT::value_type&& _Val)
			{
				_STD unique_lock<_STD shared_mutex> lock(_mtx);
				baseT::push(_STD move(_Val));
			}
			void push(baseT::value_type const &_Val)
			{
				_STD unique_lock<_STD shared_mutex> lock(_mtx);
				baseT::push_back(_STD move(_Val));
			}

			void pop()
			{
				_STD unique_lock<_STD shared_mutex> lock(_mtx);
				baseT::pop();
			}

			baseT::reference front()
			{
				_STD shared_lock<_STD shared_mutex> lock(_mtx);
				// return first element of mutable queue
				return (baseT::front());
			}
			baseT::const_reference front() const
			{
				_STD shared_lock<_STD shared_mutex> lock(_mtx);
				// return first element of nonmutable queue
				return (baseT::front());
			}
		private:
			mutable _STD shared_mutex _mtx;
		};
	}

	class io_service
	{
	public:
		using CompletionHandlerT = std::function<void()>;
		io_service(std::chrono::milliseconds wait_timeout = std::chrono::milliseconds(10))
			: _wait_timeout(wait_timeout)
		{

		}

		template<typename CompletionHandler>
		void post(CompletionHandler handler)
		{
			_tasks.push(handler);
		}
		void run()
		{
			while (!_tasks.empty())
				_run_task();
		}
		void run_one()
		{
			if (!_tasks.empty())
				_run_task();
		}

		// wait until tasks are completed 
		void wait()
		{
			while (!_tasks.empty())
				std::this_thread::sleep_for(_wait_timeout);
		}

		class strand
		{
		public:
			strand(io_service *service)
				: _service(service)
				, _isInterrupted(false)
			{
			}

			template<typename CompletionHandler>
			void post(CompletionHandler handler)
			{
				_service->post(handler);
			}
			void run()
			{
				while (!_isInterrupted.load())
					_service->run_one();
			}
			// wait until tasks are completed or strand is interrupted
			void wait()
			{
				while (!_service->_tasks.empty() && !_isInterrupted.load())
					std::this_thread::sleep_for(_service->_wait_timeout);
			}

			// interrupt all operations as run and wait
			void interrupt()
			{
				_isInterrupted.store(true);
			}

		private:
			io_service *_service;
			std::atomic<bool> _isInterrupted;
		};

	private:
		void _run_task()
		{
			auto &task = _tasks.front();
			task();
			_tasks.pop();
		}
		container::queue<CompletionHandlerT> _tasks;
		std::chrono::milliseconds _wait_timeout;
	};
}

template<class ContainerT>
ContainerT PrepareContainer(size_t size, std::function<typename ContainerT::value_type()> &fillerFunc)
{
	ContainerT container;

	container.resize(size);
	std::generate(container.begin(), container.end(), fillerFunc);

	return std::move(container);
}

template<class ContainerT, class TesteeFuncT>
algorithm::info<double> TestAndGetAverageInfo(size_t containerSize, size_t repeats, TesteeFuncT testee, std::function<typename ContainerT::value_type()> containerFiller)
{
	auto container = PrepareContainer<ContainerT>(containerSize, containerFiller);
	std::vector<algorithm::info<uint64_t>> infos(repeats);
	std::generate(infos.begin(), infos.end(), [&testee, &container, containerSize, &containerFiller]()
	{
		std::random_shuffle(container.begin(), container.end());
		algorithm::info<uint64_t> info{ 0,0 };
		testee(info, container.begin(), container.end(), [](auto first, auto second) { return *first > *second; });
		return info;
	});

	algorithm::info<double> averageInfo{ 0,0 };
	for (auto const &info : infos)
	{
		averageInfo.comparers += info.comparers;
		averageInfo.swaps += info.swaps;
	}
	averageInfo.comparers /= infos.size();
	averageInfo.swaps /= infos.size();

	return averageInfo;
};

int main()
{
	std::string fileName = "./output.csv";
	auto ostream = ptr<std::ofstream>(new std::ofstream(fileName), [](std::ofstream* out) { if(out->is_open()) out->close(); delete out; });
	if (!ostream->is_open())
	{
		std::cout << "An error occurred while opening a file \"" << std::experimental::filesystem::current_path() << "\\" << fileName << "\"\n";
		system("pause");
		throw std::exception("An error occurred while opening a file");
	}

	std::srand(unsigned(time(0)));
	auto filler = []() noexcept { return rand() % 2000 - 1000; };
	auto testee = [](auto &info, auto begin, auto end, auto pred) { algorithm::sort::Insertion(info, begin, end, pred); };

	size_t repeats = 5;

	asio::io_service testService;
	asio::io_service::strand test(&testService);
	auto strandThreadFunc = [](asio::io_service::strand *strand) { strand->run(); };

	auto testAndOutputResultToFile = [](size_t containerSize, size_t repeats, auto testee, auto containerFiller, std::ofstream *out)
	{
		auto averageInfo = TestAndGetAverageInfo<std::vector<int64_t>>(containerSize, repeats, testee, containerFiller);
		(*out) << containerSize << "; " << (averageInfo.comparers + averageInfo.swaps) << '\n';
	};

	auto testThread = ptr<std::thread>(new std::thread(std::bind(strandThreadFunc, &test)), [](std::thread* thread) { if (thread->joinable()) thread->join(); delete thread; });


	for (size_t arraySizeForTest = 100; arraySizeForTest <= 10000; arraySizeForTest+=100)
		test.post(std::bind(testAndOutputResultToFile, arraySizeForTest, repeats, testee, filler, ostream.get()));

	test.wait();// wait untill tasks are completed
	test.interrupt();// stop test thread

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}
