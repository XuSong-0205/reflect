#pragma once

/**
 * @file reflect.hpp
 * @brief 现代C++反射序列化库 - 支持成员变量和成员函数反射
 * @version C++20
 */

#ifndef REFLECT_HPP
#define REFLECT_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <variant>
#include <tuple>
#include <any>
#include <type_traits>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <array>
#include <memory>
#include <format>
#include <ranges>
#include <functional>
#include <string_view>
#include <filesystem>
#include <stdexcept>
#include <concepts>

namespace reflect {

	// ============================================================================
	// 第一部分：编译期字符串处理
	// ============================================================================

	namespace detail {

		consteval std::string_view extract_name(std::string_view sv) {
			size_t start = 0;
			while (start < sv.size() && (sv[start] == '&' || sv[start] == ' ')) ++start;

			size_t pos = sv.find("::", start);
			if (pos != std::string_view::npos) {
				pos += 2;
				while (pos < sv.size() && sv[pos] == ' ') ++pos;
				return sv.substr(pos);
			}
			return sv.substr(start);
		}

		template<size_t N>
		consteval auto split_names(std::string_view str) {
			std::array<std::string_view, N> result{};
			size_t idx = 0, start = 0;
			for (size_t i = 0; i <= str.size(); ++i) {
				if (i == str.size() || str[i] == ',') {
					result[idx++] = extract_name(str.substr(start, i - start));
					start = i + 1;
					while (start < str.size() && str[start] == ' ') ++start;
				}
			}
			return result;
		}

	}	// namespace detail


	// ============================================================================
	// 第二部分：反射宏系统
	// ============================================================================

	namespace detail {

		// 成员变量
		template<typename T> struct member_count : std::integral_constant<size_t, 0> {};
		template<typename T> constexpr auto member_names() { return std::array<std::string_view, 0>{}; }
		template<typename T> constexpr auto members() { return std::tuple<>(); }

		// 成员函数
		template<typename T> struct function_count : std::integral_constant<size_t, 0> {};
		template<typename T> constexpr auto function_names() { return std::array<std::string_view, 0>{}; }
		template<typename T> constexpr auto functions() { return std::tuple<>(); }

		template<typename T> concept Reflectable = member_count<T>::value > 0 || function_count<T>::value > 0;

		constexpr size_t count_args() { return 0; }
		template<typename T, typename... Rest>
		constexpr size_t count_args(T, Rest... rest) { return 1 + count_args(rest...); }

	}	// namespace detail

