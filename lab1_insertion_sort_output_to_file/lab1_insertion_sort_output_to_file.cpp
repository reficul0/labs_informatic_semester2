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

#include "../include/Sort.h"
#include "../include/Log.h"

using TestContainerT = std::vector<int64_t>; // todo get opputurnity for user to choise from code container type

using TestFunctionT = std::function<
	void(
		Algorithm::Info<uint64_t>&,
		typename TestContainerT::iterator/*from first*/,
		typename TestContainerT::iterator/*to last*/,
		std::function</*comparer*/ 
			bool(
				typename TestContainerT::iterator,
				typename TestContainerT::iterator
			)
		>
	)
>;

std::mutex progressUpdate;// todo


namespace test
{
	class AverageCalculator
	{
		size_t _repeats;

		template<class ContainerT, typename FuncT>
		inline ContainerT GetContainer(size_t size, FuncT &&generatingFunc)
		{
			ContainerT container;

			container.resize(size);
			std::generate(container.begin(), container.end(), generatingFunc);

			return std::move(container);
		}
	public:
		AverageCalculator(size_t repeats)
			: _repeats(repeats)
		{
		}

		template<class ContainerT, typename FuncT>
		void Calculate(TestFunctionT &func, size_t containerSize, FuncT &&generatingFunc, std::promise< std::tuple<size_t, float> > &&result)
		{
			std::vector<Algorithm::Info<uint64_t>> infos(_repeats);
			for (size_t i(0); i < infos.size(); ++i)
			{
				auto container(GetContainer<ContainerT>(containerSize, std::forward<FuncT>(generatingFunc)));

				Algorithm::Info<uint64_t> info{ 0,0 };
				func(info, container.begin(), container.end(), [](auto first, auto second) { return *first > *second; });
				infos[i] = info;
			}

			Algorithm::Info<float> averageInfo{ 0,0 };
			for (auto const &info : infos)
			{
				averageInfo.comparers += info.comparers;
				averageInfo.swaps += info.swaps;
			}
			averageInfo.comparers /= infos.size();
			averageInfo.swaps /= infos.size();

			result.set_value(std::make_tuple(containerSize, averageInfo.comparers + averageInfo.swaps));
		}
	};

	class Executor
	{
	public:
		using Ptr = std::shared_ptr<Executor>;
		using BordersT = std::tuple<size_t/*first*/, size_t/*last*/>;
		using FillingFunctionT = std::function<typename TestContainerT::value_type()>;
	protected:
		using ExecutionResultT = std::tuple<size_t, float>;

		AverageCalculator _calculator;

		std::string const _id;
		std::shared_ptr<std::ofstream> _ostream;
		std::shared_ptr<Log::ProgressesT> _progresses;

		BordersT _borders;
		size_t _stepSize;

		mutable std::mutex _outputToFile;

		void _OnProgress(ExecutionResultT executeResult)
		{
			{
				std::lock_guard<std::mutex> outputToFileLock(_outputToFile);
				(*_ostream.get()) << std::get<0>(executeResult) << "; " << std::get<1>(executeResult) << '\n';
			}

			{
				std::lock_guard<std::mutex> progressUpdateLock(progressUpdate);
				system("cls");
				_progresses->at(_id) += (double)_stepSize / std::get<1>(_borders);
				Log::Progress(*_progresses.get());
			}
		}
	public:
		template<typename StringT, typename CalculatorT>
		Executor(
			StringT &&id,
			std::shared_ptr<Log::ProgressesT> const &progresses,
			BordersT const &borders,
			CalculatorT &&calculator
		)
			: _id(std::forward<StringT>(id))
			, _progresses(progresses)
			, _borders(borders)
			, _stepSize(100)
			, _calculator(std::forward<CalculatorT>(calculator))
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
		virtual ~Executor()
		{
			if (_ostream && _ostream->is_open())
				_ostream->close();
		}

