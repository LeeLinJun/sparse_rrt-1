/**
 * @file tree_node.hpp
 * 
 * @copyright Software License Agreement (BSD License)
 * Copyright (c) 2014, Rutgers the State University of New Jersey, New Brunswick  
 * All Rights Reserved.
 * For a full description see the file named LICENSE.
 * 
 * Authors: Zakary Littlefield, Kostas Bekris 
 * 
 */

#ifndef SPARSE_TREE_NODE_HPP
#define SPARSE_TREE_NODE_HPP

#include <list>
#include <cstddef>

class proximity_node_t;

/**
 * @brief An edge of the tree.
 * @details An edge of the tree.
 */
class tree_edge_t
{
public:
    /**
     * @brief The control for this edge.
     */
	double* control;
    /**
     * @brief The duration to execute control.
     */
	double duration;
};

/**
 * @brief A node of the tree.
 * @details A node of the tree.
 */
class tree_node_t
{
public:
	tree_node_t(double* a_point)
	    : point(a_point)
	{
		prox_node = NULL;
		parent_edge = NULL;
		children.clear();
		cost = 0;
	}

    /**
     * @brief A pointer to the node in the nearest neighbor structure for easy removal.
     */
    proximity_node_t* prox_node;

    /**
    * @brief Parent edge
    */
    tree_edge_t* parent_edge;

    /**
     * @brief The path cost to this node.
     */
    double cost;

    double* get_point() const {
        return this->point;
    }

    void dealloc_point() {
        delete[] point;
        point = NULL;
    }

//    const std::list<tree_node_t*>& get_children() const {
//        return this->children;
//    }

    void add_child(tree_node_t* node) {
        this->children.insert(this->children.begin(), node);
    }

    void remove_child(tree_node_t* node) {
        this->children.remove(node);
    }
    /**
     * @brief Children vertices
     */
    std::list<tree_node_t*> children;

private:
     /**
     * @brief The state represented by this node.
     */
    double* point;

};

#endif