#include <iostream>
#include <set>
#include <utility>
#include <string>

using namespace std;

int main()
{
	set<pair<string, string>> my_dict;
	unsigned int synonyms_count;
	cin >> synonyms_count;
	string new_synonyms;
	string first_synonym;
	string second_synonym;
	string request;
	for (unsigned int i = 0; i < synonyms_count; ++i)
	{
		cin >> first_synonym;
		cin >> second_synonym;
		my_dict.insert(make_pair(first_synonym, second_synonym));
	}
	cin >> request;
	for (auto synonyms_pair = my_dict.begin(); synonyms_pair != my_dict.end(); synonyms_pair++)
	{
		if (synonyms_pair->first == request)
		{
			cout << synonyms_pair->second;
		}
			
		if (synonyms_pair->second == request)
			cout << synonyms_pair->first;
	}

	return 0;
}
