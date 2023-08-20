#pragma once

#include <iostream>
#include <vector>

template<typename T>
struct BinomialDistrib {
	T prob;
	uint32_t failId;
};

/**
* Transform a discrete distribution to a set of binomial distributions
*   so that an O(1) sampling approach can be applied
*/
template<typename T>
struct DiscreteSampler1D {
	using DistribT = BinomialDistrib<T>;

	DiscreteSampler1D() = default;

	DiscreteSampler1D(std::vector<T> values) {
		binomDistribs.resize(values.size() + 1);
		T sumAll = static_cast<T>(0);

		for (const auto& val : values) {
			sumAll += val;
		}
		T sumInv = static_cast<T>(values.size()) / sumAll;

		for (auto& val : values) {
			val *= sumInv;
		}

		std::vector<DistribT> stackGtOne(values.size() * 2);
		std::vector<DistribT> stackLsOne(values.size() * 2);
		int topGtOne = 0;
		int topLsOne = 0;

		for (int i = 0; i < values.size(); i++) {
			auto& val = values[i];
			(val > static_cast<T>(1) ? stackGtOne[topGtOne++] : stackLsOne[topLsOne++]) = DistribT{ val, i + 1 };
		}

		while (topGtOne && topLsOne) {
			DistribT gt = stackGtOne[--topGtOne];
			DistribT ls = stackLsOne[--topLsOne];

			binomDistribs[ls.failId] = DistribT{ ls.prob, gt.failId };
			// Place ls in the table, and "fill" the rest of probability with gt.prob
			gt.prob -= (static_cast<T>(1) - ls.prob);
			// See if gt.prob is still greater than 1 that it needs more iterations to
			//   be splitted to different binomial distributions
			(gt.prob > static_cast<T>(1) ? stackGtOne[topGtOne++] : stackLsOne[topLsOne++]) = gt;
		}

		for (int i = topGtOne - 1; i >= 0; i--) {
			DistribT gt = stackGtOne[i];
			binomDistribs[gt.failId] = gt;
		}

		for (int i = topLsOne - 1; i >= 0; i--) {
			DistribT ls = stackLsOne[i];
			binomDistribs[ls.failId] = ls;
		}
		binomDistribs[0] = { sumAll, values.size() };
	}

	void clear() {
		binomDistribs.clear();
	}

	uint32_t sample(float r1, float r2) {
		uint32_t passId = uint32_t(float(binomDistribs.size()) * r1);
		DistribT distrib = binomDistribs[passId];
		return (r2 < distrib.prob) ? passId : distrib.failId;
	}

	std::vector<DistribT> binomDistribs;
};

template<typename T>
struct DiscreteSampler2D {
	using DistribT = BinomialDistrib<T>;

	DiscreteSampler2D() = default;

	DiscreteSampler2D(const T* data, int width, int height) {
		columnSamplers.resize(height);
		std::vector<T> sumRows(height);
		std::vector<T> rowData(width);

		for (int i = 0; i < height; i++) {
			for (int j = 0; j < width; j++) {
				sumRows[i] += data[i * width + j];
			}
			float sumRowInv = static_cast<T>(1) / sumRows[i];

			for (int j = 0; j < width; j++) {
				rowData[j] = data[i * width + j] * sumRowInv;
			}
			columnSamplers[i] = DiscreteSampler1D<T>(rowData);
			sumAll += sumRows[i];
		}

		T sumAllInv = static_cast<T>(1) / sumAll;
		for (int i = 0; i < height; i++) {
			sumRows[i] *= sumAllInv;
		}
		rowSampler = DiscreteSampler1D<T>(sumRows);
	}

	void clear() {
		columnSamplers.clear();
		rowSampler.clear();
		sumAll = static_cast<T>(0);
	}

	std::pair<uint32_t, uint32_t> sample(float r1, float r2, float r3, float r4) {
		uint32_t row = rowSampler.sample(r1, r2);
		uint32_t column = columnSamplers[row].sample(r3, r4);
		return { row, column };
	}

	std::vector<DiscreteSampler1D<T>> columnSamplers;
	DiscreteSampler1D<T> rowSampler;
	T sumAll = static_cast<T>(0);
};