#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include "MinHeap.h"
#include "RedBlackTree.h"

using namespace std;

void ReadFile(string file, string mode, MinHeap &mh, RedBlackTree &rb) {//reads imdb dataset file

    ifstream inFile(file);

    if (inFile.is_open()) {
        string fileLine;
        getline(inFile, fileLine);


        while (getline(inFile, fileLine)) {
            istringstream stream(fileLine);

            string tconst;
            string titleType;
            string primaryTitle;
            string originalTitle;
            string tempIsAdult;
            int isAdult;
            string tempStartYear;
            int startYear;
            string tempEndYear;
            string tempRuntimeMinutes;
            int runtimeMinutes;
            string genres;


            getline(stream, tconst, '\t');
            getline(stream, titleType, '\t');
            getline(stream, primaryTitle, '\t');
            getline(stream, originalTitle, '\t');
            getline(stream, tempIsAdult, '\t');
            isAdult = stoi(tempIsAdult);
            getline(stream, tempStartYear, '\t');
            if (isdigit(tempStartYear[0])) {
                startYear = stoi(tempStartYear);
            } else {
                startYear = -1;
            }

            getline(stream, tempEndYear, '\t');
            getline(stream, tempRuntimeMinutes, '\t');
            if (isdigit(tempRuntimeMinutes[0])) {
                runtimeMinutes = stoi(tempRuntimeMinutes);
            } else {
                runtimeMinutes = -1;
            }
            getline(stream, genres, '\t');

            if (titleType == "movie") { //only add entries of type movie

                if (mode == "rb") {//inserts into red black tree
                    rb.insert(primaryTitle, startYear, runtimeMinutes,
                              genres);
                } else {//inserts into min heap
                    mh.Insert(primaryTitle, startYear, runtimeMinutes,
                              genres);

                }
            }
        }
    }
}


int main() {


    bool run = true;
    string mode;
    clock_t t1;//creates clock objects for use in determining execution time of all commands
    clock_t t2;
    while (run) {

        int input;
        //main menu where you can choose data mode
        cout << "Movie Catalog -- Main Menu:" << endl;
        cout << "Select a Data Mode:" << endl;
        cout << "1. Red-Black Tree" << endl;
        cout << "2. Min Heap" << endl;
        cout << "3. Exit" << endl;
        bool menu = true;
        cin >> input;
        RedBlackTree rb;//initiating both data structures, empty to start
        MinHeap mh;
        if (input == 1) {
            t1 = clock();
            mode = "rb";
            cout << "Creating Red-Black Tree..." << endl;
            ReadFile("title.basics.tsv", "rb", mh, rb);
            t2 = clock();
            cout << "Execution Time: " << ((float) (t2) / CLOCKS_PER_SEC - (float) (t1) / CLOCKS_PER_SEC) << " seconds"
                 << endl;//determining execution time in seconds by subtracting time 2 from time 1 and dividing by clocks per seconds
        } else if (input == 2) {
            t1 = clock();
            mode = "mh";
            cout << "Creating Min Heap..." << endl;
            ReadFile("title.basics.tsv", "mh", mh, rb);
            t2 = clock();
            cout << "Execution Time: " << ((float) (t2) / CLOCKS_PER_SEC - (float) (t1) / CLOCKS_PER_SEC) << " seconds"
                 << endl;
        } else {
            run = false;
            menu = false;
        }

        while (menu) {//menu of operations to perform on set of movies

            cout << "Select an Operation:" << endl;
            cout << "1. Search Movie by Title" << endl;
            cout << "2. Search Movie by Genres (Displays up to 10)" << endl;
            cout << "3. Search Movie by Runtime (Displays up to 10)" << endl;
            cout << "4. Search Movie by Year (Displays up to 10)" << endl;
            cout << "5. Main Menu" << endl;

            cin >> input;

            switch (input) {
                case 1: {
                    cout << "Enter Title: " << endl;
                    std::cin.ignore();//flushing cin
                    string title;
                    getline(cin, title);
                    t1 = clock();
                    if (mode == "rb") {//calls a method based on data mode
                        rb.SearchTitle(title);
                    } else {
                        mh.SearchTitle(title);
                    }
                    t2 = clock();
                    cout << "Execution Time: " << ((float) (t2) / CLOCKS_PER_SEC - (float) (t1) / CLOCKS_PER_SEC)
                         << " seconds" << endl;//printing execution time
                    break;
                }
                case 2: {
                    cout << "Enter Genre: " << endl;
                    std::cin.ignore();
                    string genre;
                    getline(cin, genre);
                    t1 = clock();
                    if (mode == "rb") {
                        rb.SearchG(genre);
                    } else {
                        mh.SearchGenres(genre);
                    }
                    t2 = clock();
                    cout << "Execution Time: " << ((float) (t2) / CLOCKS_PER_SEC - (float) (t1) / CLOCKS_PER_SEC)
                         << " seconds" << endl;
                    break;
                }
                case 3: {//entering bounds for runtime
                    cout << "Enter Minimum Runtime: " << endl;
                    int min;
                    cin >> min;
                    cout << "Enter Maximum Runtime: " << endl;
                    int max;
                    cin >> max;
                    t1 = clock();
                    if (mode == "rb") {
                        rb.SearchR(min, max);
                    } else {
                        mh.SearchRuntimes(min, max);
                    }
                    t2 = clock();
                    cout << "Execution Time: " << ((float) (t2) / CLOCKS_PER_SEC - (float) (t1) / CLOCKS_PER_SEC)
                         << " seconds" << endl;
                    break;
                }
                case 4: {
                    cout << "Enter Release Year: " << endl;
                    int year;
                    cin >> year;
                    t1 = clock();
                    if (mode == "rb") {
                        rb.SearchY(year);
                    } else {
                        mh.SearchYears(year);
                    }
                    t2 = clock();
                    cout << "Execution Time: " << ((float) (t2) / CLOCKS_PER_SEC - (float) (t1) / CLOCKS_PER_SEC)
                         << " seconds" << endl;
                    break;
                }
                case 5://goes back to main menu
                    menu = false;
                    break;
            }
            cout << endl;
        }
    }
    return 0;
}