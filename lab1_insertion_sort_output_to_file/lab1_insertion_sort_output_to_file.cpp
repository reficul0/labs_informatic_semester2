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
#include <deque>
#include <shared_mutex>

#include <boost/thread.hpp>

#include "../include/sort.h"
#include "../include/log.h"
#include "../include/ptr.h"


namespace reficul
{
	namespace asio
	{
		namespace container
		{
			// потокобезопасный дэк
			template<class _Ty,
				class _Container = std::deque<_Ty> >
				class deque
				: std::deque<_Ty, _Container>
			{
				using baseT = typename std::deque<_Ty, _Container>;
			public:
				bool empty() const
				{
					_STD shared_lock<_STD shared_mutex> lock(_mtx);
					return (baseT::empty());
				}

				void push_back(typename baseT::value_type&& _Val)
				{
					_STD unique_lock<_STD shared_mutex> lock(_mtx);
					baseT::push_back(_STD move(_Val));
				}
				void push(typename baseT::value_type const &_Val)
				{
					_STD unique_lock<_STD shared_mutex> lock(_mtx);
					baseT::push_back(_STD move(_Val));
				}

				void pop()
				{
					_STD unique_lock<_STD shared_mutex> lock(_mtx);
					baseT::pop();
				}

				typename baseT::reference front()
				{
					_STD shared_lock<_STD shared_mutex> lock(_mtx);
					return (baseT::front());
				}
				typename baseT::const_reference front() const
				{
					_STD shared_lock<_STD shared_mutex> lock(_mtx);
					return (baseT::front());
				}

				typename baseT::iterator begin() _NOEXCEPT
				{
					_STD shared_lock<_STD shared_mutex> lock(_mtx);
					return baseT::begin();
				}

				typename baseT::const_iterator begin() const _NOEXCEPT
				{
					_STD shared_lock<_STD shared_mutex> lock(_mtx);
					return baseT::begin();
				}

				typename baseT::iterator end() _NOEXCEPT
				{
					_STD shared_lock<_STD shared_mutex> lock(_mtx);
					return baseT::end();
				}

				typename baseT::const_iterator end() const _NOEXCEPT
				{
					_STD shared_lock<_STD shared_mutex> lock(_mtx);
					return baseT::end();
				}
			private:
				mutable _STD shared_mutex _mtx;
			};
		}
	}
}

template<class ContainerT>
ContainerT PrepareContainer(size_t size, std::function<typename ContainerT::value_type()> &fillerFunc)
{
	ContainerT container;

	container.resize(size);
	std::generate(container.begin(), container.end(), fillerFunc);

	return container;// не перемещаем т.к. nrvo
}

