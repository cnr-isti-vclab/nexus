/*
Nexus

Copyright(C) 2012 - Federico Ponchio
ISTI - Italian National Research Council - Visual Computing Lab

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License (http://www.gnu.org/licenses/gpl.txt)
for more details.
*/
#ifndef NX_TRAVERSAL_H
#define NX_TRAVERSAL_H

#include "nexusdata.h"

/* a generic DAG traversal algorithm:
   add(0) //needs virtual compute error
   while(heap) {
	 if(process()) //virtual
	   addchildren()
	 else
	   break;
   }
 */

namespace nx {

class Traversal {
public:
	enum Action { STOP,           //stop traversal
				  EXPAND,         //expand node
				  BLOCK };        //do not expand node and block all his children

	struct HeapNode {
		uint32_t node;
		float error;
		bool visible;

		HeapNode(uint32_t _node, float _error, bool _visible):
			node(_node), error(_error), visible(_visible) {}
		bool operator<(const HeapNode &n) const {
			if(error == n.error) return node > n.node;
			return error < n.error;
		}
		bool operator>(const HeapNode &n) const {
			if(error == n.error) return node < n.node;
			return error > n.error;
		}
	};

	NexusData *nexus;
	std::vector<bool> selected;

	Traversal();
	virtual ~Traversal() {}
	void traverse(NexusData *nx);

	virtual float nodeError(uint32_t node, bool &visible);
	virtual Action expand(HeapNode /*h*/) = 0;

protected:
	uint32_t sink;
	std::vector<HeapNode> heap;
	std::vector<bool> visited;
	std::vector<bool> blocked; //nodes blocked by some parent which could not be expanded
	int32_t non_blocked;
	int32_t prefetch;

	bool skipNode(uint32_t node);

private:
	bool add(uint32_t node);                //returns true if added
	void addChildren(uint32_t node);
	void blockChildren(uint32_t node);

};
/*
class MetricTraversal: public Traversal {
public:
	FrustumMetric metric;
	float target_error;

	MetricTraversal();
	virtual ~MetricTraversal();
	virtual float nodeError(uint32_t node, bool &visible);
	virtual Action expand(HeapNode h);

	void setTargetError(float error) { target_error = error; }
	void getView(const float *proj = NULL, const float *modelview = NULL, const int *viewport = NULL) {
		metric.GetView(proj, modelview, viewport);
	}
};*/

}//namespace
#endif // NX_TRAVERSAL_H
