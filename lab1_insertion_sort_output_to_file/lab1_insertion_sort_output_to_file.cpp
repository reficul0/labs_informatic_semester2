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

std::mutex outputToFile;// todo нужно только мультитридовому
std::mutex progressUpdate;

template<typename FuncT>
void CalcAverage(FuncT &&func, size_t size, std::ostream &ostream) noexcept
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
}

namespace executors
{
	class Executor
	{
	protected:
		std::string const _id;
		std::shared_ptr<std::ofstream> _ostream;
		std::shared_ptr<std::unordered_map<std::string, float>> _progresses;

		std::tuple<size_t, size_t> _borders;
	public:
		template<typename StringT>
		Executor(
			StringT &&id,
			std::shared_ptr<std::unordered_map<std::string, float>> const &progresses,
			std::tuple<size_t, size_t> const &borders
		)
			: _id(std::forward<StringT>(id))
			, _progresses(progresses)
			, _borders(borders)
		{
			std::string fileName = "output" + _id + ".csv";
			_ostream = std::make_shared<std::ofstream>(fileName, std::ifstream::out);
			if (!_ostream->is_open())
			{
				std::cout << "An error occurred while opening a file \"" << std::experimental::filesystem::current_path() << "\\" << fileName << "\"\n";
				system("pause");
				throw std::exception("An error occurred while opening a file");
			}

			_progresses->emplace(id, 0);
		}
		~Executor()
		{
			if (_ostream && _ostream->is_open())
				_ostream->close();
		}
	};

	class Sync : Executor
	{
	public:
		template<typename StringT>
		Sync(
			StringT &&id,
			std::shared_ptr<std::unordered_map<std::string, float>> const &progresses,
			std::tuple<size_t, size_t> const &borders
		)
			: Executor(std::forward<StringT>(id), progresses, borders)
		{
		}

		template<typename FuncT>
		void Execute(FuncT &&func)
		{
			_progresses->at(_id) = 0;

			size_t first = std::get<0>(_borders),
					last = std::get<1>(_borders);

			for (size_t size = first; size <= last; size += 100)
			{
				CalcAverage(std::forward<FuncT>(func), size, *_ostream.get());

				// todo dry
				std::lock_guard<std::mutex> progressUpdateLock(progressUpdate);
				system("cls");
				_progresses->at(_id) += 0.01;
				Log::Progress(*_progresses.get());
			}
		}
	};

	class Async : Executor
	{
	public:
		template<typename StringT>
		Async(
			StringT &&id,
			std::shared_ptr<std::unordered_map<std::string, float>> const &progresses,
			std::tuple<size_t, size_t> const &borders
		)
			: Executor(std::forward<StringT>(id), progresses, borders)
		{
		}

		template<typename FuncT>
		void Execute(FuncT &func)
		{
			_progresses->at(_id) = 0;

			size_t bottom = std::get<0>(_borders), 
					top = std::get<1>(_borders);

			std::ostream &ostream = *_ostream;
			std::unordered_map<std::string, float> &progresses = *_progresses.get();

			using ThreadPool = std::queue<std::thread>;
			ThreadPool threadPool;
			for (size_t size = bottom; size <= top; size += 100)
				threadPool.push(std::thread(&CalcAverage<FuncT>, func, size, std::ref(ostream)));


			while (!threadPool.empty())
			{
				threadPool.front().join();

				std::lock_guard<std::mutex> progressUpdateLock(progressUpdate);
				system("cls");
				_progresses->at(_id) += 0.01;// todo вычисление нормального процента
				Log::Progress(*_progresses.get());

				threadPool.pop();
			}
		}
	};
}

int main()
{
	std::vector<int64_t> arr;

	std::srand(unsigned(time(0)));

	using namespace std::chrono;

	std::tuple<size_t, size_t> borders(100, 10000);
	std::shared_ptr<std::unordered_map<std::string, float>> progresses = std::make_shared<std::unordered_map<std::string, float>>();
	auto testee = [](auto &info, auto begin, auto end, auto pred) { Sort::Insertion(info, begin, end, pred); };

	executors::Sync syncExecutor("sync_impl", progresses, borders);

	high_resolution_clock::time_point beginTime = high_resolution_clock::now();
	syncExecutor.Execute(testee);
	std::chrono::duration<double, std::milli> syncImplDuration = beginTime - high_resolution_clock::now();

	executors::Async asyncExecutor("async_impl", progresses, borders);

	beginTime = high_resolution_clock::now();
	asyncExecutor.Execute(testee);
	std::chrono::duration<double, std::milli> asyncImplDuration = beginTime - high_resolution_clock::now();

	std::cout << "\Sync implementation duration: " << syncImplDuration.count() << " ms.\n";
	std::cout << "Async implementation duration: " << asyncImplDuration.count() << " ms.\n\n";
	std::cout << "Results in file \"" << std::experimental::filesystem::current_path() << "\\output?impl_name?.txt\"\n\n";

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}
