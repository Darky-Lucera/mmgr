#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "check.h"

struct Base {
    virtual ~Base() {
    }

    int a = 0;
};

struct Derived : Base {
    int b[8] = {};
};

struct Second {
    virtual ~Second() {
    }

    int c = 0;
};

struct Multiple : Base, Second {
    int d[3] = {};
};

struct NonTrivial {
    ~NonTrivial() {
    }

    int x = 0;
};

struct Empty {};

struct alignas(64) AlignedNonTrivial {
    ~AlignedNonTrivial() {
    }

    char data[64];
};

// Every delete here is correct, so the check of the sized delete must not report anything.
int
main() {
    {
        std::vector<std::string> strings;
        for (int i = 0; i < 1000; ++i) {
            strings.push_back(std::string(i % 50, 'x'));
        }

        std::map<int, std::vector<int>> map;
        for (int i = 0; i < 200; ++i) {
            map[i].resize(i);
        }

        std::list<int>          list(100, 1);
        std::function<size_t()> function = [strings]() {
            return strings.size();
        };
        std::shared_ptr<Derived>       shared = std::make_shared<Derived>();
        std::unique_ptr<Base>          unique(new Derived);
        std::vector<int>               big(100000);
        std::vector<AlignedNonTrivial> aligned(10);

        unique.reset();
        CHECK(function() == strings.size());
    }

    Base * derived = new Derived;
    delete derived;

    Second * multiple = new Multiple;
    delete multiple;

    NonTrivial * one = new NonTrivial;
    delete one;

    NonTrivial * array = new NonTrivial[7];
    delete[] array;

    NonTrivial * none = new NonTrivial[0];
    delete[] none;

    int * ints = new int[7];
    delete[] ints;

    Empty * empty = new Empty;
    delete empty;

    AlignedNonTrivial * alignedOne = new AlignedNonTrivial;
    delete alignedOne;

    AlignedNonTrivial * alignedArray = new AlignedNonTrivial[3];
    delete[] alignedArray;

    CHECK(loggedErrors() == 0);
    CHECK(liveBlocks() == 0);
    return checkFailures;
}
