#include <vector>

using namespace std;

class RedBlackTree {
public:
    RedBlackTree();

    struct RBNode {
    public:
        RBNode(string primaryTitle, int year, int runtimeMinutes, string genres);

        bool is_black;
        RBNode *parent, *left_child, *right_child = nullptr;
        string primaryTitle;
        int year;
        int runtimeMinutes;
        string genres;

    };

//
    RBNode *root;

    // Define a function to insert a new node into the tree
    void insert(string primaryTitle, int year,
                int runtimeMinutes, string genres);

    // Define a function to fix the tree after inserting a new node
    void fix_tree(RBNode *node);

    // Define a function to perform a left rotation on a given node
    void rotate_left(RBNode *node);

    // Define a function to perform a right rotation on a given node
    void rotate_right(RBNode *node);

    // Define a function to search for a movie in the tree
    void SearchTitle(string title);

    // Print all movies in the tree in alphabetical order by title
    void SearchG(string genre);

    void SearchR(int min, int max);

    void SearchRuntimes(RBNode *node, int min, int max, vector<RBNode *> &nodes);

    void SearchY(int year);

    void SearchYears(RBNode *node, int year, vector<RBNode *> &nodes);

    void SearchGenres(RBNode *node, string genre, vector<RBNode *> &nodes);

};