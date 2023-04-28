#include <iostream>
#include "MinHeap.h"
#include <random>
#include <algorithm>

using namespace std;


MinHeap::MinHeap() {//default constructor

}

MinHeap::HeapNode::HeapNode(string primaryTitle, int year, int runtimeMinutes,
                            string genres) {//node constructor
    this->primaryTitle = primaryTitle;
    this->year = year;
    this->runtimeMinutes = runtimeMinutes;
    this->genres = genres;

}

void MinHeap::Insert(string primaryTitle, int year,
                     int runtimeMinutes, string genres) {//inserts new entry into heap vector
    HeapNode *h = new HeapNode(primaryTitle, year, runtimeMinutes, genres);//creating node
    arr.push_back(h);
    int child = arr.size() - 1;
    int parent = (arr.size() - 1) / 2;
    while (parent >= 0 && arr[parent]->primaryTitle > arr[child]->primaryTitle) {//putting the vector entry into its correct spot (min heap - smallest value on top)
        HeapNode *temp = arr[parent];
        arr[parent] = arr[child];
        arr[child] = temp;
        child = parent;
        parent = (child - 1) / 2;
    }
}

void MinHeap::SearchTitle(string title) {
    bool found = false;//will be true if match is found
    for (int i = 0; i < arr.size(); i++) {//iterates through array looking for title match
        if (arr[i]->primaryTitle == title) {
            found = true;
            cout << arr[i]->primaryTitle << " " << arr[i]->year << endl;
            cout << "Runtime (minutes): " << arr[i]->runtimeMinutes << endl;
            cout << "Genres: " << arr[i]->genres << endl;
        }
    }
    if (!found) {
        cout << "Title not found!" << endl;
    }
}

void MinHeap::SearchGenres(string genre) {
    vector<HeapNode *> nodes;
    for (int i = 0; i < arr.size(); i++) {
        if (arr[i]->genres.find(genre) != string::npos) {
            nodes.push_back(arr[i]);
        }
    }
    if (nodes.size() == 0) {
        cout << "Genre not found!" << endl;
        return;
    }
    auto rng = default_random_engine{};
    shuffle(nodes.begin(), nodes.end(), rng);
    for (int i = 0; i < 10; i++) {
        cout << nodes[i]->primaryTitle << " " << nodes[i]->year << endl;
        cout << "Runtime (minutes): " << nodes[i]->runtimeMinutes << endl;
        cout << "Genres: " << nodes[i]->genres << endl;
        cout << endl;
    }


}

void MinHeap::SearchRuntimes(int min, int max) {
    vector<HeapNode *> nodes;
    for (int i = 0; i < arr.size(); i++) {
        if (arr[i]->runtimeMinutes >= min && arr[i]->runtimeMinutes <= max) {
            nodes.push_back(arr[i]);
        }
    }
    if (nodes.size() == 0) {
        cout << "No runtimes matching filters!" << endl;
        return;
    }
    auto rng = default_random_engine{};
    shuffle(nodes.begin(), nodes.end(), rng);
    for (int i = 0; i < 10; i++) {
        cout << nodes[i]->primaryTitle << " " << nodes[i]->year << endl;
        cout << "Runtime (minutes): " << nodes[i]->runtimeMinutes << endl;
        cout << "Genres: " << nodes[i]->genres << endl;
        cout << endl;
    }

}

void MinHeap::SearchYears(int year) {
    vector<HeapNode *> nodes;
    for (int i = 0; i < arr.size(); i++) {
        if (arr[i]->year == year) {
            nodes.push_back(arr[i]);
        }
    }
    if (nodes.size() == 0) {
        cout << "No movies from !" << year << endl;
        return;
    }

    auto rng = default_random_engine{};
    shuffle(nodes.begin(), nodes.end(), rng);
    for (int i = 0; i < 10; i++) {
        cout << nodes[i]->primaryTitle << " " << nodes[i]->year << endl;
        cout << "Runtime (minutes): " << nodes[i]->runtimeMinutes << endl;
        cout << "Genres: " << nodes[i]->genres << endl;
        cout << endl;
    }

}