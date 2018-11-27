//
// Created by aiden on 16/11/18.
//

#ifndef MYBOT_UTILS_H
#define MYBOT_UTILS_H

#include <sys/time.h>
#include <string>
#include <bits/stdc++.h>

using namespace std;

double getTime() {
	timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec + tv.tv_usec * 1e-6;
}

struct Timer {
	double total = 0;
	double startTime;
	bool running = false;
	string name = "";

	Timer(string _name = "") {
		name = _name;
	}

	void start() {
		startTime = getTime();
		running = true;
	}

	void stop() {
		total += getTime() - startTime;
		running = false;
	}

	double elapsed() {
		return total + (running ? getTime() - startTime : 0);
	}

	string tostring() {
		return "Timer" + name + ": " + to_string(elapsed());
	}
};

/*

const int N = 64;
// state space tree node
struct Node
{
	// stores parent node of current node
	// helps in tracing path when answer is found
	Node* parent;

	// contains cost for ancestors nodes
	// including current node
	double pathCost;

	// contains least promising cost
	double cost;

	// contain worker number
	int workerID;

	// contains Job ID
	int jobID;

	// Boolean array assigned will contains
	// info about available jobs
	bool assigned[N];
};

// Function to allocate a new search tree node
// Here Person x is assigned to job y
Node* newNode(int x, int y, bool assigned[],
			  Node* parent)
{
	Node* node = new Node;

	for (int j = 0; j < N; j++)
		node->assigned[j] = assigned[j];

	if (y >= 0)
        node->assigned[y] = true;

	node->parent = parent;
	node->workerID = x;
	node->jobID = y;

	return node;
}

// Function to calculate the least promising cost
// of node after worker x is assigned to job y.
int calculateCost(int costMatrix[N][N], int x,
				  int y, bool assigned[])
{
	int cost = 0;

	// to store unavailable jobs
	bool available[N] = {true};

	// start from next worker
	for (int i = x + 1; i < N; i++)
	{
		int min = -9999999999, minIndex = -1;

		// do for each job
		for (int j = 0; j < N; j++)
		{
			// if job is unassigned
			if (!assigned[j] && available[j] &&
				costMatrix[i][j] < min)
			{
				// store job number
				minIndex = j;

				// store cost
				min = costMatrix[i][j];
			}
		}

		// add cost of next worker
		cost += min;

		// job becomes unavailable
		available[minIndex] = false;
	}

	return cost;
}

// Comparison object to be used to order the heap
struct comp
{
	bool operator()(const Node* lhs,
					const Node* rhs) const
	{
		return lhs->cost > rhs->cost;
	}
};

// print Assignments
void printAssignments(Node *min)
{
	if(min->parent==NULL)
		return;

	printAssignments(min->parent);
	cout << "Assign Worker " << char(min->workerID + 'A')
		 << " to Job " << min->jobID << endl;

}

// Finds minimum cost using Branch and Bound.
int findMinCost(int costMatrix[N][N])
{
	// Create a priority queue to store live nodes of
	// search tree;
	priority_queue<Node*, std::vector<Node*>, comp> pq;

	// initailize heap to dummy node with cost 0
	bool assigned[N] = {false};
	Node* root = newNode(-1, -1, assigned, NULL);
	root->pathCost = root->cost = 0;
	root->workerID = -1;

	// Add dummy node to list of live nodes;
	pq.push(root);

	// Finds a live node with least cost,
	// add its childrens to list of live nodes and
	// finally deletes it from the list.
	while (!pq.empty())
	{
		// Find a live node with least estimated cost
		Node* min = pq.top();

		// The found node is deleted from the list of
		// live nodes
		pq.pop();

		// i stores next worker
		int i = min->workerID + 1;

		// if all workers are assigned a job
		if (i == N)
		{
			printAssignments(min);
			return min->cost;
		}

		// do for each job
		for (int j = 0; j < N; j++)
		{
			// If unassigned
			if (!min->assigned[j])
			{
				// create a new tree node
				Node* child = newNode(i, j, min->assigned, min);

				// cost for ancestors nodes including current node
				child->pathCost = min->pathCost + costMatrix[i][j];

				// calculate its lower bound
				child->cost = child->pathCost +
							  calculateCost(costMatrix, i, j, child->assigned);

				// Add child to list of live nodes;
				pq.push(child);
			}
		}
	}
}*/

#endif //MYBOT_UTILS_H
