#include "array_view.h"
#include "parallel_vector.h"
#include "strong_typedef.h"

#include <string>
#include <tuple>
#include <vector>

void testStrongTypedef()
{
	STRONG_TYPEDEF(first_name, std::string);
	STRONG_TYPEDEF(last_name, std::string);
	STRONG_TYPEDEF(age, int);

	age agex; // should be left uninitialized

	std::tuple<first_name, last_name, age> t = { "John", "Smith", 25 };
	first_name name = std::get<first_name>(t);
	bool ok = name == "John";
	std::string name2 = std::move(name);
	name = "other";
	auto a = std::get<age>(t) + 1;

	STRONG_TYPEDEF(mypair, std::pair<int, float>);
	mypair p = { 1, 2.0f };

	STRONG_TYPEDEF(myptr, const char*);
	myptr ptr = "Test string";

	STRONG_TYPEDEF(intarr, int[2]);
	intarr ia = { 1, 2 };
	int x = ia[1];
}

void testArrayView()
{
	std::vector<int> vec = { 1, 2, 3 };
	int arr[] = { 10, 20, 30 };

	utl::array_view<int> v1;
	utl::array_view<int> v2 = { vec.data() + 1, vec.data() + vec.size() };
	utl::array_view<int> v3 = { arr, arr + 3 };

	int sum = 0;
	for (auto i : v2)
		sum += i;

	v2[0] = 4;
}

int main()
{
// 	testStrongTypedef();
// 	testArrayView();

	using namespace utl::detail;

	using mylist = type_list<int, char>;
	using t0 = type_list_element_t<0, mylist>;
	using t1 = type_list_element_t<1, mylist>;
// 	using t2 = type_list_element_t<2, mylist>;
// 	using t3 = type_list_element_t<0, char>;

	utl::parallel_vector<int> vz;
	vz.push_back(1);
	vz.push_back(3);

	utl::parallel_vector<std::string, int, char> vec(1);
	std::string x = "mylonglonglonglonglonglongstirng";
	vec.push_back(std::move(x), 1, 'a');
	x = "myotherotherotherstring";
	vec.push_back(std::forward_as_tuple(x), 2, 'b');
	vec.push_back(std::forward_as_tuple(20, 'x'), 3, 'c');
	vec.push_back(std::forward_as_tuple(), 4, 'd');
	vec.push_back("fifth", 5, 'e');

	utl::parallel_vector<std::string, int, char> vec2;
	vec2.push_back("somethingxxx", 10, 'x');
	vec2.push_back("somethingxxx2", 11, 'y');

	const auto &rvec2 = vec2;

	vec.insert_move(4, vec2, 0, 1);

	for (auto &s : vec.slice<std::string>())
	{
		printf("%s\n", s.c_str());
	}
	printf("---\n");
	for (auto &s : vec2.slice<std::string>())
	{
		printf("%s\n", s.c_str());
	}

	utl::parallel_vector<std::tuple<int, char>, float> vv;
	vv.push_back(std::make_tuple(1, 'a'), 1.0f);

	return 0;
}