template<class ContainerT, class TesteeFuncT>
algorithm::info<double> TestAndGetAverageInfo(size_t containerSize, size_t testRepeats, TesteeFuncT testee, std::function<typename ContainerT::value_type()> containerFiller)
{
	auto container = PrepareContainer<ContainerT>(containerSize, containerFiller);
	std::vector<algorithm::info<uint64_t>> infos(testRepeats);
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

struct ResultsStreamSupport
	: public std::pair<size_t, algorithm::info<double>>
{
	using baseT = typename std::pair<size_t, algorithm::info<double>>;
public:
	ResultsStreamSupport(baseT const& from) noexcept
	{
		this->first = from.first;
		this->second = from.second;
	}
	friend std::ostream &operator << (std::ostream &out, baseT const &dest)
	{
		return out << dest.first << "; " << (dest.second.comparers + dest.second.swaps);
	}
};

int main()
{
	std::string fileName = "./output.csv";
	auto ostream = ptr<std::ofstream>(
		new std::ofstream(fileName), 
		[](std::ofstream* out) { if (out->is_open()) out->close();  delete out; }
	);// RAII
	if (!ostream->is_open())
	{
		std::cout << "An error occurred while opening a file \"" << std::experimental::filesystem::current_path() << "\\" << fileName << "\"\n";
		system("pause");
		throw std::exception("An error occurred while opening a file");
	}

	std::srand(unsigned(time(0)));
	auto filler = []() noexcept { return rand() % 2000 - 1000; };
	auto testee = [](auto &info, auto begin, auto end, auto pred) { algorithm::sort::Insertion(info, begin, end, pred); };

	size_t const 
		containerSizeUpperBound = 10000,
		fromTestToTestSizeDiff = 100;

	auto testAndStoreResult = [](size_t containerSize, size_t testRepeats, auto testee, auto containerFiller, auto &results)
	{
		auto info = TestAndGetAverageInfo<std::vector<int64_t>>(containerSize, testRepeats, testee, containerFiller);
		results.push(std::make_pair(containerSize, info));
	};
	auto testBlockAndStoreResults = [fromTestToTestSizeDiff, &testAndStoreResult](size_t begin_inclusive, size_t end_notInclusive, size_t testRepeats, auto testee, auto containerFiller, auto &results)
	{
		// Первыми на исполнение отправим самые долгие тесты, за счет чего выиграем во времени ожидания
		for (auto current = end_notInclusive - fromTestToTestSizeDiff; current >= begin_inclusive; current -= fromTestToTestSizeDiff)
			testAndStoreResult(current, testRepeats, testee, containerFiller, results);
	};
	
	/* 
		Вычислять будем асинхронно, ибо синхронно - долго.
		Чтрбы асинхронность не была мелвежей услугой, будем учитывать количество потоков, поддерживаемых машиной.
		Иначе будет происходить рост временени, затрачиваемого на контекстные переключения, что приведет к потере производительности.
	*/
	size_t const 
		testsCount = containerSizeUpperBound / fromTestToTestSizeDiff,
		testsCountPerThreadLimit = 5,
		maxThreads = (testsCount + testsCountPerThreadLimit - 1) / testsCountPerThreadLimit,
		//The number of hardware threads available on the current system (e.g. number of CPUs or cores or hyperthreading units), or 0 if this information is not available. 
		hardwareThreadsLimit = boost::thread::hardware_concurrency();

	size_t const
		optimalThreadsCount = std::min(hardwareThreadsLimit ? hardwareThreadsLimit : 2, maxThreads),
		testsPerThread = testsCount / optimalThreadsCount;
	bool const doAvailableThreadsCoverAllTests = !(testsCount % optimalThreadsCount);

	reficul::asio::container::deque<std::pair<size_t, algorithm::info<double>>> results;
	{
		auto testers = ptr<boost::thread_group>(new boost::thread_group(), [](boost::thread_group* threads) {
			threads->interrupt_all(); threads->join_all(); delete threads;
		});// RAII

		size_t threadFirstTestContainerSize = fromTestToTestSizeDiff, const testRepeats = 5;
		for (size_t threadId = 0; threadId < optimalThreadsCount; ++threadId)
		{
			size_t threadLastTestContainerSize = threadFirstTestContainerSize + testsPerThread * fromTestToTestSizeDiff;
			testers->create_thread(
				std::bind(
					testBlockAndStoreResults, 
					threadFirstTestContainerSize, 
					threadLastTestContainerSize, 
					testRepeats, 
					testee,
					filler, 
					std::ref(results)
				)
			);
			threadFirstTestContainerSize = threadLastTestContainerSize;
		}
		if(!doAvailableThreadsCoverAllTests)
			testers->create_thread(
				std::bind(
					testBlockAndStoreResults, 
					threadFirstTestContainerSize, 
					containerSizeUpperBound + fromTestToTestSizeDiff, 
					testRepeats, 
					testee, 
					filler, 
					std::ref(results)
				)
			);

	}

	// Упорядочим т.к. результаты сохранялись асинхронно.
	std::sort(results.begin(), results.end(), [](decltype(*results.begin()) lhs, decltype(*results.begin()) rhs) {
		return lhs.first < rhs.first;
	});
	std::copy( results.begin(), results.end(), std::ostream_iterator<ResultsStreamSupport>((*ostream), "\n") );

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}
