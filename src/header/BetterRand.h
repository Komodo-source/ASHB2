#pragma once
#include <random>
#include <type_traits>
#include <algorithm>

namespace BetterRand
{
    using namespace std;
    template<typename T>
    using uniform_distribution_t = std::conditional_t <std::is_floating_point<T>::value,
            std::uniform_real_distribution<T>,
            std::conditional_t<std::is_integral<T>::value,
                std::uniform_int_distribution<T>,
                void>>;

    struct BERNOULI {};
    struct BINOMIAL {};
    struct POISSON {};
    struct NORMAL {};
    struct UNIFORM {};

    namespace
    {
        std::random_device rd;
        std::mt19937 gen(rd());
    };

    template<typename D=UNIFORM, typename T>
    T genNrInInterval(T low, T high)
    {
        uniform_distribution_t<T> uniform_dist(low, high);
        return uniform_dist(gen);
    }

    template<typename D, typename T>
    typename std::enable_if_t<std::is_same<D,BERNOULI>::value, bool>
    genNrInInterval(T probability)
    {
        static_assert(std::is_same<T,float>::value || std::is_same<T,double>::value,
            "Bernouli distribution needs a probability value as argument!");
        std::bernoulli_distribution bernouli(probability);
        return bernouli(gen);
    }

    template<typename D>
    typename std::enable_if_t<std::is_same<D,BINOMIAL>::value, typename std::binomial_distribution<>::result_type>
    genNrInInterval(int nrOfTrials, double probabilityDist)
    {
        std::binomial_distribution<> binomial(nrOfTrials, probabilityDist);
        return binomial(gen);
    }

    template<typename D>
    typename std::enable_if_t<std::is_same<D,POISSON>::value, typename std::poisson_distribution<>::result_type>
    genNrInInterval(double mean)
    {
        std::poisson_distribution<> poisson(mean);
        return poisson(gen);
    }

    template<typename D>
    typename std::enable_if_t<std::is_same<D,NORMAL>::value, typename std::normal_distribution<>::result_type>
    genNrInInterval(double mean = 0.0, double stddev = 1.0)
    {
        std::normal_distribution<> normal(mean, stddev);
        return normal(gen);
    }

    template<typename T>
    auto getGen(T low, T high)
    {
        return [low, high]() {
             uniform_distribution_t<T> dist(low, high);
             return dist(gen);
        };
    }

    template<typename Container>
    void shuffleContainer(Container& cont)
    {
        shuffle(cont.begin(), cont.end(), rd);
    }
};
