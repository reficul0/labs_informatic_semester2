// lab1_insertion_sort_output_to_file.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
//

#include "pch.h"
#include <iostream>
#include <vector>
#include <time.h>
#include <algorithm>
#include <fstream>
#include <iterator>
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

void CalcAverage(size_t size, 
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
		Sort::Insertion(info, arr.begin(), arr.end(), [](auto first, auto second) { return *first > *second; });
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
		ostream << size << "; " << averageInfo.comparers << "; " << averageInfo.swaps << '\n';
	}

	std::lock_guard<std::mutex> progressUpdateLock(progressUpdate);
	system("cls"); 
	progresses[id] += 0.01;
	Log::Progress(progresses);
}
void CalcAverageOfRange(size_t first, size_t last, 
	std::ostream &ostream, std::unordered_map<std::string, float> &progresses, std::string const &id
) noexcept
{
	for (size_t size = first; size <= last; size+=100)
		CalcAverage(size, ostream, progresses, id);
}

namespace impl
{
	void Fast(std::ostream &ostream, std::unordered_map<std::string, float> &progresses, std::string const &id, uint16_t threadsCount) noexcept
	{
		using ThreadPool = std::queue<std::thread>;
		ThreadPool threadPool;
		size_t stepSize = 10000 / threadsCount;
		for (size_t size(0); size < 10000; size += stepSize)
			threadPool.push(std::thread(&CalcAverageOfRange, size + 100, size + stepSize, std::ref(ostream), std::ref(progresses), id));

		while (!threadPool.empty())
		{
			threadPool.front().join();
			threadPool.pop();
		}
	}
	void Slow(std::ostream &ostream, std::unordered_map<std::string, float> &progresses, std::string const &id) noexcept
	{
		CalcAverageOfRange(100, 10000, ostream, progresses, id);
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

	high_resolution_clock::time_point beginTime = high_resolution_clock::now();
	demonstration::Demonstrate(progresses, "slow_impl", [](std::ostream &out, auto &progresses, auto &myName) {
		impl::Slow(out, progresses, myName);
	});
	std::chrono::duration<double, std::milli> slowImplDuration = beginTime - high_resolution_clock::now();

	beginTime = high_resolution_clock::now();
	demonstration::Demonstrate(progresses, "fast_impl", [&](std::ostream &out, auto &progresses, auto &myName) {
		impl::Fast(out, progresses, myName, 50);
	});
	std::chrono::duration<double, std::milli> fastImplDuration = beginTime - high_resolution_clock::now();

	std::cout << "\nSlow implementation duration: " << slowImplDuration.count() << " ms.\n";
	std::cout << "Fast implementation duration: " << fastImplDuration.count() << " ms.\n\n";
	std::cout << "Results in file \"" << std::experimental::filesystem::current_path() << "\\output?impl_name?.txt\"\n\n";

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}
