/**
 * @file test_reflect.cpp
 * @brief 反射库完整测试
 */

#include <iostream>
#include <cassert>
#include <filesystem>
#include "../reflect/reflect.hpp"
#include "test.h"


 //using namespace reflect;
using reflect::Reflection;
using reflect::serialize_binary;
using reflect::deserialize_binary;
using reflect::serialize_to_file;
using reflect::deserialize_from_file;

// ============================================================================
// 定义测试类
// ============================================================================

struct Address {
    std::string street;
    std::string city;
    std::shared_ptr<uint32_t> zipcode;

    // 默认构造函数
    Address() = default;

    // 带参数的构造函数（用于初始化）
    Address(std::string s, std::string c, std::shared_ptr<uint32_t> z)
        : street(std::move(s)), city(std::move(c)), zipcode(std::move(z)) {
    }

    void set_zip(uint32_t z) { *zipcode = z; }
    uint32_t get_zip() const { return *zipcode; }
    std::string full_address() const { return street + ", " + city; }
};

REFLECT_VARS(Address, &Address::street, &Address::city, &Address::zipcode);
REFLECT_FUNCS(Address, &Address::set_zip, &Address::get_zip, &Address::full_address);

struct Person {
    std::string name;
    int age;
    double salary;
    std::vector<Address> addresses;

    void birthday() { ++age; }
    double calc_tax(double rate) const { return salary * rate; }
    std::string introduce() const { return "I am " + name + ", " + std::to_string(age) + " years old"; }
    void add_address(const Address& addr) { addresses.push_back(addr); }
};

REFLECT_VARS(Person, &Person::name, &Person::age, &Person::salary, &Person::addresses);
REFLECT_FUNCS(Person, &Person::birthday, &Person::calc_tax, &Person::introduce, &Person::add_address);

// ============================================================================
// 测试函数
// ============================================================================

void test_basic_reflection() {
    std::cout << "=== Test 1: Basic Reflection Info ===" << std::endl;

    constexpr auto var_names = reflect::detail::member_names<Person>();
    static_assert(var_names.size() == 4);
    static_assert(var_names[0] == "name");
    static_assert(var_names[1] == "age");
    static_assert(var_names[2] == "salary");
    static_assert(var_names[3] == "addresses");

    constexpr auto func_names = reflect::detail::function_names<Person>();
    static_assert(func_names.size() == 4);
    static_assert(func_names[0] == "birthday");
    static_assert(func_names[1] == "calc_tax");
    static_assert(func_names[2] == "introduce");
    static_assert(func_names[3] == "add_address");

    std::cout << "Person has " << reflect::detail::member_count<Person>::value << " member variables" << std::endl;
    std::cout << "Person has " << reflect::detail::function_count<Person>::value << " member functions" << std::endl;
    std::cout << "[ok] Basic reflection info correct" << std::endl;
}

void test_dynamic_string_call() {
    std::cout << "\n=== Test 2: Dynamic String-based Call ===" << std::endl;

    Person p{ "李四", 30, 20000.0, {} };

    Reflection<Person>::call(&p, "birthday");
    std::cout << "After birthday: age = " << p.age << " (expected 31)" << std::endl;
    assert(p.age == 31);

    std::any tax_result = Reflection<Person>::call(&p, "calc_tax", 0.2);
    double tax = std::any_cast<double>(tax_result);
    std::cout << "Tax (20% of 20000): " << tax << " (expected 4000)" << std::endl;
    assert(tax == 4000.0);

    std::any intro = Reflection<Person>::call(&p, "introduce");
    std::string intro_str = std::any_cast<std::string>(intro);
    std::cout << "Introduction: " << intro_str << std::endl;

    auto tax2 = Reflection<Person>::static_call<"calc_tax">(&p, 0.1);
    std::cout << "Tax (10% of 20000): " << tax2 << " (expected 2000)" << std::endl;

    std::cout << "[ok] Dynamic string call works" << std::endl;
}

void test_serialization() {
    std::cout << "\n=== Test 3: Binary Serialization ===" << std::endl;

    Address addr1{ "长安街1号", "北京", std::make_shared<uint32_t>(100000) };
    Address addr2{ "南京路2号", "上海", std::make_shared<uint32_t>(200000) };

    Person p{ "王五", 25, 12000.0, {} };
    p.addresses.push_back(std::move(addr1));
    p.addresses.push_back(std::move(addr2));

    auto data = serialize_binary(p);
    std::cout << "Serialized size: " << data.size() << " bytes" << std::endl;

    Person p2 = deserialize_binary<Person>(data);
    std::cout << "De serialized: " << p2.name << ", " << p2.age << " years old" << std::endl;
    std::cout << "Addresses count: " << p2.addresses.size() << std::endl;

    assert(p2.name == p.name);
    assert(p2.age == p.age);
    assert(p2.addresses.size() == 2);
    assert(*p2.addresses[0].zipcode == 100000);

    std::cout << "[ok] Serialization works" << std::endl;
}

