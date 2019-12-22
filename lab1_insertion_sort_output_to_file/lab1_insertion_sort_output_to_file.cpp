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

using TestContainerT = std::vector<int64_t>;

namespace Algorithm
{
	namespace Test
	{
		using AlgorithmT = std::function<
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

		class AverageCalculator
		{
			// Class contains container-specific logic.
		public:
			using FillierT = std::function<typename TestContainerT::value_type()>;
		private:
			size_t _repeats;
			FillierT _filler;

			template<class ContainerT>
			inline ContainerT GetContainer(size_t size)
			{
				ContainerT container;

				container.resize(size);
				std::generate(container.begin(), container.end(), _filler);

				return std::move(container);
			}
		public:
			template<typename FillingFuncT>
			AverageCalculator(size_t repeats, FillingFuncT &&filler)
				: _repeats(repeats)
				, _filler(std::forward<FillingFuncT>(filler))
			{
			}

			template<class ContainerT>
			void Calculate(AlgorithmT &func, size_t containerSize, std::promise< std::tuple<size_t, float> > &&result)
			{
				std::vector<Algorithm::Info<uint64_t>> infos(_repeats);
				for (size_t i(0); i < infos.size(); ++i)
				{
					auto container(GetContainer<ContainerT>(containerSize));

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
		protected:
			using ExecutionResultT = std::tuple<size_t, float>;

			AverageCalculator _calculator;

			std::string const _id;
			std::shared_ptr<std::ofstream> _ostream;
			std::shared_ptr<Log::ProgressesT> _progresses;

			BordersT _borders;
			size_t _stepSize;

			mutable std::mutex _outputToFile;

			void _PutOutResult(ExecutionResultT &executeResult)
			{
				std::lock_guard<std::mutex> outputToFileLock(_outputToFile);
				(*_ostream.get()) << std::get<0>(executeResult) << "; " << std::get<1>(executeResult) << '\n';
			}
			virtual void _UpdateProgresses(ExecutionResultT &executeResult)
			{
				_progresses->at(_id) += (double)_stepSize / std::get<1>(_borders);
				system("cls");
				Log::Progress(*_progresses.get());
			}
			void _OnProgress(ExecutionResultT executeResult)
			{
				_PutOutResult(executeResult);
				_UpdateProgresses(executeResult);
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

			virtual void Execute(AlgorithmT&) = 0;
		};
		class SyncExecutor : public Executor
		{
		public:
			template<typename StringT, typename CalculatorT>
			SyncExecutor(
				StringT &&id,
				std::shared_ptr<Log::ProgressesT> const &progresses,
				BordersT const &borders,
				CalculatorT &&calculator
			)
				: Executor(std::forward<StringT>(id), progresses, borders, std::forward<CalculatorT>(calculator))
			{
			}

			void Execute(AlgorithmT &func) override
			{
				_progresses->at(_id) = 0;

				size_t bottom = std::get<0>(_borders),
					top = std::get<1>(_borders);

				for (size_t size = bottom; size <= top; size += _stepSize)
				{
					std::promise<ExecutionResultT> promise;
					auto future = promise.get_future();
					_calculator.Calculate<TestContainerT>(func, size, std::move(promise));
					_OnProgress(future.get());
				}
			}
		};
		class AsyncExecutor : public Executor
		{
			std::mutex _progressUpdate;
		protected:
			void _UpdateProgresses(ExecutionResultT &executeResult) override
			{
				std::lock_guard<std::mutex> progressUpdateLock(_progressUpdate);
				Executor::_UpdateProgresses(executeResult);
			}
		public:
			template<typename StringT, typename CalculatorT>
			AsyncExecutor(
				StringT &&id,
				std::shared_ptr<Log::ProgressesT> const &progresses,
				BordersT const &borders,
				CalculatorT &&calculator
			)
				: Executor(std::forward<StringT>(id), progresses, borders, std::forward<CalculatorT>(calculator))
			{
			}

			void Execute(AlgorithmT &func) override
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
								&AverageCalculator::Calculate<TestContainerT>,
								&_calculator, std::ref(func), size, std::move(promise)
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

		class ExecutionBenchmark
		{
			Executor::Ptr _excecutor;
		public:
			ExecutionBenchmark(Executor::Ptr excecutor)
				: _excecutor(excecutor)
			{}

			std::chrono::duration<double, std::milli> operator()(Test::AlgorithmT &func)
			{
				using namespace std::chrono;
				high_resolution_clock::time_point beginTime = high_resolution_clock::now();
				_excecutor->Execute(func);
				return beginTime - high_resolution_clock::now();
			}
			void Reset(Executor::Ptr excecutor = nullptr)
			{
				_excecutor = excecutor;
			}
		};
	}
}


int main()
{
	std::srand(unsigned(time(0)));

	using namespace Algorithm::Test;

	Executor::BordersT borders(100, 10000);
	std::shared_ptr<Log::ProgressesT> progresses = std::make_shared<Log::ProgressesT>();
	AverageCalculator::FillierT filler = []() noexcept { return rand() % 2000 - 1000; };
	AlgorithmT testee = [](auto &info, auto begin, auto end, auto pred) { Algorithm::Sort::Insertion(info, begin, end, pred); };

	auto syncImplDuration  = ExecutionBenchmark(std::make_shared<SyncExecutor>("sync_impl", progresses, borders, AverageCalculator(5, filler)))(testee);
	auto asyncImplDuration = ExecutionBenchmark(std::make_shared<AsyncExecutor>("async_impl", progresses, borders, AverageCalculator(5, filler)))(testee);

	std::cout << "\Sync implementation duration: " << syncImplDuration.count() << " ms.\n";
	std::cout << "Async implementation duration: " << asyncImplDuration.count() << " ms.\n\n";
	std::cout << "Results in file \"" << std::experimental::filesystem::current_path() << "\\output?impl_name?.txt\"\n\n";

	std::cout << "Variant 10 Insertion sort.\n\n";

	system("pause");
}