		virtual void Execute(TestFunctionT&, FillingFunctionT&) = 0;
	};
	class Sync : public Executor
	{
	public:
		template<typename StringT, typename CalculatorT>
		Sync(
			StringT &&id,
			std::shared_ptr<Log::ProgressesT> const &progresses,
			BordersT const &borders,
			CalculatorT &&calculator
		)
			: Executor(std::forward<StringT>(id), progresses, borders, std::forward<CalculatorT>(calculator))
		{
		}

		void Execute(TestFunctionT &func, FillingFunctionT& filler) override
		{
			_progresses->at(_id) = 0;

			size_t bottom = std::get<0>(_borders),
				top = std::get<1>(_borders);

			for (size_t size = bottom; size <= top; size += _stepSize)
			{
				std::promise<ExecutionResultT> promise;
				auto future = promise.get_future();
				_calculator.Calculate<TestContainerT>(func, size, filler, std::move(promise));
				_OnProgress(future.get());
			}
		}
	};
	class Async : public Executor
	{
	public:
		template<typename StringT, typename CalculatorT>
		Async(
			StringT &&id,
			std::shared_ptr<Log::ProgressesT> const &progresses,
			BordersT const &borders,
			CalculatorT &&calculator
		)
			: Executor(std::forward<StringT>(id), progresses, borders, std::forward<CalculatorT>(calculator))
		{
		}

		void Execute(TestFunctionT &func, FillingFunctionT& filler) override
		{
			_progresses->at(_id) = 0;

			size_t bottom = std::get<0>(_borders),
				top = std::get<1>(_borders);

			using TaskT = std::tuple<std::thread, std::future<ExecutionResultT>>;
			using ThreadsPool = std::stack<TaskT>;

			ThreadsPool threadsPool;
			for (size_t size = top; size >= bottom; size -= _stepSize)
			{
				std::promise<ExecutionResultT> promise;
				auto future = promise.get_future();
				threadsPool.emplace(
					std::make_tuple(
						std::thread(
							&AverageCalculator::Calculate<TestContainerT, FillingFunctionT>,
							&_calculator, std::ref(func), size, filler, std::move(promise)
						),
						std::move(future)
					)
				);
			}

			while (!threadsPool.empty())
			{
				auto &task = threadsPool.top();
				std::get<0>(task).join();
				_OnProgress(std::get<1>(task).get());
				threadsPool.pop();
			}
		}
	};

	class Benchmark
	{
		Executor::Ptr _excecutor;
	public:
		Benchmark(Executor::Ptr excecutor)
			: _excecutor(excecutor)
		{}

		std::chrono::duration<double, std::milli> operator()(TestFunctionT &func, Executor::FillingFunctionT &filler)
		{
			using namespace std::chrono;
			high_resolution_clock::time_point beginTime = high_resolution_clock::now();
			_excecutor->Execute(func, filler);
			return beginTime - high_resolution_clock::now();
		}
		void Reset(Executor::Ptr excecutor = nullptr)
		{
			_excecutor = excecutor;
		}
	};
}


int main()
{
	std::srand(unsigned(time(0)));

	using namespace test;

	Executor::BordersT borders(100, 10000);
	std::shared_ptr<Log::ProgressesT> progresses = std::make_shared<Log::ProgressesT>();
	TestFunctionT testee = [](auto &info, auto begin, auto end, auto pred) { Algorithm::Sort::Insertion(info, begin, end, pred); };
	Executor::FillingFunctionT filler = []() noexcept { return rand() % 2000 - 1000; };

	auto syncImplDuration  = Benchmark(std::make_shared<Sync>("sync_impl", progresses, borders, AverageCalculator(5)))(testee, filler);
	auto asyncImplDuration = Benchmark(std::make_shared<Async>("async_impl", progresses, borders, AverageCalculator(5)))(testee, filler);

	std::cout << "\Sync implementation duration: " << syncImplDuration.count() << " ms.\n";
	std::cout << "Async implementation duration: " << asyncImplDuration.count() << " ms.\n\n";
	std::cout << "Results in file \"" << std::experimental::filesystem::current_path() << "\\output?impl_name?.txt\"\n\n";

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}
