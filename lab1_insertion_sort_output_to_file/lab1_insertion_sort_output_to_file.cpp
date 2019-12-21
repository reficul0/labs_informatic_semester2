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
#include <queue>
#include <chrono>
#include <ratio>
#include <unordered_map>

#include "../include/Sort.h"
#include "../include/Log.h"

std::mutex outputToFile;
std::mutex progressUpdate;

template<typename FuncT>
void CalcAverage(FuncT &&func, size_t size,
	std::ostream &ostream, std::unordered_map<std::string, float> &progresses, std::string const &id
) noexcept
{
	std::vector<int64_t> arr;

	std::vector<Sort::Info<uint64_t>> infos(5);
	for (size_t i(0); i <= 5; ++i)
	{
		arr.resize(size);
		std::generate(arr.begin(), arr.end(), []() { return std::rand() % 1000 - 2000; });

		Sort::Info<uint64_t> info{ 0,0 };
		func(info, arr.begin(), arr.end(), [](auto first, auto second) { return *first > *second; });
		infos.push_back(info);
	}

	Sort::Info<float> averageInfo{ 0,0 };
	for (auto const &info : infos)
	{
		averageInfo.comparers += info.comparers;
		averageInfo.swaps += info.swaps;
	}
	averageInfo.comparers /= infos.size();
	averageInfo.swaps /= infos.size();

	{
		std::lock_guard<std::mutex> outputToFileLock(outputToFile);
		ostream << size << "; " << averageInfo.comparers + averageInfo.swaps << '\n';
	}

	std::lock_guard<std::mutex> progressUpdateLock(progressUpdate);
	system("cls"); 
	progresses[id] += 0.01;
	Log::Progress(progresses);
}
template<typename FuncT>
void CalcAverageOfRange(FuncT &&func, size_t first, size_t last,
	std::ostream &ostream, std::unordered_map<std::string, float> &progresses, std::string const &id
) noexcept
{
	for (size_t size = first; size <= last; size+=100)
		CalcAverage(std::forward<FuncT>(func), size, ostream, progresses, id);
}

namespace impl
{
	template<typename FuncT>
	void Fast(FuncT &&func, std::ostream &ostream, std::unordered_map<std::string, float> &progresses, std::string const &id, uint16_t threadsCount) noexcept
	{
		using ThreadPool = std::queue<std::thread>;
		ThreadPool threadPool;
		size_t stepSize = 10000 / threadsCount;
		for (size_t size(0); size < 10000; size += stepSize)
			threadPool.push(std::thread(&CalcAverageOfRange<FuncT>, std::forward<FuncT>(func), size + 100, size + stepSize, std::ref(ostream), std::ref(progresses), id));

		while (!threadPool.empty())
		{
			threadPool.front().join();
			threadPool.pop();
		}
	}
	template<typename FuncT>
	void Slow(FuncT &&func, std::ostream &ostream, std::unordered_map<std::string, float> &progresses, std::string const &id) noexcept
	{
		CalcAverageOfRange(std::forward<FuncT>(func), 100, 10000, ostream, progresses, id);
	}
}

namespace demonstration
{
	using FuntT = std::function<void(std::ostream&, std::unordered_map<std::string, float>&, std::string&)>;
	bool Demonstrate(std::unordered_map<std::string, float> &progresses, std::string myName, FuntT func)
	{
		using namespace std::chrono;
		progresses.emplace(myName, 0);

		std::string fileName = "output" + myName + ".csv";
		std::ofstream file(fileName, std::ifstream::out);
		if (!file.is_open())
		{
			std::cout << "An error occurred while opening a file \"" << std::experimental::filesystem::current_path() << "\\" << fileName << "\"\n";
			system("pause");
			return false;
		}
		auto sThread = std::thread(func, std::ref(file), std::ref(progresses), std::ref(myName));
		sThread.join();

		return true;
	}
}

int main()
{
	std::vector<int64_t> arr;

	std::srand(unsigned(time(0)));

	using namespace std::chrono;

	std::unordered_map<std::string, float> progresses;
	auto testee = [](auto &info, auto begin, auto end, auto pred) { Sort::Insertion(info, begin, end, pred); };

	high_resolution_clock::time_point beginTime = high_resolution_clock::now();
	demonstration::Demonstrate(progresses, "slow_impl", [testee](std::ostream &out, auto &progresses, auto &myName) {
		impl::Slow(testee, out, progresses, myName);
	});
	std::chrono::duration<double, std::milli> slowImplDuration = beginTime - high_resolution_clock::now();

	beginTime = high_resolution_clock::now();
	demonstration::Demonstrate(progresses, "fast_impl", [testee](std::ostream &out, auto &progresses, auto &myName) {
		static constexpr uint16_t threadsCount = 50;
		impl::Fast(testee, out, progresses, myName, threadsCount);
	});
	std::chrono::duration<double, std::milli> fastImplDuration = beginTime - high_resolution_clock::now();

	std::cout << "\nSlow implementation duration: " << slowImplDuration.count() << " ms.\n";
	std::cout << "Fast implementation duration: " << fastImplDuration.count() << " ms.\n\n";
	std::cout << "Results in file \"" << std::experimental::filesystem::current_path() << "\\output?impl_name?.txt\"\n\n";

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}
