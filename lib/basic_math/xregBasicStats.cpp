/*
 * MIT License
 *
 * Copyright (c) 2020 Robert Grupp
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "xregBasicStats.h"

namespace {
  // MISRA C++ compliant: Named constants instead of magic numbers
  constexpr xreg::CoordScalar kZero = 0.0;
  constexpr xreg::size_type kMinSampleCount = 1U;
}

xreg::CoordScalar xreg::SampleMean(const CoordScalarList& x)
{
  CoordScalar sum = kZero;

  for (const auto& s : x)
  {
    sum += s;
  }

  return sum / x.size();
}

xreg::CoordScalar xreg::SampleStdDev(const CoordScalarList& x)
{
  // MISRA M3: Explicit bool conversion for controlling expression
  // MISRA R9: Single exit point
  CoordScalar result = kZero;

  if (static_cast<bool>(x.size() > kMinSampleCount))
  {
    result = SampleStdDev(x, SampleMean(x));
  }

  return result;
}

xreg::CoordScalar xreg::SampleStdDev(const CoordScalarList& x, const CoordScalar sample_mean)
{
  CoordScalar sum = kZero;

  const size_type N = x.size();

  if (static_cast<bool>(N > kMinSampleCount))
  {
    for (size_type i = kZero; i < N; ++i)
    {
      // MISRA R1: Declare tmp inside loop as const
      const CoordScalar tmp = x[i] - sample_mean;
      sum += tmp * tmp;
    }

    sum = std::sqrt(sum / (N - kMinSampleCount));
  }

  return sum;
}

std::tuple<xreg::CoordScalar,xreg::CoordScalar> xreg::SampleMeanAndStdDev(const CoordScalarList& x)
{
  const CoordScalar sample_mean = SampleMean(x);

  return std::make_tuple(sample_mean, SampleStdDev(x, sample_mean));
}
