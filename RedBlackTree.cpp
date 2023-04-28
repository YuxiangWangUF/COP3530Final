#include <iostream>
#include <string>
#include <algorithm>
#include "RedBlackTree.h"
#include <vector>
#include <random>


using namespace std;

RedBlackTree::RedBlackTree() {
    root = nullptr;
}


RedBlackTree::RBNode::RBNode(string primaryTitle,
                             int year,
                             int runtimeMinutes, string genres) {

    this->primaryTitle = primaryTitle;
    this->year = year;
    this->runtimeMinutes = runtimeMinutes;
    this->genres = genres;

    parent = nullptr;
    left_child = nullptr;
    right_child = nullptr;

}

void
RedBlackTree::insert(string primaryTitle, int year,
                     int runtimeMinutes, string genres) {

    auto *new_node = new RBNode(primaryTitle, year, runtimeMinutes, genres);
    if (root == nullptr) {
        new_node->is_black = true;
        root = new_node;
    } else {
        new_node->is_black = false;
        RBNode *current_node = root;
        while (true) {
            if (primaryTitle < current_node->primaryTitle) {
                if (current_node->left_child == nullptr) {
                    current_node->left_child = new_node;
                    new_node->parent = current_node;
                    break;
                } else {
                    current_node = current_node->left_child;
                }
            } else {
                if (current_node->right_child == nullptr) {
                    current_node->right_child = new_node;
                    new_node->parent = current_node;
                    break;
                } else {
                    current_node = current_node->right_child;
                }
            }
        }
        fix_tree(new_node);
    }
}

void RedBlackTree::rotate_left(RBNode *node) {
    RBNode *right_child = node->right_child;
    node->right_child = right_child->left_child;
    if (right_child->left_child != nullptr) {
        right_child->left_child->parent = node;
    }
    right_child->parent = node->parent;
    if (node->parent == nullptr) {
        root = right_child;
    } else if (node == node->parent->left_child) {
        node->parent->left_child = right_child;
    } else {
        node->parent->right_child = right_child;
    }
    right_child->left_child = node;
    node->parent = right_child;

}

void RedBlackTree::fix_tree(RBNode *node) {
    while (node != root && !node->parent->is_black && node->parent->parent != nullptr) {
        RBNode *parent = node->parent;
        RBNode *grandparent = parent->parent;
        if (parent == grandparent->left_child) {
            RBNode *uncle = grandparent->right_child;
            if (uncle != nullptr && !uncle->is_black) {
                parent->is_black = true;
                uncle->is_black = true;
                grandparent->is_black = false;
                node = grandparent;
            } else {
                if (node == parent->right_child) {
                    node = parent;
                    rotate_left(node);
                }
                parent->is_black = true;
                grandparent->is_black = false;
                rotate_right(grandparent);
            }
        } else {
            RBNode *uncle = grandparent->left_child;
            if (uncle != nullptr && !uncle->is_black) {
                parent->is_black = true;
                uncle->is_black = true;
                grandparent->is_black = false;
                node = grandparent;
            } else {
                if (node == parent->left_child) {
                    node = parent;
                    rotate_right(node);
                }
                parent->is_black = true;
                grandparent->is_black = false;
                rotate_left(grandparent);
            }
        }
    }
    root->is_black = true;
}


void RedBlackTree::rotate_right(RBNode *node) {
    RBNode *left_child = node->left_child;
    node->left_child = left_child->right_child;
    if (left_child->right_child != nullptr) {
        left_child->right_child->parent = node;
    }
    left_child->parent = node->parent;
    if (node->parent == nullptr) {
        root = left_child;
    } else if (node == node->parent->left_child) {
        node->parent->left_child = left_child;
    } else {
        node->parent->right_child = left_child;
    }
    left_child->right_child = node;
    node->parent = left_child;
}

void RedBlackTree::SearchTitle(string title) {
    RBNode *current_node = root;
    bool found = false;
    while (current_node != nullptr) {
        if (current_node->primaryTitle == title) {
            found = true;
            cout << current_node->primaryTitle << " " << current_node->year << endl;
            cout << "Runtime (minutes): " << current_node->runtimeMinutes << endl;
            cout << "Genres: " << current_node->genres << endl;
            current_node = current_node->right_child;
        } else if (title < current_node->primaryTitle) {
            current_node = current_node->left_child;
        } else {
            current_node = current_node->right_child;
        }
    }
    if(!found){
        cout << "Title not found!" << endl;
    }
}

// Implement the print_movies function using inorder traversal
void RedBlackTree::SearchG(string genre) {
    vector<RBNode *> nodes;
    SearchGenres(root, genre, nodes);
    auto rng = default_random_engine{};
    shuffle(nodes.begin(), nodes.end(), rng);
    if(nodes.size() == 0){
        cout << "Genre not found!" << endl;
        return;
    }
    for (int i = 0; i < 10; i++) {
        cout << nodes[i]->primaryTitle << " " << nodes[i]->year << endl;
        cout << "Runtime (minutes): " << nodes[i]->runtimeMinutes << endl;
        cout << "Genres: " << nodes[i]->genres << endl;
        cout << endl;
    }
}

// Helper function for inorder traversal
void RedBlackTree::SearchGenres(RBNode *node, string genre, vector<RBNode *> &nodes) {

    if (node != nullptr) {
        SearchGenres(node->left_child, genre, nodes);
        if (node->genres.find(genre) != string::npos) {
            nodes.push_back(node);
        }
        SearchGenres(node->right_child, genre, nodes);
    }
}

void RedBlackTree::SearchR(int min, int max) {
    vector<RBNode *> nodes;
    SearchRuntimes(root, min, max, nodes);
    if(nodes.size() == 0){
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

void RedBlackTree::SearchRuntimes(RBNode *node, int min, int max, vector<RBNode *> &nodes) {
    if (node != nullptr) {
        SearchRuntimes(node->left_child, min, max, nodes);
        if (node->runtimeMinutes >= min && node->runtimeMinutes <= max) {
            nodes.push_back(node);
        }
        SearchRuntimes(node->right_child, min, max, nodes);
    }

}

void RedBlackTree::SearchY(int year) {
    vector<RBNode *> nodes;
    SearchYears(root, year, nodes);
    if(nodes.size() == 0){
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

void RedBlackTree::SearchYears(RBNode *node, int year, vector<RBNode *> &nodes) {
    if (node != nullptr) {
        SearchYears(node->left_child, year, nodes);
        if (node->year == year) {
            nodes.push_back(node);
        }
        SearchYears(node->right_child, year, nodes);
    }

}







