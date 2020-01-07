#pragma once

#include <iterator>
#include <algorithm>
#include <functional>
#include <tuple>

namespace algorithm
{
	template<class AccuracyT>
	struct info
	{
		AccuracyT swaps;
		AccuracyT comparers;
	};
	namespace sort
	{
		template<class IterT, class PredicateT>
		void Insertion(IterT first, IterT last, PredicateT pred)
		{
			using namespace std;
			for (auto j(next(first)); j != last; j = next(j))
				for (auto i(j); i != first && pred(prev(i), i); i = prev(i))
					iter_swap(i, prev(i));
		}
		template<class IterT, class PredicateT, class InfoT>
		void Insertion(InfoT &info, IterT first, IterT last, PredicateT pred)
		{
			using namespace std;
			auto commitCmp = [&info](auto opRes)
			{
				return (++info.comparers, opRes);
			};

			for (auto j(next(first)); commitCmp(j != last); j = next(j))
				for (auto i(j); commitCmp(i != first) && commitCmp(pred(prev(i), i)); i = prev(i))
				{
					iter_swap(i, prev(i));
					++info.swaps;
				}
		}
	}
}