	// 反射成员变量
#define REFLECT_VARS(Struct, ...) \
		template<> struct reflect::detail::member_count<Struct> \
			: std::integral_constant<size_t, reflect::detail::count_args(__VA_ARGS__)> {}; \
		template<> inline constexpr auto reflect::detail::member_names<Struct>() { \
			return reflect::detail::split_names<reflect::detail::count_args(__VA_ARGS__)>(#__VA_ARGS__); \
		} \
		template<> inline constexpr auto reflect::detail::members<Struct>() { \
			return std::make_tuple(__VA_ARGS__); \
		}


	// 反射成员函数
#define REFLECT_FUNCS(Struct, ...) \
		template<> struct reflect::detail::function_count<Struct> \
			: std::integral_constant<size_t, reflect::detail::count_args(__VA_ARGS__)> { \
			static_assert(detail::function_count<Struct>::value <= 50, "Too many functions, consider using iterative approach"); \
		}; \
		template<> inline constexpr auto reflect::detail::function_names<Struct>() { \
			return reflect::detail::split_names<reflect::detail::count_args(__VA_ARGS__)>(#__VA_ARGS__); \
		} \
		template<> inline constexpr auto reflect::detail::functions<Struct>() { \
			return std::make_tuple(__VA_ARGS__); \
		}


	// ============================================================================
	// 第三部分：反射系统（动静态反射调用，运行时字符串调用）
	// ============================================================================

	namespace detail {

		template<size_t N>
		struct FixedString {
			char data[N]{};
			size_t length = N;

			consteval FixedString(const char(&str)[N]) {
				for (size_t i = 0; i < N; ++i) data[i] = str[i];
			}

			constexpr std::string_view sv() const { return { data, N - 1 }; }

			constexpr bool operator==(const char* other) const {
				return sv() == other;
			}
			constexpr bool operator==(std::string_view other) const {
				return sv() == other;
			}
		};

	}	// namespace detail


	// 反射调用接口
	template<typename Class>
	class Reflection {
	public:
		// ============================================================================
		// 运行时版本
		// 编译期递归展开进行函数参数检查，运行时名字查找调用
		// ============================================================================
		template<typename Object, typename... Args>
		static std::any call(Object&& obj, std::string_view name, Args&&... args) {
			constexpr size_t N = detail::function_count<Class>::value;
			return call_impl<0, N>(get_ptr(std::forward<Object>(obj)), name, std::forward<Args>(args)...);
		}

		// ============================================================================
		// 运行时版本
		// 类型化快速路径动态调用 - 编译期签名匹配
		// ============================================================================
		template<typename Ret, typename Object, typename... Args>
		static Ret call_typed(Object&& obj, std::string_view name, Args&&... args) {
			constexpr size_t N = detail::function_count<Class>::value;
			auto obj_ptr = get_ptr(std::forward<Object>(obj));
			return call_typed_impl<0, N, decltype(obj_ptr), Ret, Args...>(obj_ptr, name, std::forward<Args>(args)...);
		}

		// ============================================================================
		// 编译期版本
		// 编译期字符串查找调用静态调用，函数不存在或参数错误时会直接编译错误
		// ============================================================================
		template<detail::FixedString Name, typename Object, typename... Args>
		static decltype(auto) static_call(Object&& obj, Args&&... args) {
			constexpr size_t idx = find_index<Name>();
			static_assert(idx != static_cast<size_t>(-1), "Function not found");

			constexpr auto func = std::get<idx>(detail::functions<Class>());
			return (get_ptr(std::forward<Object>(obj))->*func)(std::forward<Args>(args)...);
		}

	private:
		static Class* get_ptr(Class* p) { return p; }
		static Class* get_ptr(Class& r) { return &r; }
		static const Class* get_ptr(const Class* p) { return p; }
		static const Class* get_ptr(const Class& r) { return &r; }

		// 编译期查找索引
		template<detail::FixedString Name>
		static consteval size_t find_index() {
			constexpr auto names = detail::function_names<Class>();
			for (size_t i = 0; i < names.size(); ++i)
				if (names[i] == Name.sv()) return i;
			return static_cast<size_t>(-1);
		}

		// 编译期递归函数参数检查，运行时查找调用
		template<size_t I, size_t N, typename ObjPtr, typename... Args>
		static std::any call_impl(ObjPtr obj, std::string_view name, Args&&... args) {
			constexpr auto names = detail::function_names<Class>();

			if constexpr (I < N) {
				if (names[I] == name) {
					constexpr auto funcs = detail::functions<Class>();
					constexpr auto func = std::get<I>(funcs);

					// 检查函数参数是否匹配，不匹配不实例化
					if constexpr (requires { (obj->*func)(std::forward<Args>(args)...); }) {
						if constexpr (std::is_void_v<decltype((obj->*func)(args...))>) {
							(obj->*func)(std::forward<Args>(args)...);
							return std::any{};
						}
						else {
							return std::any{ (obj->*func)(std::forward<Args>(args)...) };
						}
					}
					else {
						// 函数参数不匹配
						throw std::runtime_error(std::format("Function parameter mismatch or const qualifier error: <{}>", name));
					}
				}

				// 不匹配，继续递归
				return call_impl<I + 1, N>(obj, name, std::forward<Args>(args)...);
			}

			// 函数没有找到，抛出异常
			throw std::runtime_error(std::format("Function '{}' not found in {}", name, typeid(Class).name()));
		}

		// 编译期递归函数参数检查，运行时查找调用
		template<size_t I, size_t N, typename ObjPtr, typename Ret, typename... Args>
		static Ret call_typed_impl(ObjPtr obj, std::string_view name, Args&&... args) {
			constexpr auto names = detail::function_names<Class>();

			if constexpr (I < N) {
				if (names[I] == name) {
					constexpr auto funcs = detail::functions<Class>();
					constexpr auto func = std::get<I>(funcs);

					// 检查函数签名
					if constexpr (std::is_invocable_r_v<Ret, decltype(func), ObjPtr, Args...>) {
						if constexpr (std::is_void_v<Ret>) {
							(obj->*func)(std::forward<Args>(args)...);
							return;
						}
						else {
							return (obj->*func)(std::forward<Args>(args)...);
						}
					}
					else {
						throw std::runtime_error(std::format("Function parameter mismatch or const qualifier error: <{}>", name));
					}
				}

				// 递归继续查找
				return call_typed_impl<I + 1, N, ObjPtr, Ret, Args...>(obj, name, std::forward<Args>(args)...);
			}

			// 函数没有找到，抛出异常
			throw std::runtime_error(std::format("Function '{}' not found in {}", name, typeid(Class).name()));
		}

	};


	// ============================================================================
	// 第四部分：类型特征（用于序列化）
	// ============================================================================

	namespace detail {

		template<typename T> struct is_container : std::false_type {};
		template<typename T, typename Alloc> struct is_container<std::vector<T, Alloc>> : std::true_type {};
		template<typename K, typename V, typename Comp, typename Alloc> struct is_container<std::map<K, V, Comp, Alloc>> : std::true_type {};
		template<typename T> inline constexpr bool is_container_v = is_container<T>::value;

		template<typename T> struct is_std_array : std::false_type {};
		template<typename T, size_t N> struct is_std_array<std::array<T, N>> : std::true_type {};
		template<typename T> inline constexpr bool is_std_array_v = is_std_array<T>::value;

		template<typename T> struct is_optional : std::false_type {};
		template<typename T> struct is_optional<std::optional<T>> : std::true_type {};
		template<typename T> inline constexpr bool is_optional_v = is_optional<T>::value;

		template<typename T> struct is_variant : std::false_type {};
		template<typename... Args> struct is_variant<std::variant<Args...>> : std::true_type {};
		template<typename T> inline constexpr bool is_variant_v = is_variant<T>::value;

		template<typename T> struct is_smart_ptr : std::false_type {};
		template<typename T> struct is_smart_ptr<std::unique_ptr<T>> : std::true_type {};
		template<typename T> struct is_smart_ptr<std::shared_ptr<T>> : std::true_type {};
		template<typename T> inline constexpr bool is_smart_ptr_v = is_smart_ptr<T>::value;

	}	// namespace detail


	// ============================================================================
	// 第五部分：二进制序列化器
	// ============================================================================

	class BinarySerializer {
	public:
		std::vector<uint8_t> data;

		template<typename T>
			requires std::is_arithmetic_v<T> || std::is_enum_v<T>
		void write(const T & value) {
			const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
			data.insert(data.end(), bytes, bytes + sizeof(T));
		}

		void write(const std::string& str) {
			write(static_cast<uint32_t>(str.size()));
			data.insert(data.end(), str.begin(), str.end());
		}

		template<typename T>
			requires detail::is_container_v<T> || detail::is_std_array_v<T>
		void write(const T & container) {
			if constexpr (detail::is_std_array_v<T>) {
				for (const auto& item : container) write(item);
			}
			else {
				write(static_cast<uint32_t>(container.size()));
				for (const auto& item : container) {
					if constexpr (requires { typename T::mapped_type; }) {
						write(item.first);
						write(item.second);
					}
					else {
						write(item);
					}
				}
			}
		}

		template<typename T>
			requires detail::is_optional_v<T>
		void write(const T& opt) {
			write(static_cast<bool>(opt.has_value()));
			if (opt.has_value()) write(*opt);
		}

		template<typename... Args>
		void write(const std::variant<Args...>& var) {
			write(static_cast<uint32_t>(var.index()));
			std::visit([this](const auto& val) { this->write(val); }, var);
		}

		template<typename T>
			requires detail::is_smart_ptr_v<T>
		void write(const T& ptr) {
			write(static_cast<bool>(ptr != nullptr));
			if (ptr) write(*ptr);
		}

		template<typename T>
			requires detail::Reflectable<T>
		void write(const T& obj) {
			if constexpr (detail::member_count<T>::value > 0) {
				write_members(obj, std::make_index_sequence<detail::member_count<T>::value>{});
			}
		}

	private:
		template<typename T, size_t... Is>
		void write_members(const T& obj, std::index_sequence<Is...>) {
			constexpr auto member_ptrs = detail::members<T>();
			(write(obj.*std::get<Is>(member_ptrs)), ...);
		}
	};

	// ============================================================================
	// 第六部分：二进制反序列化器
	// ============================================================================

	class BinaryDeserializer {
	public:
		const uint8_t* data;
		size_t size;
		size_t pos = 0;

		BinaryDeserializer(const std::vector<uint8_t>& d) : data(d.data()), size(d.size()) {}

		template<typename T>
			requires std::is_arithmetic_v<T> || std::is_enum_v<T>
		void read(T & value) {
			if (pos + sizeof(T) > size) throw std::runtime_error("Buffer overflow");
			std::memcpy(&value, data + pos, sizeof(T));
			pos += sizeof(T);
		}

		void read(std::string& str) {
			uint32_t len;
			read(len);
			if (pos + len > size) throw std::runtime_error("String overflow");
			str.assign(reinterpret_cast<const char*>(data + pos), len);
			pos += len;
		}

		template<typename T>
			requires detail::is_container_v<T> || detail::is_std_array_v<T>
		void read(T & container) {
			if constexpr (detail::is_std_array_v<T>) {
				for (auto& item : container) read(item);
			}
			else {
				uint32_t len;
				read(len);
				container.clear();
				if constexpr (requires { container.reserve(0); }) container.reserve(len);
				for (uint32_t i = 0; i < len; ++i) {
					if constexpr (requires { typename T::mapped_type; }) {
						typename T::key_type key;
						typename T::mapped_type val;
						read(key);
						read(val);
						container.emplace(std::move(key), std::move(val));
					}
					else {
						typename T::value_type val;
						read(val);
						container.push_back(std::move(val));
					}
				}
			}
		}

		template<typename T>
			requires detail::is_optional_v<T>
		void read(T& opt) {
			bool has_val;
			read(has_val);
			if (has_val) {
				typename T::value_type val;
				read(val);
				opt = std::move(val);
			}
			else {
				opt = std::nullopt;
			}
		}

		template<typename... Args>
		void read(std::variant<Args...>& var) {
			uint32_t index;
			read(index);

			if (index >= sizeof...(Args)) {
				throw std::runtime_error("Invalid variant index");
			}

			auto read_variant = [this, index]<size_t... Is>(std::index_sequence<Is...>) {
				((index == Is ? (var = read_variant_alt<Args...>(std::integral_constant<size_t, Is>{}), 0) : 0), ...);
			};

			read_variant(std::index_sequence_for<Args...>{});
		}

		template<typename... Args, size_t I>
		auto read_variant_alt(std::integral_constant<size_t, I>) {
			using Type = std::variant_alternative_t<I, std::variant<Args...>>;
			Type val;
			read(val);
			return val;
		}

		template<typename T>
			requires detail::is_smart_ptr_v<T>
		void read(T& ptr) {
			bool has_val;
			read(has_val);
			if (has_val) {
				using Pointee = typename T::element_type;
				auto val = std::make_unique<Pointee>();
				read(*val);
				if constexpr (std::is_same_v<T, std::unique_ptr<Pointee>>) {
					ptr = std::move(val);
				}
				else {
					ptr = std::shared_ptr<Pointee>(std::move(val));
				}
			}
			else {
				ptr = nullptr;
			}
		}

		template<typename T>
			requires detail::Reflectable<T>
		void read(T& obj) {
			if constexpr (detail::member_count<T>::value > 0) {
				read_members(obj, std::make_index_sequence<detail::member_count<T>::value>{});
			}
		}

	private:
		template<typename T, size_t... Is>
		void read_members(T& obj, std::index_sequence<Is...>) {
			constexpr auto member_ptrs = detail::members<T>();
			((this->read(obj.*std::get<Is>(member_ptrs))), ...);
		}
	};


	// ============================================================================
	// 第七部分：便捷API
	// ============================================================================

	template<typename T>
	std::vector<uint8_t> serialize_binary(const T& obj) {
		BinarySerializer ser;
		ser.write(obj);
		return std::move(ser.data);
	}

	template<typename T>
	T deserialize_binary(const std::vector<uint8_t>& data) {
		T obj{};
		BinaryDeserializer des(data);
		des.read(obj);
		return obj;
	}

	template<typename T>
	void serialize_to_file(const T& obj, std::string_view file_path) {
		BinarySerializer ser;
		ser.write(obj);
		std::ofstream file(std::string(file_path), std::ios::binary);
		if (!file) throw std::runtime_error("Failed to open file for writing: " + std::string(file_path));
		file.write(reinterpret_cast<const char*>(ser.data.data()), ser.data.size());
		if (!file) throw std::runtime_error("Failed to write file: " + std::string(file_path));
	}

	template<typename T>
	T deserialize_from_file(std::string_view file_path) {
		if (!std::filesystem::exists(file_path)) {
			throw std::runtime_error("File not found: " + std::string(file_path));
		}
		std::vector<uint8_t> data;
		size_t file_size = std::filesystem::file_size(file_path);
		data.resize(file_size);
		std::basic_ifstream<uint8_t, std::char_traits<uint8_t>> file(file_path.data(), std::ios::binary);
		if (!file) throw std::runtime_error("Failed to open file for reading: " + std::string(file_path));
		file.read(data.data(), data.size());
		if (!file) throw std::runtime_error("Failed to read file: " + std::string(file_path));
		T obj{};
		BinaryDeserializer des(data);
		des.read(obj);
		return obj;
	}

} // namespace reflect

#endif // REFLECT_HPP