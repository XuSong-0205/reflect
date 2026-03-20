/**
 * @file test_performance.cpp
 * @brief 反射库性能测试 - 对比直接调用、静态调用、动态调用
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include "reflect.hpp"
#include "test.h"

using namespace reflect;
using namespace std::chrono;

// ============================================================================
// 测试类定义
// ============================================================================

struct Calculator {
    double value;

    Calculator(double val = 0.0) : value(val) {}

    // 无参函数
    void reset() { value = 0.0; }

    // 单参函数
    void add(double x) { value += x; }

    // 多参函数
    double compute(double a, double b, double c) const {
        return value + a * b + c;
    }

    // 虚函数（测试多态开销）
    virtual double virtual_compute(double x) const {
        return value + x;
    }
};

REFLECT_VARS(Calculator, &Calculator::value);
REFLECT_FUNCS(Calculator, &Calculator::reset, &Calculator::add, &Calculator::compute, &Calculator::virtual_compute);

// 派生类（测试虚函数）
struct AdvancedCalculator : Calculator {
    double virtual_compute(double x) const override {
        return value * 2 + x;
    }
};

// ============================================================================
// 性能测试工具
// ============================================================================

class PerfTimer {
    high_resolution_clock::time_point start_;
    const char* name_;

public:
    explicit PerfTimer(const char* name) : start_(high_resolution_clock::now()), name_(name) {}

    ~PerfTimer() {
        auto end = high_resolution_clock::now();
        auto us = duration_cast<microseconds>(end - start_).count();
        std::cout << std::left << std::setw(40) << name_ << ": " << std::right << std::setw(10) << us << " us";
    }

    // 手动结束并返回耗时（纳秒）
    static auto measure_ns(auto&& func, int iterations) {
        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = high_resolution_clock::now();
        return duration_cast<nanoseconds>(end - start).count() / iterations;
    }
};

// ============================================================================
// 具体性能测试
// ============================================================================

void test_no_arg_call() {
    std::cout << "\n=== Test 1: No-Argument Function (reset()) ===" << std::endl;
    constexpr int ITERATIONS = 10'000'000;

    Calculator calc;

    // 1. 直接调用
    auto t1 = PerfTimer::measure_ns([&]() {
        calc.reset();
        }, ITERATIONS);

    // 2. 静态反射调用
    auto t2 = PerfTimer::measure_ns([&]() {
        Reflection<Calculator>::static_call<"reset">(calc);
        }, ITERATIONS);

    // 3. 动态反射调用
    auto t3 = PerfTimer::measure_ns([&]() {
        Reflection<Calculator>::call(calc, "reset");
        }, ITERATIONS);

    // 4. 动态反射调用（类型化快速路径）
    auto t4 = PerfTimer::measure_ns([&]() {
        Reflection<Calculator>::call_typed<void>(calc, "reset");
        }, ITERATIONS);

    std::cout << "Direct call:           " << std::setw(6) << t1 << " ns/call (" << std::fixed << std::setprecision(2) << (double)t3 / t1 << "x faster than dynamic)\n";
    std::cout << "Static call:           " << std::setw(6) << t2 << " ns/call (" << std::fixed << std::setprecision(2) << (double)t3 / t2 << "x faster than dynamic)\n";
    std::cout << "Dynamic call:          " << std::setw(6) << t3 << " ns/call\n";
    std::cout << "Dynamic typed call:    " << std::setw(6) << t4 << " ns/call (" << std::fixed << std::setprecision(2) << (double)t3 / t4 << "x faster than dynamic)\n";
}

void test_single_arg_call() {
    std::cout << "\n=== Test 2: Single-Argument Function (add(x)) ===" << std::endl;
    constexpr int ITERATIONS = 10'000'000;

    Calculator calc;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 100.0);

    // 预生成随机数，避免测量随机数生成时间
    std::vector<double> random_values;
    random_values.reserve(ITERATIONS);
    for (int i = 0; i < ITERATIONS; ++i) {
        random_values.push_back(dis(gen));
    }

    // 1. 直接调用
    auto t1 = PerfTimer::measure_ns([&]() {
        static int idx = 0;
        calc.add(random_values[idx++ % ITERATIONS]);
        }, ITERATIONS);

    // 2. 静态反射调用
    auto t2 = PerfTimer::measure_ns([&]() {
        static int idx = 0;
        Reflection<Calculator>::static_call<"add">(calc, random_values[idx++ % ITERATIONS]);
        }, ITERATIONS);

    // 3. 动态反射调用
    auto t3 = PerfTimer::measure_ns([&]() {
        static int idx = 0;
        Reflection<Calculator>::call(calc, "add", random_values[idx++ % ITERATIONS]);
        }, ITERATIONS);

    // 4. 动态反射调用（类型化快速路径）
    auto t4 = PerfTimer::measure_ns([&]() {
        static int idx = 0;
        Reflection<Calculator>::call_typed<void>(calc, "add", random_values[idx++ % ITERATIONS]);
        }, ITERATIONS);

    std::cout << "Direct call:           " << std::setw(6) << t1 << " ns/call\n";
    std::cout << "Static call:           " << std::setw(6) << t2 << " ns/call\n";
    std::cout << "Dynamic call:          " << std::setw(6) << t3 << " ns/call\n";
    std::cout << "Dynamic typed call:    " << std::setw(6) << t4 << " ns/call\n";
    std::cout << "Dynamic overhead:      " << std::setw(6) << (t3 - t1) << " ns/call (" << std::fixed << std::setprecision(1) << (double)t3 / t1 << "x slower)\n";
}

void test_multi_arg_call() {
    std::cout << "\n=== Test 3: Multi-Argument Function (compute(a,b,c)) ===" << std::endl;
    constexpr int ITERATIONS = 5'000'000;

    Calculator calc{ 100.0 };

    // 1. 直接调用
    auto t1 = PerfTimer::measure_ns([&]() {
        volatile double result = calc.compute(1.0, 2.0, 3.0);
        (void)result;
        }, ITERATIONS);

    // 2. 静态反射调用
    auto t2 = PerfTimer::measure_ns([&]() {
        volatile double result = Reflection<Calculator>::static_call<"compute">(calc, 1.0, 2.0, 3.0);
        (void)result;
        }, ITERATIONS);

    // 3. 动态反射调用
    auto t3 = PerfTimer::measure_ns([&]() {
        volatile std::any result = Reflection<Calculator>::call(calc, "compute", 1.0, 2.0, 3.0);
        (void)result;
        }, ITERATIONS);

    // 4. 动态反射调用（类型化快速路径）
    auto t4 = PerfTimer::measure_ns([&]() {
        volatile double result = Reflection<Calculator>::call_typed<double>(calc, "compute", 1.0, 2.0, 3.0);
        (void)result;
        }, ITERATIONS);

    std::cout << "Direct call:           " << std::setw(6) << t1 << " ns/call\n";
    std::cout << "Static call:           " << std::setw(6) << t2 << " ns/call\n";
    std::cout << "Dynamic call:          " << std::setw(6) << t3 << " ns/call\n";
    std::cout << "Dynamic typed call:    " << std::setw(6) << t4 << " ns/call\n";
}

void test_virtual_call() {
    std::cout << "\n=== Test 4: Virtual Function Call Overhead ===" << std::endl;
    constexpr int ITERATIONS = 10'000'000;

    AdvancedCalculator adv_calc;
    Calculator* base_ptr = &adv_calc;

    // 1. 直接虚调用
    auto t1 = PerfTimer::measure_ns([&]() {
        volatile double result = base_ptr->virtual_compute(5.0);
        (void)result;
        }, ITERATIONS);

    // 2. 静态反射调用（通过基类）
    auto t2 = PerfTimer::measure_ns([&]() {
        volatile double result = Reflection<Calculator>::static_call<"virtual_compute">(*base_ptr, 5.0);
        (void)result;
        }, ITERATIONS);

    // 3. 动态反射调用（通过基类）
    auto t3 = PerfTimer::measure_ns([&]() {
        volatile std::any result = Reflection<Calculator>::call(*base_ptr, "virtual_compute", 5.0);
        (void)result;
        }, ITERATIONS);

    std::cout << "Direct virtual call:   " << std::setw(6) << t1 << " ns/call\n";
    std::cout << "Static virtual call:   " << std::setw(6) << t2 << " ns/call\n";
    std::cout << "Dynamic virtual call:  " << std::setw(6) << t3 << " ns/call\n";
    std::cout << "Reflection overhead on virtual: " << (t2 - t1) << " ns (static), " << (t3 - t1) << " ns (dynamic)\n";
}

void test_mixed_workload() {
    std::cout << "\n=== Test 5: Mixed Workload (Simulate Real Usage) ===" << std::endl;
    constexpr int ITERATIONS = 1'000'000;

    std::vector<Calculator> calcs;
    for (int i = 0; i < 1000; ++i) {
        calcs.push_back(Calculator{ static_cast<double>(i) });
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> calc_dis(0, 999);
    std::uniform_real_distribution<> val_dis(0.0, 10.0);

    // 场景1：全部直接调用（最优情况）
    auto t1 = PerfTimer::measure_ns([&]() {
        auto& calc = calcs[calc_dis(gen)];
        calc.add(val_dis(gen));
        if (calc.value > 5000) calc.reset();
        }, ITERATIONS);

    // 场景2：全部静态调用（编译期确定）
    auto t2 = PerfTimer::measure_ns([&]() {
        auto& calc = calcs[calc_dis(gen)];
        Reflection<Calculator>::static_call<"add">(calc, val_dis(gen));
        if (calc.value > 5000) Reflection<Calculator>::static_call<"reset">(calc);
        }, ITERATIONS);

    // 场景3：动态调用（配置驱动，如从文件读取函数名）
    // 模拟：80% add, 20% reset
    auto t3 = PerfTimer::measure_ns([&]() {
        auto& calc = calcs[calc_dis(gen)];
        if (calc_dis(gen) % 5 == 0) {
            Reflection<Calculator>::call(calc, "reset");
        }
        else {
            Reflection<Calculator>::call(calc, "add", val_dis(gen));
        }
        }, ITERATIONS);

    // 场景4：动态调用 + 类型化快速路径
    auto t4 = PerfTimer::measure_ns([&]() {
        auto& calc = calcs[calc_dis(gen)];
        if (calc_dis(gen) % 5 == 0) {
            Reflection<Calculator>::call_typed<void>(calc, "reset");
        }
        else {
            Reflection<Calculator>::call_typed<void>(calc, "add", val_dis(gen));
        }
        }, ITERATIONS);

    std::cout << "Direct mixed:          " << std::setw(6) << t1 << " ns/op\n";
    std::cout << "Static mixed:          " << std::setw(6) << t2 << " ns/op\n";
    std::cout << "Dynamic mixed:         " << std::setw(6) << t3 << " ns/op\n";
    std::cout << "Dynamic typed mixed:   " << std::setw(6) << t4 << " ns/op\n";
    std::cout << "Dynamic vs Direct:     " << std::fixed << std::setprecision(1) << (double)t3 / t1 << "x slower\n";
    std::cout << "DynamicTyped vs Direct:" << std::fixed << std::setprecision(1) << (double)t4 / t1 << "x slower\n";
}

void test_compile_time_impact() {
    std::cout << "\n=== Test 6: Compile-Time vs Runtime Binding ===" << std::endl;

    // 测试：编译期字符串 vs 运行时字符串构造
    constexpr int ITERATIONS = 5'000'000;

    Calculator calc;

    // 1. 编译期确定的字符串（编译期哈希）
    auto t1 = PerfTimer::measure_ns([&]() {
        // 编译期计算，零开销
        Reflection<Calculator>::static_call<"add">(calc, 1.0);
        }, ITERATIONS);

    // 2. 运行时构造的字符串（需要哈希计算）
    std::string func_name = "add";
    auto t2 = PerfTimer::measure_ns([&]() {
        // 运行时哈希计算 + 查找
        Reflection<Calculator>::call(calc, func_name, 1.0);
        }, ITERATIONS);

    // 3. 运行时字符串字面量（哈希可缓存）
    auto t3 = PerfTimer::measure_ns([&]() {
        Reflection<Calculator>::call(calc, "add", 1.0);  // 编译器可能缓存哈希
        }, ITERATIONS);

    std::cout << "Compile-time bound:    " << std::setw(6) << t1 << " ns/call\n";
    std::cout << "Runtime string var:    " << std::setw(6) << t2 << " ns/call\n";
    std::cout << "Runtime string literal:" << std::setw(6) << t3 << " ns/call\n";
    std::cout << "String lookup cost:    " << (t2 - t1) << " ns\n";
}

// ============================================================================
// 主函数
// ============================================================================

int test_performance_main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Reflection Performance Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Compiler: " << _MSC_VER << std::endl;
    std::cout << "Platform: " << (sizeof(void*) == 8 ? "x64" : "x86") << std::endl;
    std::cout << "Iterations: 5-10 million per test" << std::endl;

    // 预热，避免冷启动影响
    {
        Calculator calc;
        for (int i = 0; i < 100000; ++i) {
            calc.add(1.0);
            Reflection<Calculator>::static_call<"add">(calc, 1.0);
            Reflection<Calculator>::call(calc, "add", 1.0);
        }
    }

    test_no_arg_call();
    test_single_arg_call();
    test_multi_arg_call();
    test_virtual_call();
    test_mixed_workload();
    test_compile_time_impact();

    std::cout << "\n========================================" << std::endl;
    std::cout << "Performance Test Complete" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}