void test_file_io() {
    std::cout << "\n=== Test 4: File I/O ===" << std::endl;

    std::string test_file = (std::filesystem::current_path() / "test_person.bin").string();

    Person p{ "赵六", 35, 30000.0, {} };
    p.addresses.push_back(Address{ "测试路", "测试市", std::make_shared<uint32_t>(12345) });

    serialize_to_file(p, test_file);
    std::cout << "Written to " << test_file << std::endl;

    Person p2 = deserialize_from_file<Person>(test_file);
    std::cout << "Read from file: " << p2.name << ", " << p2.age << " years old" << std::endl;

    assert(p2.name == p.name);
    assert(p2.salary == p.salary);

    std::filesystem::remove(test_file);

    std::cout << "[ok] File I/O works" << std::endl;
}

void test_address_reflection() {
    std::cout << "\n=== Test 5: Address Class Reflection ===" << std::endl;

    Address addr{ "测试街", "测试市", std::make_shared<uint32_t>(99999) };

    std::any zip1 = Reflection<Address>::call(&addr, "get_zip");
    std::cout << "Original zip: " << std::any_cast<uint32_t>(zip1) << std::endl;

    Reflection<Address>::call(&addr, "set_zip", uint32_t(88888));

    std::any zip2 = Reflection<Address>::call(&addr, "get_zip");
    std::cout << "New zip: " << std::any_cast<uint32_t>(zip2) << " (expected 88888)" << std::endl;

    std::any full = Reflection<Address>::call(&addr, "full_address");
    std::cout << "Full address: " << std::any_cast<std::string>(full) << std::endl;

    std::cout << "[ok] Address reflection works" << std::endl;
}

void test_error_handling() {
    std::cout << "\n=== Test 6: Error Handling ===" << std::endl;

    Person p{ "测试", 20, 1000.0, {} };

    try {
        Reflection<Person>::call(&p, "nonexistent");
        assert(false);
    }
    catch (const std::runtime_error& e) {
        std::cout << "[ok] Caught expected error: " << e.what() << std::endl;
    }

    try {
        Reflection<Person>::call(&p, "calc_tax");
        assert(false);
    }
    catch (const std::runtime_error& e) {
        std::cout << "[ok] Caught arg count error: " << e.what() << std::endl;
    }

    try {
        auto p2 = deserialize_from_file<Person>("nonexistent_file.bin");
        assert(false);
    }
    catch (const std::runtime_error& e) {
        std::cout << "[ok] Caught file error: " << e.what() << std::endl;
    }
}


struct TestA
{
    int a = 1;
    int b = 2;

    virtual ~TestA() {}
    virtual void foo() const {
        std::cout << "TestA::foo" << std::endl;
    }
};

REFLECT_VARS(TestA, &TestA::a, &TestA::b);
REFLECT_FUNCS(TestA, &TestA::foo);

struct TestB : public TestA
{
    virtual ~TestB() {}
    virtual void foo() const override {
        std::cout << "TestB::foo" << std::endl;
    }
};

REFLECT_FUNCS(TestB, &TestB::foo);

void test_call_virtual() {
    std::cout << "\n=== Test 7: Call Virtual Method ===" << std::endl;

    TestA a;
    TestB b;

    std::cout << "static_call virtual method: " << std::endl;
    Reflection<TestA>::static_call<"foo">(a);
    Reflection<TestA>::static_call<"foo">(b);
    Reflection<TestB>::static_call<"foo">(b);

    std::cout << "call virtual method: " << std::endl;
    Reflection<TestA>::call(&a, "foo");
    Reflection<TestA>::call(&b, "foo");
    Reflection<TestB>::call(&b, "foo");

    std::cout << "[ok] Virtual method call works" << std::endl;
}

void test_call_typed()
{
    std::cout << "\n=== Test 8: Call Typed ===" << std::endl;

    Person p{ "王五", 36, 50000.0, {} };

    std::cout << "static_call virtual method: " << std::endl;
    Reflection<Person>::call_typed<void>(p, "birthday");

    double v1 = Reflection<Person>::call_typed<double>(p, "calc_tax", 0.5);
    std::cout << "Reflection<Person>::call_typed<double>(p, calc_tax, 0.5): " << v1 << "\n";

    std::string s1 = Reflection<Person>::call_typed<std::string>(p, "introduce");
    std::cout << "Reflection<Person>::call_typed<std::string>(p, introduce): " << s1 << "\n";


    std::cout << "[ok] Call Typed works" << std::endl;
}


int test_reflection_main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Reflect-Serializer Library Test Suite" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        test_basic_reflection();
        test_dynamic_string_call();
        test_serialization();
        test_file_io();
        test_address_reflection();
        test_error_handling();
        test_call_virtual();
        test_call_typed();

        std::cout << "\n========================================" << std::endl;
        std::cout << "All tests passed! [ok]" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
