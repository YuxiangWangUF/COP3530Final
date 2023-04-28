#include <string>
#include <vector>

using namespace std;

class MinHeap {
public:
    MinHeap();

    struct HeapNode {
        HeapNode(string primaryTitle, int year,
                 int runtimeMinutes, string genres);

        string primaryTitle;
        int year;
        int runtimeMinutes;
        string genres;

    };

    vector<HeapNode *> arr;

    void Insert(string primaryTitle, int year,
                int runtimeMinutes, string genres);

    void SearchTitle(string title);

    void SearchGenres(string genre);

    void SearchRuntimes(int min, int max);

    void SearchYears(int year);
};