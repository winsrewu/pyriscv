#include <stdio.h>
#include <cstring>
#include <algorithm> // 用于排序
#include <vector>

using namespace std;

int main()
{
    char str[100];
    scanf("%99[^\n]", str);

    int n = strlen(str);
    vector<int> arr = vector<int>(0);

    bool num_end = false;
    int num_cache = 0;

    for (int i = 0; i < n; i++)
    {
        if (str[i] == ',')
        {
            arr.push_back(num_cache);
            num_cache = 0;
            continue;
        }

        if (str[i] >= '0' && str[i] <= '9')
        {
            num_cache = num_cache * 10 + (str[i] - '0');
        }
    }

    arr.push_back(num_cache);

    sort(arr.begin(), arr.end());

    printf("After sorting: ");
    for (auto i : arr)
    {
        printf("%d ", i);
    }
    printf("\n");

    return 0;
}
